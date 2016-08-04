#include "ProfilerApp.h"
#include "MemoryApp.h"
#include "ProfilerAtomicHelpers.h"

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <stdexcept>
#include <vector>
#include <map>


#ifdef USE_MPI
    #include <mpi.h>
#endif

inline void ERROR_MSG( const std::string& msg ) {
    throw std::logic_error(msg);
}


#define VEC_SIZE_MIN 256
#define pout    std::cout
#define perr    std::cerr
#define printp  printf


// Include system dependent headers and define some functions
#ifdef USE_WINDOWS
    #include <windows.h>
    #include <stdio.h>   
    #include <tchar.h>
    #include <Psapi.h>
    #define get_frequency( f ) QueryPerformanceFrequency(&f)
    #define get_time( t ) QueryPerformanceCounter(&t)
    #define get_diff(start,end,f) \
        static_cast<double>(end.QuadPart-start.QuadPart)/static_cast<double>(f.QuadPart)
    #define get_diff_ns(start,end,f) \
        (0x3B9ACA00*static_cast<int64_t>(end.QuadPart-start.QuadPart))/static_cast<int64_t>(f.QuadPart)
#elif defined(USE_LINUX)
    #include <signal.h>
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <malloc.h>
    #define get_frequency( f ) do { f.tv_sec=0; f.tv_usec=0; } while(0)
    #define get_time( t ) gettimeofday(&t,NULL)
    #define get_diff(start,end,f) 1e-6*static_cast<double>( \
        0xF4240*(static_cast<int64_t>(end.tv_sec)-static_cast<int64_t>(start.tv_sec)) + \
                (static_cast<int64_t>(end.tv_usec)-static_cast<int64_t>(start.tv_usec)) )
    #define get_diff_ns(start,end,f) 0x3E8*( \
        0xF4240*(static_cast<int64_t>(end.tv_sec)-static_cast<int64_t>(start.tv_sec)) + \
                (static_cast<int64_t>(end.tv_usec)-static_cast<int64_t>(start.tv_usec)) )
#elif defined(USE_MAC)
    #include <signal.h>
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <mach/mach.h>
    #include <libkern/OSAtomic.h>
    #include <sys/time.h>
    #include <execinfo.h>
    #include <sys/time.h>
    #include <cxxabi.h>
    #include <stdint.h>
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
    #define get_frequency( f ) do { f.tv_sec=0; f.tv_usec=0; } while(0)
    #define get_time( t ) gettimeofday(&t,NULL)
    #define get_diff(start,end,f) 1e-6*static_cast<double>( \
        0xF4240*(static_cast<int64_t>(end.tv_sec)-static_cast<int64_t>(start.tv_sec)) + \
                (static_cast<int64_t>(end.tv_usec)-static_cast<int64_t>(start.tv_usec)) )
    #define get_diff_ns(start,end,f) 0x3E8*( \
        0xF4240*(static_cast<int64_t>(end.tv_sec)-static_cast<int64_t>(start.tv_sec)) + \
                (static_cast<int64_t>(end.tv_usec)-static_cast<int64_t>(start.tv_usec)) )
#else
    #error Unknown OS
#endif


// Check the limits of the define variables
#if MAX_TRACE_MEMORY > 0xFFFFFFFF
    #error MAX_TRACE_MEMORY must be < 2^32
#endif


#define ASSERT(EXP) do {                                            \
    if ( !(EXP) ) {                                                 \
        std::stringstream stream;                                   \
        stream << "Failed assertion: " << #EXP                      \
            << " " << __FILE__ << " " << __LINE__ << std::endl;     \
        ERROR_MSG(stream.str());                                    \
    }                                                               \
}while(false)


#define INSIST(EXP,MSG) do {                                        \
    if ( !(EXP) ) {                                                 \
        std::stringstream stream;                                   \
        stream << "Failed assertion: " << #EXP                      \
            << " " << __FILE__ << " " << __LINE__ << std::endl;     \
        stream << "Message: " << #MSG << std::endl;                 \
        ERROR_MSG(stream.str());                                    \
    }                                                               \
}while(false)


/******************************************************************
* Special Notes                                                   *
* Note 1: When using std::string in parallel, prior to C++11      *
*    std::string had a reference count.  This makes the copy      *
*    constructor and assignment operator unsafe when copying from *
*    a common object.  For example the following pseudo code is   *
*    not thread-safe because the reference counter in 'a' is      *
*    modified and not thread safe for all enviornments:           *
*       const std::string a;                                      *
*       for all threads                                           *
*           std::string b = a;      // Not thread safe            *
******************************************************************/


/******************************************************************
* Define global variables                                         *
******************************************************************/
#ifdef TIMER_ENABLE_THREAD_LOCAL
    thread_local ProfilerApp::RecursiveFunctionMap ProfilerApp::d_level_map;
#endif
ProfilerApp global_profiler;


/******************************************************************
* Define some helper functions                                    *
******************************************************************/
template<class TYPE>
inline TYPE* getPtr( std::vector<TYPE>& x ) { return x.empty() ? NULL:&x[0]; }
template<class TYPE>
inline const TYPE* getPtr( const std::vector<TYPE>& x ) { return x.empty() ? NULL:&x[0]; }


// Declare quicksort
template <class type_a, class type_b>
static inline void quicksort2(int n, type_a *arr, type_b *brr);

template <class type_a, class type_b>
static inline void mergeArrays( size_t N_list, 
    size_t *N, type_a **arr, type_b **brr,
    size_t *N_result, type_a **arr_result, type_b **brr_result );


// Inline function to get the current time/date string (without the newline character)
static inline std::string getDateString() 
{
    time_t rawtime;
    time ( &rawtime );
    std::string tmp(ctime(&rawtime));
    return tmp.substr(0,tmp.length()-1);
}


// Helper function to check (and if necessary resize) an array
template<class type1, class type2> 
void check_allocate_arrays( size_t* N_allocated, size_t N_current, size_t N_max, 
    type1** data1, type2** data2, TimerUtility::atomic::int64_atomic volatile *bytes_used )
{
    int64_t size_old = *N_allocated;
    int64_t size_new = std::max<size_t>(size_old,VEC_SIZE_MIN);
    while ( size_new <= (int64_t) N_current )
        size_new *= 2;
    // Stop allocating memory if we reached the limit
    if ( size_new > (int64_t) N_max ) 
        size_new = N_max;
    if ( size_old != size_new ) {
        // Expand the vector
        type1* data1_new = new type1[size_new];
        type2* data2_new = new type2[size_new];
        ASSERT(data1_new!=NULL&&data2_new!=NULL);
        TimerUtility::atomic::int64_atomic size = (size_new-size_old)*(sizeof(type1)+sizeof(type2));
        TimerUtility::atomic::atomic_add(bytes_used,size);
        memset(data1_new,0,size_new*sizeof(type1));
        memset(data2_new,0,size_new*sizeof(type2));
        if ( size_old != 0 ) {
            size_t N_copy = std::min<size_t>(size_old,size_new);
            memcpy(data1_new,*data1,N_copy*sizeof(type1));
            memcpy(data2_new,*data2,N_copy*sizeof(type2));
            delete [] *data1;
            delete [] *data2;
        }
        *data1 = data1_new;
        *data2 = data2_new;
        *N_allocated = size_new;
    }
}


/******************************************************************
* Some inline functions to acquire/release a mutex                *
******************************************************************/
#ifdef USE_WINDOWS
    static inline bool GET_LOCK(const HANDLE *lock) {
        int retval = WaitForSingleObject(*lock,INFINITE);
        if ( retval != WAIT_OBJECT_0 ) {
            perr << "Error locking mutex\n";
            return true;
        }
	    return false;
    }
    static inline bool RELEASE_LOCK(const HANDLE *lock) {
        int retval = ReleaseMutex(*lock);
        if ( retval == 0 ) {
            perr << "Error unlocking mutex\n";
            return true;
        }
    	return false;
    }
#elif defined(USE_LINUX) || defined(USE_MAC)
    static inline bool GET_LOCK(const pthread_mutex_t *lock) {
        int retval = pthread_mutex_lock(const_cast<pthread_mutex_t*>(lock));
        if ( retval == -1 ) {
            perr << "Error locking mutex\n";
            return true;
        }
	    return false;
    }
    static inline bool RELEASE_LOCK(const pthread_mutex_t *lock) {
        int retval = pthread_mutex_unlock(const_cast<pthread_mutex_t*>(lock));
        if ( retval == -1 ) {
            perr << "Error unlocking mutex\n";
            return true;
        }
	    return false;
    }
#else
    #error Unknown OS
#endif




/******************************************************************
* Some inline functions to get the rank and comm size             *
* Note: we want these functions to be safe to use, even if MPI    *
*    has not been initialized.                                    *
******************************************************************/
static inline int comm_size() 
{
    int size=1;
    #ifdef USE_MPI
        int flag=0;
        MPI_Initialized(&flag);
        if ( flag )
            MPI_Comm_size(MPI_COMM_WORLD,&size);
        ASSERT(size>0);
    #endif
    return size;
}
static inline int comm_rank() 
{
    int rank=0;
    #ifdef USE_MPI
        int flag=0;
        MPI_Initialized(&flag);
        if ( flag ) {
            int size=0;
            MPI_Comm_size(MPI_COMM_WORLD,&size);
            MPI_Comm_rank(MPI_COMM_WORLD,&rank);
            ASSERT(size>0);
            ASSERT(rank<size&&rank>=0);
        }
    #endif
    return rank;
}
static inline void comm_barrier() 
{
    #ifdef USE_MPI
        if ( comm_size() > 1 )
            MPI_Barrier(MPI_COMM_WORLD);
    #endif
}
static inline double comm_max_reduce( const double val )
{
    double result = val;
    #ifdef USE_MPI
        if ( comm_size() > 1 )
            MPI_Allreduce((double*)&val,&result,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
    #endif
    return result;
}
#ifdef USE_MPI
template<class TYPE> inline MPI_Datatype getType();
template<> inline MPI_Datatype getType<char>() { return MPI_CHAR; }
template<> inline MPI_Datatype getType<unsigned char>() { return MPI_UNSIGNED_CHAR; }
template<> inline MPI_Datatype getType<int32_t>() { return MPI_INT; }
template<> inline MPI_Datatype getType<uint32_t>() { return MPI_UNSIGNED; }
template<> inline MPI_Datatype getType<int64_t>() {
    if ( sizeof(uint64_t)==sizeof(long) )
        return MPI_LONG;
    if ( sizeof(uint64_t)==sizeof(long long) )
        return MPI_LONG_LONG;
    ERROR_MSG("Invalid MPI type");
    return MPI_BYTE;
}
template<> inline MPI_Datatype getType<uint64_t>() {
    if ( sizeof(uint64_t)==sizeof(unsigned long) )
        return MPI_UNSIGNED_LONG;
    if ( sizeof(uint64_t)==sizeof(long long) )
        return MPI_UNSIGNED_LONG_LONG;
    ERROR_MSG("Invalid MPI type");
    return MPI_BYTE;
}
template<> inline MPI_Datatype getType<double>() { return MPI_DOUBLE; }
#endif
template<class TYPE>
static inline void comm_send1( const TYPE *buf, size_t size, int dest, int tag )
{
    #ifdef USE_MPI
        INSIST(size<0x80000000,"We do not support sending/recieving buffers > 2^31 (yet)");
        int err = MPI_Send( buf, (int) size, getType<TYPE>(), dest, tag, MPI_COMM_WORLD );
        ASSERT(err==MPI_SUCCESS);
    #endif
}
template<class TYPE>
static inline TYPE* comm_recv1( int source, int tag )
{
    #ifdef USE_MPI
        MPI_Status status;
        int err = MPI_Probe( source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        int count;
        MPI_Get_count( &status, getType<TYPE>(), &count );
        TYPE *buf = new TYPE[count];
        err = MPI_Recv( buf, count, getType<TYPE>(), source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        return buf;
    #else
        return NULL;
    #endif
}
template<class TYPE>
static inline void comm_send2( const std::vector<TYPE>& data, int dest, int tag )
{
    #ifdef USE_MPI
        int err = MPI_Send( getPtr(data), (int) data.size(), getType<TYPE>(), dest, tag, MPI_COMM_WORLD );
        ASSERT(err==MPI_SUCCESS);
    #endif
}
template<class TYPE>
static inline std::vector<TYPE> comm_recv2( int source, int tag )
{
    #ifdef USE_MPI
        MPI_Status status;
        int err = MPI_Probe( source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        int count;
        MPI_Get_count( &status, getType<TYPE>(), &count );
        std::vector<TYPE> data(count);
        err = MPI_Recv( getPtr(data), count, getType<TYPE>(), source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        return data;
    #else
        return std::vector<TYPE>();
    #endif
}


/***********************************************************************
* Inline functions to set or unset the ith bit of the bit array trace  *
***********************************************************************/
static inline void set_trace_bit( unsigned int i, unsigned int N, TRACE_TYPE *trace ) {
    unsigned int j = i/TRACE_TYPE_size;
    unsigned int k = i%TRACE_TYPE_size;
    size_t mask = ((size_t)0x1)<<k;
    if ( i < N*TRACE_TYPE_size )
        trace[j] |= mask;
}
static inline void unset_trace_bit( unsigned int i, unsigned int N, TRACE_TYPE *trace ) {
    unsigned int j = i/TRACE_TYPE_size;
    unsigned int k = i%TRACE_TYPE_size;
    size_t mask = ((size_t)0x1)<<k;
    if ( i < N*TRACE_TYPE_size )
        trace[j] &= ~mask;
}


/***********************************************************************
* Inline function to convert the timer id to a string                  *
***********************************************************************/
#define N_BITS_ID 24    // The probability of a collision is ~N^2/2^N_bits (N is the number of timers)
static inline id_struct convert_timer_id( size_t key ) 
{
    int N_bits = std::min<int>(N_BITS_ID,8*sizeof(unsigned int));
    // Get a new key that is representable by N bits
    size_t id2 = key;
    if ( N_BITS_ID < 8*sizeof(size_t) ) {
        if ( sizeof(size_t)==4 )
            id2 = (key*0x9E3779B9) >> (32-N_BITS_ID);
        else if ( sizeof(size_t)==8 )
            id2 = (key*0x9E3779B97F4A7C15) >> (64-N_BITS_ID);
        else
            ERROR_MSG("Unhandled case");
    }
    // Convert the new key to a string
    char id[20]={0};
    if ( N_bits <= 9 ) {
        // The id is < 512, store it as a 3-digit number        
        sprintf(id,"%03u",static_cast<unsigned int>(id2));
    } else if ( N_bits <= 16 ) {
        // The id is < 2^16, store it as a 4-digit hex
        sprintf(id,"%04x",static_cast<unsigned int>(id2));
    } else {
        // We will store the id use the 64 character set { 0-9 a-z A-Z & $ }
        int N = std::max<int>(4,(N_bits+5)/6);    // The number of digits we need to use
        size_t tmp1 = id2;
        for (int i=N-1; i>=0; i--) {
            unsigned char tmp2 = tmp1%64;
            tmp1 /= 64;
            if ( tmp2 < 10 )
                id[i] = tmp2+48;
            else if ( tmp2 < 36 )
                id[i] = tmp2+(97-10);
            else if ( tmp2 < 62 )
                id[i] = tmp2+(65-36);
            else if ( tmp2 < 63 )
                id[i] = '&';
            else if ( tmp2 < 64 )
                id[i] = '$';
            else
                id[i] = 0;   // We should never use this character
        }
        id[N] = 0;
    }
    return id_struct(id);
}
id_struct id_struct::create_id( uint64_t id )
{
    return convert_timer_id(id);
}


/***********************************************************************
* TraceResults                                                         *
***********************************************************************/
TraceResults::TraceResults( ):
    N_active(0), thread(0), rank(0), N_trace(0), 
    min(1e38f), max(0.0f), tot(0.0f), N(0), mem(NULL)
{
    STATIC_ASSERT(sizeof(id_struct)==sizeof(double));
}
TraceResults::~TraceResults( )
{
    delete [] mem;
    mem = NULL;
}
TraceResults::TraceResults(const TraceResults& rhs):
    id(rhs.id), N_active(rhs.N_active), thread(rhs.thread), 
    rank(rhs.rank), N_trace(rhs.N_trace), min(rhs.min), 
    max(rhs.max), tot(rhs.tot), N(rhs.N), mem(NULL)
{
    allocate();
    if ( mem!=NULL )
        memcpy(mem,rhs.mem,(N_active+2*N_trace)*sizeof(double));
}
TraceResults& TraceResults::operator=(const TraceResults& rhs)
{
    if ( this == &rhs )
        return *this;
    this->id = rhs.id;
    this->thread = rhs.thread;
    this->rank = rhs.rank;
    this->N_active = rhs.N_active;
    this->N_trace = rhs.N_trace;
    this->N = rhs.N;
    this->min = rhs.min;
    this->max = rhs.max;
    this->tot = rhs.tot;
    allocate();
    if ( mem!=NULL )
        memcpy(this->mem,rhs.mem,(N_active+2*N_trace)*sizeof(double));
    return *this;
}
void TraceResults::allocate( )
{
    delete [] mem;
    mem = NULL;
    if ( N_active>0 || N_trace>0 ) {
        mem = new double[N_active+2*N_trace];
        memset(mem,0,(N_active+2*N_trace)*sizeof(double));
    }
}
id_struct* TraceResults::active( )
{
    return N_active>0 ? reinterpret_cast<id_struct*>(mem):NULL;
}
const id_struct* TraceResults::active( ) const
{
    return N_active>0 ? reinterpret_cast<const id_struct*>(mem):NULL;
}
double* TraceResults::start( )
{
    return N_trace>0 ? &mem[N_active]:NULL;
}
const double* TraceResults::start( ) const
{
    return N_trace>0 ? &mem[N_active]:NULL;
}
double* TraceResults::stop( )
{
    return N_trace>0 ? &mem[N_active+N_trace]:NULL;
}
const double* TraceResults::stop( ) const
{
    return N_trace>0 ? &mem[N_active+N_trace]:NULL;
}
size_t TraceResults::size( bool store_trace ) const
{
    size_t bytes = sizeof(TraceResults);
    bytes += N_active*sizeof(id_struct);
    if ( store_trace )
        bytes += 2*N_trace*sizeof(double);
    return bytes;
}
size_t TraceResults::pack( char* data, bool store_trace ) const
{
    TraceResults *this2 = const_cast<TraceResults*>(this);
    int N_trace2 = N_trace;
    if ( !store_trace ) { this2->N_trace = 0; }
    memcpy(data,this,sizeof(TraceResults));
    this2->N_trace = N_trace2;
    size_t pos = sizeof(TraceResults);
    if ( N_active > 0 ) {
        memcpy(&data[pos],active(),N_active*sizeof(id_struct));
        pos += N_active*sizeof(id_struct);
    }
    if ( N_trace>0 && store_trace ) {
        memcpy(&data[pos],start(),N_trace*sizeof(double));
        pos += N_trace*sizeof(double);
        memcpy(&data[pos],stop(),N_trace*sizeof(double));
        pos += N_trace*sizeof(double);
    }
    return pos;
}
size_t TraceResults::unpack( const char* data )
{
    delete [] mem;
    memcpy(this,data,sizeof(TraceResults));
    mem = NULL;
    allocate();
    size_t pos = sizeof(TraceResults);
    if ( N_active > 0 ) {
        memcpy(active(),&data[pos],N_active*sizeof(id_struct));
        pos += N_active*sizeof(id_struct);
    }
    if ( N_trace > 0 ) {
        memcpy(start(),&data[pos],N_trace*sizeof(double));
        pos += N_trace*sizeof(double);
        memcpy(stop(),&data[pos],N_trace*sizeof(double));
        pos += N_trace*sizeof(double);
    }
    return pos;

}
bool TraceResults::operator==(const TraceResults& rhs) const
{
    bool equal = id==rhs.id;
    equal = equal && N_active==rhs.N_active;
    equal = equal && thread==rhs.thread;
    equal = equal && rank==rhs.rank;
    equal = equal && N_trace==rhs.N_trace;
    equal = equal && min==rhs.min;
    equal = equal && max==rhs.max;
    equal = equal && tot==rhs.tot;
    equal = equal && N==rhs.N;
    const id_struct* a1 = active();
    const id_struct* a2 = rhs.active();
    for (size_t i=0; i<N_active; i++)
        equal = equal && a1[i]==a2[i];
    return equal;
}


/***********************************************************************
* TimerResults                                                         *
***********************************************************************/
size_t TimerResults::size( bool store_trace ) const
{
    size_t bytes = sizeof(id);
    bytes += 6*sizeof(int);
    bytes += message.size();
    bytes += file.size();
    bytes += path.size();
    for (size_t i=0; i<trace.size(); i++)
        bytes += trace[i].size(store_trace);
    return bytes;
}
size_t TimerResults::pack( char* data, bool store_trace ) const
{
    memcpy(data,&id,sizeof(id));
    size_t pos = sizeof(id);
    const int tmp[6] = { (int)message.size(), (int)file.size(), (int)path.size(), start, stop, (int)trace.size() };
    memcpy(&data[pos],&tmp,sizeof(tmp));
    pos += sizeof(tmp);
    memcpy(&data[pos],message.c_str(),tmp[0]);  pos+=message.size();
    memcpy(&data[pos],file.c_str(),tmp[1]);     pos+=file.size();
    memcpy(&data[pos],path.c_str(),tmp[2]);     pos+=path.size();
    for (size_t i=0; i<trace.size(); i++)
        pos += trace[i].pack(&data[pos],store_trace);
    return pos;
}
size_t TimerResults::unpack( const char* data )
{
    memcpy(&id,data,sizeof(id));
    size_t pos = sizeof(id);
    int tmp[6];
    memcpy(&tmp,&data[pos],sizeof(tmp));
    pos += sizeof(tmp);
    message=std::string(&data[pos],tmp[0]);     pos+=message.size();
    file=std::string(&data[pos],tmp[1]);        pos+=file.size();
    path = std::string(&data[pos],tmp[2]);      pos+=path.size();
    start = tmp[3];
    stop  = tmp[4];
    trace.resize(tmp[5]);
    for (size_t i=0; i<trace.size(); i++)
        pos += trace[i].unpack(&data[pos]);
    return pos;
}
bool TimerResults::operator==(const TimerResults& rhs) const
{
    bool equal = id==rhs.id;
    equal = equal && message==rhs.message;
    equal = equal && file==rhs.file;
    equal = equal && path==rhs.path;
    equal = equal && start==rhs.start;
    equal = equal && stop==rhs.stop;
    for (size_t i=0; i<trace.size(); i++)
        equal = equal && trace[i]==rhs.trace[i];
    return equal;
}


/***********************************************************************
* MemoryResults                                                        *
***********************************************************************/
size_t MemoryResults::size( ) const
{
    size_t N_bytes = 2*sizeof(size_t);
    N_bytes += time.size()*sizeof(double);
    N_bytes += bytes.size()*sizeof(size_t);
    return N_bytes;
}
size_t MemoryResults::pack( char* data ) const
{
    const size_t tmp[2] = { (size_t)rank, time.size() };
    memcpy(data,&tmp,sizeof(tmp));
    size_t pos = sizeof(tmp);
    if ( !time.empty() ) {
        memcpy(&data[pos],&time[0],tmp[1]*sizeof(double));
        pos += time.size()*sizeof(double);
        memcpy(&data[pos],&bytes[0],tmp[1]*sizeof(size_t));
        pos += time.size()*sizeof(double);
    }
    return pos;
}
size_t MemoryResults::unpack( const char* data )
{
    size_t tmp[2];
    memcpy(&tmp,data,sizeof(tmp));
    rank = static_cast<int>(tmp[0]);
    size_t N = tmp[1];
    time.resize(N);
    bytes.resize(N);
    size_t pos = sizeof(tmp);
    if ( !time.empty() ) {
        memcpy(&time[0],&data[pos],tmp[1]*sizeof(double));
        pos += time.size()*sizeof(double);
        memcpy(&bytes[0],&data[pos],tmp[1]*sizeof(size_t));
        pos += time.size()*sizeof(double);
    }
    return pos;
}
bool MemoryResults::operator==(const MemoryResults& rhs) const
{
    bool equal = rank==rhs.rank;
    equal = equal && time.size()==rhs.time.size();
    equal = equal && bytes.size()==rhs.bytes.size();
    if ( !equal )
        return false;
    for (size_t i=0; i<time.size(); i++)
        equal = equal && time[i]==rhs.time[i];
    for (size_t i=0; i<bytes.size(); i++)
        equal = equal && bytes[i]==rhs.bytes[i];
    return equal;
}


/***********************************************************************
* TimerMemoryResults                                                   *
***********************************************************************/
size_t TimerMemoryResults::size() const
{
    size_t bytes = 3*sizeof(int);
    for (size_t i=0; i<timers.size(); i++)
        bytes += timers[i].size();
    for (size_t i=0; i<memory.size(); i++)
        bytes += memory[i].size();
    return bytes;
}
size_t TimerMemoryResults::pack( char* data ) const
{
    const int tmp[3] = { N_procs, (int)timers.size(), (int)memory.size() };
    memcpy(data,&tmp,sizeof(tmp));
    size_t pos = sizeof(tmp);
    for (size_t i=0; i<timers.size(); i++)
        pos += timers[i].pack(&data[pos]);
    for (size_t i=0; i<memory.size(); i++)
        pos += memory[i].pack(&data[pos]);
    return pos;
}
size_t TimerMemoryResults::unpack( const char* data )
{
    int tmp[3];
    memcpy(&tmp,data,sizeof(tmp));
    size_t pos = sizeof(tmp);
    N_procs = tmp[0];
    timers.resize(tmp[1]);
    memory.resize(tmp[2]);
    for (size_t i=0; i<timers.size(); i++)
        pos += timers[i].unpack(&data[pos]);
    for (size_t i=0; i<memory.size(); i++)
        pos += memory[i].unpack(&data[pos]);
    return pos;
}
bool TimerMemoryResults::operator==(const TimerMemoryResults& rhs) const
{
    bool equal = N_procs==rhs.N_procs;
    equal = equal && timers.size()==rhs.timers.size();
    equal = equal && memory.size()==rhs.memory.size();
    if ( !equal )
        return false;
    for (size_t i=0; i<timers.size(); i++)
        equal = equal && timers[i]==rhs.timers[i];
    for (size_t i=0; i<memory.size(); i++)
        equal = equal && memory[i]==rhs.memory[i];
    return equal;
}


/***********************************************************************
* Consructor                                                           *
***********************************************************************/
ProfilerApp::ProfilerApp()
{
    #ifdef TIMER_ENABLE_THREAD_LOCAL
        d_level_map = RecursiveFunctionMap();
    #endif
    get_time(d_construct_time);
    get_frequency(d_frequency);
    if ( sizeof(id_struct)!=8 )
        ERROR_MSG("id_struct is an unexpected size\n");
    #ifdef USE_WINDOWS
        lock = CreateMutex (NULL, FALSE, NULL);
    #elif defined(USE_LINUX) || defined(USE_MAC)
        pthread_mutex_init (&lock,NULL);
    #else
        #error Unknown OS
    #endif
    for (int i=0; i<MAX_THREADS; i++)
        thread_table[i] = NULL;
    for (int i=0; i<TIMER_HASH_SIZE; i++)
        timer_table[i] = NULL;
    N_timers = 0;
    d_level = 0;
    d_shift = 0.0;
    d_store_trace_data = false;
    d_store_memory_data = false;
    d_disable_timer_error = false;
    d_max_trace_remaining = static_cast<size_t>(MAX_TRACE_MEMORY);
    d_N_memory_steps = 0;
    d_time_memory = NULL;
    d_size_memory = NULL;
    d_bytes = sizeof(ProfilerApp);
}
void ProfilerApp::set_store_trace( bool profile ) 
{ 
    if ( N_timers==0 ) 
        d_store_trace_data = profile;
    else
        ERROR_MSG("Cannot change trace status after a timer is started\n");
}
void ProfilerApp::set_store_memory( bool memory ) 
{ 
    if ( N_timers==0 ) 
        d_store_memory_data = memory;
    else
        ERROR_MSG("Cannot change memory status after a timer is started\n");
}


/***********************************************************************
* Deconsructor                                                         *
***********************************************************************/
ProfilerApp::~ProfilerApp() 
{
    // Disable and delete the timers
    disable();
}


/***********************************************************************
* Function to syncronize the timers                                    *
***********************************************************************/
void ProfilerApp::synchronize() 
{
    GET_LOCK(&lock);
	comm_barrier();
    TIME_TYPE sync_time_local;
    get_time(sync_time_local);
    double current_time = get_diff(d_construct_time,sync_time_local,d_frequency);
    double max_current_time = comm_max_reduce(current_time);
    d_shift = max_current_time - current_time;
    RELEASE_LOCK(&lock);
}


/***********************************************************************
* Function to start profiling a block of code                          *
***********************************************************************/
void ProfilerApp::start( const char* message, const char* filename, 
    int line, int level, uint64_t timer_id ) 
{
    if ( level<0 || level>=128 )
        ERROR_MSG("level must be in the range 0-127");
    if ( this->d_level<level )
        return;
    // Get the thread data
    thread_info* thread_data = get_thread_data();
    // Get the appropriate timer
    if ( timer_id == 0 )
        timer_id = get_timer_id(message,filename);
    store_timer* timer = get_block(thread_data,timer_id,true,message,filename,line,-1);
    if ( timer->is_active ) {
        if ( d_disable_timer_error ) {
            // Stop the timer before starting
            this->stop( message, filename, -1, level, timer_id );
        } else {
            // Throw an error
            std::stringstream msg;
            msg << "Timer is already active, did you forget to call stop? (" << 
                message << " in " << filename << " at line " << line << ")\n";
            ERROR_MSG(msg.str());
        }
    }
    // Get the memory usage
    if ( d_store_memory_data && thread_data->N_memory_steps<d_max_trace_remaining ) {
        size_t* N_alloc = &thread_data->N_memory_alloc;
        size_t N_size = thread_data->N_memory_steps;
        size_t N_max = d_max_trace_remaining;
        // Check the memory allocation
        check_allocate_arrays(N_alloc,N_size,N_max,&thread_data->time_memory,&thread_data->size_memory,&d_bytes);
        // Get the current memroy usage
        thread_data->time_memory[N_size] = 0;
        thread_data->size_memory[N_size] = MemoryApp::getTotalMemoryUsage()-d_bytes;
    }
    // Start the timer 
    memcpy(timer->trace,thread_data->active,TRACE_SIZE*sizeof(TRACE_TYPE));
    timer->is_active = true;
    timer->N_calls++;
    set_trace_bit(timer->trace_index,TRACE_SIZE,thread_data->active);
    get_time(timer->start_time);
    // Record the time of the memory usage
    if ( d_store_memory_data && thread_data->N_memory_steps<d_max_trace_remaining ) {
        thread_data->time_memory[thread_data->N_memory_steps] = 
            get_diff_ns(d_construct_time,timer->start_time,d_frequency);
        thread_data->N_memory_steps++;
    }
}


/***********************************************************************
* Function to stop profiling a block of code                           *
***********************************************************************/
void ProfilerApp::stop( const char* message, const char* filename, 
    int line, int level, uint64_t timer_id ) 
{
    if ( level<0 || level>=128 )
        ERROR_MSG("level must be in the range 0-127");
    if ( this->d_level<level )
        return;
    // Use the current time (minimize the effects of the overhead of the timer)
    TIME_TYPE end_time;
    get_time(end_time);
    // Get the thread data
    thread_info* thread_data = get_thread_data();
    // Get the appropriate timer
    if ( timer_id == 0 )
        timer_id = get_timer_id(message,filename);
    store_timer* timer = get_block(thread_data,timer_id,true,message,filename,-1,line);
    if ( !timer->is_active ) {
        if ( d_disable_timer_error ) {
            // Start the timer before starting
            this->start( message, filename, -1, level, timer_id );
            get_time(end_time);  // Use the current time as the new stop
        } else {
            std::stringstream msg;
            msg << "Timer is not active, did you forget to call start? (" << 
                message << " in " << filename << " at line " << line << ")\n";
            ERROR_MSG(msg.str());
        }
    }
    timer->is_active = false;
    // Update the active trace log
    unset_trace_bit(timer->trace_index,TRACE_SIZE,thread_data->active );
    // The timer is only a calling timer if it was active before and after the current timer
    TRACE_TYPE active[TRACE_SIZE]={0};
    for (size_t i=0; i<TRACE_SIZE; i++)
        active[i] = thread_data->active[i] & timer->trace[i];
    uint64_t trace_id = get_trace_id( active );
    // Find the trace to save
    store_trace *trace = timer->trace_head;
    while ( trace != NULL) {
        if ( trace_id==trace->id )
            break;
        trace = trace->next;
    }
    if ( trace == NULL ) {
        trace = new store_trace;
        TimerUtility::atomic::int64_atomic size = sizeof(store_trace);
        TimerUtility::atomic::atomic_add(&d_bytes,size);
        memcpy(trace->trace,active,TRACE_SIZE*sizeof(TRACE_TYPE));
        trace->id = trace_id;
        if ( timer->trace_head == NULL ) {
            timer->trace_head = trace;
        } else {
            store_trace *trace_list = timer->trace_head;
            while ( trace_list->next != NULL)
                trace_list = trace_list->next;
            trace_list->next = trace;
        }
    }
    // Calculate the time elapsed since start was called
    double time = get_diff(timer->start_time,end_time,d_frequency);
    // Save the starting and ending time if we are storing the detailed traces
    if ( d_store_trace_data && trace->N_calls<MAX_TRACE_TRACE) {
        // Check if we need to allocate more memory to store the times
        size_t* N_alloc = &trace->N_trace_alloc;
        size_t N_size = trace->N_calls;
        size_t N_max = static_cast<size_t>(MAX_TRACE_TRACE);
        check_allocate_arrays(N_alloc,N_size,N_max,&trace->start_time,&trace->end_time,&d_bytes);
        // Calculate the time elapsed since the profiler was created
        trace->start_time[trace->N_calls] = get_diff(d_construct_time,timer->start_time,d_frequency);
        trace->end_time[trace->N_calls]   = get_diff(d_construct_time,end_time,d_frequency);
    }
    // Save the minimum, maximum, and total times
    timer->max_time = std::max<double>(timer->max_time,time);
    timer->min_time = std::min<double>(timer->min_time,time);
    timer->total_time += time;
    // Save the new time info to the trace
    trace->max_time = std::max<double>(trace->max_time,time);
    trace->min_time = std::min<double>(trace->min_time,time);
    trace->total_time += time;
    trace->N_calls++;
    // Get the memory usage
    if ( d_store_memory_data && thread_data->N_memory_steps<d_max_trace_remaining ) {
        size_t* N_alloc = &thread_data->N_memory_alloc;
        size_t N_size = thread_data->N_memory_steps;
        size_t N_max = d_max_trace_remaining;
        // Check the memory allocation
        check_allocate_arrays(N_alloc,N_size,N_max,&thread_data->time_memory,&thread_data->size_memory,&d_bytes);
        // Get the current memroy usage
        thread_data->time_memory[N_size] = get_diff_ns(d_construct_time,end_time,d_frequency);
        thread_data->size_memory[N_size] = MemoryApp::getTotalMemoryUsage()-d_bytes;
        thread_data->N_memory_steps++;
    }
}


/***********************************************************************
* Function to enable/disable the timers                                *
***********************************************************************/
void ProfilerApp::enable( int level )
{
    // This is a blocking function so it cannot be called at the same time as disable
    if ( level<0 || level>=128 )
        ERROR_MSG("level must be in the range 0-127");
    GET_LOCK(&lock);
    d_level = level;
    RELEASE_LOCK(&lock);
}
void ProfilerApp::disable( )
{
    // First, change the status flag
    GET_LOCK(&lock);
    d_level = -1;
    // delete the thread structures
    for (int i=0; i<MAX_THREADS; i++) {
        delete thread_table[i];
        thread_table[i] = NULL;
    }
    // delete the timer data structures
    for (int i=0; i<TIMER_HASH_SIZE; i++) {
        delete timer_table[i];
        timer_table[i] = NULL;
    }
    N_timers = 0;
    // delete the memory info
    d_max_trace_remaining = static_cast<size_t>(MAX_TRACE_MEMORY);
    d_N_memory_steps = 0;
    delete [] d_time_memory;
    delete [] d_size_memory;
    d_time_memory = NULL;
    d_size_memory = NULL;
    d_bytes = sizeof(ProfilerApp);
    RELEASE_LOCK(&lock);
    // Delete scoped variables
    #ifdef TIMER_ENABLE_THREAD_LOCAL
        d_level_map.clear();
    #endif
}


/***********************************************************************
* Function to return the profiling info                                *
***********************************************************************/
inline void ProfilerApp::getTimerResultsID( uint64_t id, std::vector<const thread_info*>& thread_list,
    int rank, const TIME_TYPE& end_time, TimerResults& results ) const
{
    const size_t key = GET_TIMER_HASH( id );
    results.id = convert_timer_id(id);
    // Search for the global timer info
    store_timer_data_info *timer_global = const_cast<store_timer_data_info*>(timer_table[key]);
    while ( timer_global!=NULL ) {
        if ( timer_global->id == id ) 
            break;
        timer_global = const_cast<store_timer_data_info*>(timer_global->next);
    }
    if ( timer_global==NULL )
        ERROR_MSG("Internal error");
    // Copy the basic timer info
    results.message = timer_global->message;
    results.file = timer_global->filename;
    results.path = timer_global->path;
    results.start = timer_global->start_line;
    results.stop = timer_global->stop_line;
    // Loop through the thread entries
    for (size_t j=0; j<thread_list.size(); j++) {
        const thread_info *thread_data = thread_list[j];
        int thread_id = thread_data->id;
        // Search for a timer that matches the current id
        store_timer* timer = thread_data->head[key];
        while ( timer != NULL ) {
            if ( timer->id == id )
                break;
            timer = timer->next;
        }
        if ( timer==NULL ) {
            // The current thread does not have a copy of this timer, move on
            continue;
        }
        if ( timer->N_calls==0 ) {
            // The timer was not called, move on
            continue;
        }
        // If the timer is still running, add the current processing time to the totals
        bool add_trace = false;
        double time = 0.0;
        size_t trace_id = 0;
        TRACE_TYPE active[TRACE_SIZE];
        if ( timer->is_active ) {
            add_trace = true;
            time = get_diff(timer->start_time,end_time,d_frequency);
            // The timer is only a calling timer if it was active before and after the current timer
            for (size_t i=0; i<TRACE_SIZE; i++)
                active[i] = thread_data->active[i] & timer->trace[i];
            unset_trace_bit(timer->trace_index,TRACE_SIZE,active);
            trace_id = get_trace_id( active );
        }
        // Loop through the trace entries
        store_trace *trace = timer->trace_head;
        while ( trace != NULL ) {
            size_t k = results.trace.size();
            results.trace.resize(k+1);
            // Get the running times of the trace
            size_t N_stored_trace = 0;
            if ( d_store_trace_data ) 
                N_stored_trace = std::min<size_t>(trace->N_calls,static_cast<size_t>(MAX_TRACE_TRACE));
            std::vector<id_struct> list = get_active_list( trace->trace, timer->trace_index, thread_data );
            results.trace[k].id = results.id;
            results.trace[k].thread = thread_id;
            results.trace[k].rank = rank;
            results.trace[k].N = trace->N_calls;
            results.trace[k].N_active = static_cast<unsigned short>(list.size());
            results.trace[k].N_trace = static_cast<unsigned int>(N_stored_trace);
            results.trace[k].min = static_cast<float>(trace->min_time);
            results.trace[k].max = static_cast<float>(trace->max_time);
            results.trace[k].tot = static_cast<float>(trace->total_time);
            results.trace[k].allocate();
            for (size_t j=0; j<list.size(); j++)
                results.trace[k].active()[j] = list[j];
            // Determine if we need to add the running trace
            if ( add_trace ) {
                if ( trace_id == trace->id ) {
                    results.trace[k].min = std::min<float>(results.trace[k].min,time);
                    results.trace[k].max = std::max<float>(results.trace[k].max,time);
                    results.trace[k].tot += static_cast<float>(time);
                    add_trace = false;
                }
            }
            // Save the detailed trace results (this is a binary file)
            double *start = results.trace[k].start();
            double *stop  = results.trace[k].stop();
            for (size_t m=0; m<N_stored_trace; m++) {
                start[m] = trace->start_time[m] + d_shift;
                stop[m]  = trace->end_time[m]   + d_shift;
            }
            // Advance to the next trace
            trace = trace->next;
        }
        // Create a new trace if necessary
        if ( add_trace ) { 
            size_t k = results.trace.size();
            results.trace.resize(k+1);
            size_t N_stored_trace = 0;
            if ( d_store_trace_data ) 
                N_stored_trace = 1;
            std::vector<id_struct> list = get_active_list( active, timer->trace_index, thread_data );
            results.trace[k].id = results.id;
            results.trace[k].thread = thread_id;
            results.trace[k].rank = rank;
            results.trace[k].N = 1;
            results.trace[k].N_active = static_cast<unsigned short>(list.size());
            results.trace[k].N_trace = static_cast<unsigned int>(N_stored_trace);
            results.trace[k].min = static_cast<float>(time);
            results.trace[k].max = static_cast<float>(time);
            results.trace[k].tot = static_cast<float>(time);
            results.trace[k].allocate();
            for (size_t j=0; j<list.size(); j++)
                results.trace[k].active()[j] = list[j];
            if ( d_store_trace_data ) { 
                double *start = results.trace[k].start();
                double *stop  = results.trace[k].stop();
                start[0] = get_diff(d_construct_time,timer->start_time,d_frequency) + d_shift;
                stop[0]  = get_diff(d_construct_time,end_time,d_frequency) + d_shift;
            }
        }
    }
}
std::vector<TimerResults> ProfilerApp::getTimerResults() const
{
    // Get the current time in case we need to "stop" and timers
    TIME_TYPE end_time;
    get_time(end_time);
    int rank = comm_rank();
    // Get a lock
    GET_LOCK(&lock);
    // Get the thread data
    // This can safely be done lock-free
    std::vector<const thread_info*> thread_list;
    for (int i=0; i<MAX_THREADS; i++) {
        if ( thread_table[i] != NULL )
            thread_list.push_back(thread_table[i]);
    }
    // Get a list of all timer ids
    std::vector<size_t> ids;
    for (int i=0; i<TIMER_HASH_SIZE; i++) {
        store_timer_data_info *timer_global = const_cast<store_timer_data_info*>(timer_table[i]);
        while ( timer_global!=NULL ) {
            ids.push_back(timer_global->id);
            timer_global = const_cast<store_timer_data_info*>(timer_global->next);
        }
    }
    if ( (int)ids.size()!=N_timers )
        ERROR_MSG("Not all timers were found");
    // Begin storing the timers
    std::vector<TimerResults> results(ids.size());
    for (size_t i=0; i<ids.size(); i++)
        getTimerResultsID( ids[i], thread_list, rank, end_time, results[i] );
    // Release the mutex
    RELEASE_LOCK(&lock);
    return results;
}
TimerResults ProfilerApp::getTimerResults( uint64_t id ) const
{
    // Get the current time in case we need to "stop" and timers
    TIME_TYPE end_time;
    get_time(end_time);
    int rank = comm_rank();
    // Get a lock
    GET_LOCK(&lock);
    // Get the thread data
    // This can safely be done lock-free
    std::vector<const thread_info*> thread_list;
    for (int i=0; i<MAX_THREADS; i++) {
        if ( thread_table[i] != NULL )
            thread_list.push_back(thread_table[i]);
    }
    // Get the timer
    TimerResults results;
    getTimerResultsID( id, thread_list, rank, end_time, results );
    // Release the mutex
    RELEASE_LOCK(&lock);
    return results;
}


/***********************************************************************
* Function to return the memory usage as a function of time            *
***********************************************************************/
MemoryResults ProfilerApp::getMemoryResults() const
{
    MemoryResults data;
    data.rank = comm_rank();
    // Get the thread data
    // This can safely be done lock-free
    std::vector<const thread_info*> thread_list;
    for (int i=0; i<MAX_THREADS; i++) {
        if ( thread_table[i] != NULL )
            thread_list.push_back(thread_table[i]);
    }
    // First unify the memory info from the different threads
    // Technically there should be a lock, but we do not use locks on individual threads
    TimerUtility::atomic::int64_atomic bytes_changed = 0;
    std::vector<size_t> N_time;
    std::vector<size_t*> data_time;
    std::vector<size_t*> size_time;
    for (size_t i=0; i<thread_list.size(); i++) {
        volatile thread_info *thread_data = const_cast<volatile thread_info*>(thread_list[i]);
        // Copy the pointers so that we minimize the chance of the data being modified
        size_t N_steps = thread_data->N_memory_steps;
        size_t N_alloc = thread_data->N_memory_alloc;
        size_t* time = thread_data->time_memory;
        size_t* size = thread_data->size_memory;
        thread_data->N_memory_steps = 0;
        thread_data->N_memory_alloc = 0;
        thread_data->time_memory = NULL;
        thread_data->size_memory = NULL;
        bytes_changed -= N_alloc*(sizeof(double)+sizeof(size_t));
        if ( N_steps>0 ) { 
            N_time.push_back(N_steps);
            data_time.push_back(time);
            size_time.push_back(size);
        }
    }
    if ( d_N_memory_steps>0 ) {
        N_time.push_back(d_N_memory_steps);
        data_time.push_back(d_time_memory);
        size_time.push_back(d_size_memory);
    }
    size_t N_memory_steps_old = d_N_memory_steps;
    mergeArrays<size_t,size_t>( N_time.size(), getPtr(N_time), getPtr(data_time), 
        getPtr(size_time), &d_N_memory_steps, &d_time_memory, &d_size_memory );
    for (size_t i=0; i<data_time.size(); i++) {
        delete [] data_time[i];
        delete [] size_time[i];
    }
    // Compress the results by removing values that have not changed
    // Note: we will always keep the first and last values
    if ( d_N_memory_steps > 2 ) {
        size_t i = 1;
        for (size_t j=1; j<d_N_memory_steps-1; j++) {
            if ( d_size_memory[j]==d_size_memory[i-1] && d_size_memory[j]==d_size_memory[j+1] )
                continue;
            d_time_memory[i] = d_time_memory[j];
            d_size_memory[i] = d_size_memory[j];
            i++;
        }
        d_time_memory[i] = d_time_memory[d_N_memory_steps-1];
        d_size_memory[i] = d_size_memory[d_N_memory_steps-1];
        d_N_memory_steps = i+1;
    }
    d_max_trace_remaining = std::max<size_t>((size_t)MAX_TRACE_MEMORY-d_N_memory_steps,0);
    bytes_changed += (d_N_memory_steps-N_memory_steps_old)*(sizeof(double)+sizeof(size_t));
    TimerUtility::atomic::atomic_add(&d_bytes,bytes_changed);
    // Copy the results to the output vector
    data.time.resize(d_N_memory_steps);
    data.bytes.resize(d_N_memory_steps);
    for (size_t i=0; i<d_N_memory_steps; i++) {
        data.time[i]  = 1e-9*static_cast<double>(d_time_memory[i]);
        data.bytes[i] = d_size_memory[i];
    }
    return data;
}


/***********************************************************************
* Function to save the profiling info                                  *
***********************************************************************/
void ProfilerApp::save( const std::string& filename, bool global ) const
{
    if ( this->d_level<0 ) {
        pout << "Warning: Timers are not enabled, no data will be saved\n";
        return;
    }
    int N_procs = comm_size();
    int rank = comm_rank();
    // Set the filenames
    char filename_timer[1000], filename_trace[1000], filename_memory[1000];
    if ( !global ) {
        sprintf(filename_timer,"%s.%i.timer",filename.c_str(),rank+1);
        sprintf(filename_trace,"%s.%i.trace",filename.c_str(),rank+1);
        sprintf(filename_memory,"%s.%i.memory",filename.c_str(),rank+1);
    } else {
        sprintf(filename_timer,"%s.0.timer",filename.c_str());
        sprintf(filename_trace,"%s.0.trace",filename.c_str());
        sprintf(filename_memory,"%s.0.memory",filename.c_str());
    }
    // Get the current results
    std::vector<TimerResults> results = getTimerResults();
    if ( (int)results.size()!=N_timers )
        ERROR_MSG("Not all timers were found");
    if ( global ) {
        // Gather the timers from all files (rank 0 will do all writing)
        gather_timers( results );
    }
    if ( !results.empty() ) {
        // Get the timer ids and sort the ids by the total time 
        // to create a global order to save the results
        std::vector<size_t> id_order(results.size(),0);
        std::vector<double> total_time(results.size(),0);
        for (size_t i=0; i<results.size(); i++) {
            id_order[i] = i;
            total_time[i] = 0.0;
            int N_threads = TimerUtility::ProfilerThreadIndex::getNThreads();
            std::vector<double> time_thread(N_threads,0);
            for (size_t j=0; j<results[i].trace.size(); j++)
                time_thread[results[i].trace[j].thread] += results[i].trace[j].tot;
            for (int j=0; j<N_threads; j++)
                total_time[i] = std::max<double>(total_time[i],time_thread[j]);
        }
        quicksort2((int)results.size(),&total_time[0],&id_order[0]);
        // Open the file(s) for writing
        FILE *timerFile = fopen(filename_timer,"wb");
        if ( timerFile == NULL ) {
            perr << "Error opening file for writing (timer)";
            return;
        }
        FILE *traceFile = NULL;
        if ( d_store_trace_data ) {
            traceFile = fopen(filename_trace,"wb");
            if ( traceFile == NULL ) {
                perr << "Error opening file for writing (trace)";
                fclose(timerFile);
                return;
            }
        }
        // Create the file header
        fprintf(timerFile,"                  Message                      Filename        ");
        fprintf(timerFile,"  Thread  Start Line  Stop Line  N_calls  Min Time  Max Time  Total Time\n");
        fprintf(timerFile,"---------------------------------------------------------------");
        fprintf(timerFile,"------------------------------------------------------------------------\n");
        // Loop through the list of timers, storing the most expensive first
        for (int ii=static_cast<int>(results.size())-1; ii>=0; ii--) {
            size_t i = id_order[ii];
            int N_threads = TimerUtility::ProfilerThreadIndex::getNThreads();
            std::vector<int> N_thread(N_threads,0);
            std::vector<float> min_thread(N_threads,1e38f);
            std::vector<float> max_thread(N_threads,0.0f);
            std::vector<double> tot_thread(N_threads,0.0);
            for (size_t j=0; j<results[i].trace.size(); j++) {
                int k = results[i].trace[j].thread;
                N_thread[k] += static_cast<int>(results[i].trace[j].N);
                min_thread[k] = std::min<float>(min_thread[k],results[i].trace[j].min);
                max_thread[k] = std::max<float>(max_thread[k],results[i].trace[j].max);
                tot_thread[k] += results[i].trace[j].tot;
            }
            for (int j=0; j<N_threads; j++) {
                if ( N_thread[j]==0 )
                    continue;
                // Save the timer to the file
                // Note: we always want one space in front in case the timer starts
                //    with '<' and is long.
                fprintf(timerFile," %29s  %30s   %4i   %7i    %7i  %8i     %8.3f  %8.3f  %10.3f\n",
                    results[i].message.c_str(),results[i].file.c_str(),j,results[i].start,
                    results[i].stop,N_thread[j],min_thread[j],max_thread[j],tot_thread[j]);
            }
        }
        // Loop through all of the entries, saving the detailed data and the trace logs
        fprintf(timerFile,"\n\n\n");
        fprintf(timerFile,"<N_procs=%i,id=%i",N_procs,rank);
        fprintf(timerFile,",store_trace=%i",d_store_trace_data?1:0);
        fprintf(timerFile,",store_memory=%i",d_store_memory_data?1:0);
        fprintf(timerFile,",date='%s'>\n",getDateString().c_str());
        // Loop through the list of timers, storing the most expensive first
        for (int ii=static_cast<int>(results.size())-1; ii>=0; ii--) {
            size_t i=id_order[ii];
            // Store the basic timer info
            const char e = 0x0E;    // Escape character for printing strings
            fprintf(timerFile,"<timer:id=%s,message=%c%s%c,file=%c%s%c,path=%c%s%c,start=%i,stop=%i>\n",
                results[i].id.c_str(),e,results[i].message.c_str(),e,e,results[i].file.c_str(),e,
                e,results[i].path.c_str(),e,results[i].start,results[i].stop);
            // Store the trace data
            for (size_t j=0; j<results[i].trace.size(); j++) {
                const TraceResults& trace = results[i].trace[j];
                std::string active;
                for (size_t k=0; k<trace.N_active; k++)
                    active += trace.active()[k].string() + " ";
                fprintf(timerFile,"<trace:id=%s,thread=%u,rank=%u,N=%lu,min=%e,max=%e,tot=%e,active=[ %s]>\n",
                    trace.id.c_str(),trace.thread,trace.rank,static_cast<unsigned long>(trace.N),
                    trace.min,trace.max,trace.tot,active.c_str());
                // Save the detailed trace results (this is a binary file)
                if ( trace.N_trace > 0 ) { 
                    fprintf(traceFile,"<id=%s,thread=%u,rank=%u,active=[ %s],N=%lu>\n",
                        trace.id.c_str(),trace.thread,trace.rank,active.c_str(),
                        static_cast<unsigned long>(trace.N_trace));
                    fwrite(trace.start(),sizeof(double),trace.N_trace,traceFile);
                    fwrite(trace.stop(), sizeof(double),trace.N_trace,traceFile);
                    fprintf(traceFile,"\n");
                }
            }
        }
        // Close the file(s)
        fclose(timerFile);
        if ( traceFile!=NULL)
            fclose(traceFile);
    }
    results.clear();
    // Store the memory trace info
    if ( d_store_memory_data ) {
        FILE* memoryFile = fopen(filename_memory,"wb");
        if ( memoryFile == NULL ) {
            RELEASE_LOCK(&lock);
            perr << "Error opening memory file" << std::endl;
            return;
        }
        // Get the memory usage
        std::vector<MemoryResults> data(1,getMemoryResults());
        if ( global ) {
            gather_memory( data );
        }
        for (size_t k=0; k<data.size(); k++) {
            size_t count = data[k].time.size();
            ASSERT(data[k].bytes.size()==count);
            // Determine a scale factor so we can use unsigned int to store the memory
            size_t max_mem_size = 0;
            for (size_t i=0; i<count; i++)
                max_mem_size = std::max<uint64_t>(max_mem_size,data[k].bytes[i]);
            size_t scale;
            std::string units;
            if ( max_mem_size < 0xFFFFFFFF ) {
                scale = 1;
                units = "bytes";
            } else if ( max_mem_size < 0x3FFFFFFFFFF ) {
                scale = 1024;
                units = "kB";
            } else if ( max_mem_size < 0xFFFFFFFFFFFFF ) {
                scale = 1024*1024;
                units = "MB";
            } else {
                scale = 1024*1024*1024;
                units = "GB";
            }
            // Copy the time and size to new buffers
            double *time = new double[count];
            unsigned int *size = new unsigned int[count];
            for (size_t i=0; i<count; i++) {
                time[i] = data[k].time[i];
                size[i] = static_cast<unsigned int>(data[k].bytes[i]/scale);
            }
            // Save the results
            // Note: Visual studio has an issue with type %zi
            ASSERT(sizeof(unsigned int)==4);
            fprintf(memoryFile,"<N=%li,type1=%s,type2=%s,units=%s,rank=%i>\n",
                static_cast<long int>(count),"double","uint32",units.c_str(),rank);
            size_t N1 = fwrite(time,sizeof(double),count,memoryFile);
            size_t N2 = fwrite(size,sizeof(unsigned int),count,memoryFile);
            fprintf(memoryFile,"\n");
            delete [] time;
            delete [] size;
            if ( N1!=(size_t)count || N2!=(size_t)count )
                ERROR_MSG("Failed to write memory results\n");
        }
        fclose(memoryFile);
    }
}


/***********************************************************************
* Load the timer and trace data                                        *
***********************************************************************/
static inline void get_field( const char* line, size_t N, const char* name, char* data )
{
    const char* ptr = strstr( line, name );
    if ( ptr==NULL ) {
        data[0] = 0;
    } else {
        int i1 = -1;
        int i2 = -1;
        for (int i=0; i<(int)N; i++) {
            if ( ptr[i]<32 )
                break;
            if ( ptr[i]=='=' )
                i1 = i+1;
            if ( i1==-1 )
                continue;
            if ( ptr[i]==',' || ptr[i]=='>' || ptr[i]=='\n' ) {
                i2 = i;
                break;
            }
        }
        ASSERT(i2!=-1&&i2>=i1);
        memcpy(data,&ptr[i1],i2-i1);
        data[i2-i1] = 0;
    }
}
static inline char* getFieldArray( char *line, std::vector<std::pair<char*,char*> >& data )
{
    // This function parses a line of the form <field=value,field=value>
    const char e = 0x0E;    // Escape character for printing strings
    ASSERT(*line=='<');
    data.clear();
    line++;
    while ( *line>=32 ) {
        int j1, j2;
        for (j1=0; line[j1]!='='; j1++) {}
        line[j1] = 0;
        j1++;
        if ( line[j1]==e ) {
            line[j1] = 0;
            j1++;
            for (j2=j1; line[j2]!=e; j2++) {}
            line[j2] = 0;
            line[j2+1] = 0;
            j2++;
        } else {
            for (j2=j1; line[j2]!='>'&&line[j2]!=','; j2++) {}
            line[j2] = 0;
        }
        data.push_back(std::pair<char*,char*>(line,&line[j1]));
        line += j2+1;
    }
    return line+1;
}
static inline void get_active_ids( const char* active_list, std::vector<id_struct>& ids )
{
    ids.resize(0);
    const char *p1 = active_list;
    while ( *p1 != 0 ) {
        while ( *p1==' ' || *p1=='[' || *p1==']' ) { p1++; }
        const char *p2 = p1;
        while ( *p2!=' ' && *p2!=']' && *p2!=0 ) { p2++; }
        if ( p2 > p1 ) {
            char tmp[10];
            for (int i=0; i<p2-p1; i++)
                tmp[i] = p1[i];
            tmp[p2-p1] = 0;
            ids.push_back(id_struct(tmp));
            p1 = p2;
        }
    }
}
/*static inline std::vector<id_struct> get_active_ids( const char* active_list )
{
    std::vector<id_struct> ids;
    get_active_ids( active_list, ids );
    std::sort(ids.begin(),ids.end());
    return ids;
}*/
int ProfilerApp::loadFiles( const std::string& filename, int index, TimerMemoryResults& data )
{
    int N_procs = 0;
    std::string date;
    bool trace_data, memory_data;
    char timer[200], trace[200], memory[200];
    sprintf(timer,"%s.%i.timer",filename.c_str(),index);
    sprintf(trace,"%s.%i.trace",filename.c_str(),index);
    sprintf(memory,"%s.%i.memory",filename.c_str(),index);
    load_timer(timer,data.timers,N_procs,date,trace_data,memory_data);
    if ( trace_data )
        load_trace(trace,data.timers);
    if ( memory_data )
        load_memory(memory,data.memory);
    return N_procs;
}
template<class TYPE> inline void keepRank( std::vector<TYPE>& data, int rank )
{
    size_t i2 = 0;
    for (size_t i1=0; i1<data.size(); i1++) {
        if ( (int)data[i1].rank==rank ) {
            data[i2] = data[i1];
            i2++;
        }
    }
    data.resize(i2);
}
TimerMemoryResults ProfilerApp::load( const std::string& filename, int rank, bool global )
{
    TimerMemoryResults data;
    data.timers.clear();
    data.memory.clear();
    int N_procs = 0;
    if ( global ) {
        N_procs = loadFiles(filename,0,data);
        if ( rank != -1 ) {
            for (size_t j=0; j<data.timers.size(); j++)
                keepRank( data.timers[j].trace, rank );
            keepRank( data.memory, rank );
        }
    } else {
        if ( rank==-1 ) {
            // Load the root file
            N_procs = loadFiles(filename,1,data);
            // Reserve trace memory for all ranks
            for (size_t i=0; i<data.timers.size(); i++)
                data.timers[i].trace.reserve(N_procs*data.timers[i].trace.size());
            // Load the remaining files
            for (int i=1; i<N_procs; i++)
                loadFiles(filename,i+1,data);
        } else {
            N_procs = loadFiles(filename,rank+1,data);
        }
    }
    data.N_procs = N_procs;
    return data;
}
void ProfilerApp::load_timer( const std::string& filename, std::vector<TimerResults>& data, 
    int& N_procs, std::string& date, bool& trace_data, bool& memory_data )
{
    // Load the file to memory for reading
    FILE *fid = fopen(filename.c_str(),"rb");
    if (fid==NULL)
        ERROR_MSG("Error opening file: "+filename);
    fseek(fid , 0 , SEEK_END);
    size_t file_length = ftell(fid);
    if ( file_length > 0x80000000 ) {
        // We do not yet support large files, we need to read the data in chunks
        fclose(fid);
        ERROR_MSG("Large timer files are not yet supported (likely to exhaust ram)");
        return;  // This is unnecessary but suppresses warnings from cppcheck
    }
    char *buffer = new char[file_length+10];
    memset(buffer,0,file_length+10);
    rewind(fid);
    size_t result = fread(buffer,1,file_length,fid);
    if (result!=file_length) {
        delete [] buffer;
        fclose(fid);
        ERROR_MSG("error reading file");
        return;
    }
    fclose(fid);
    // Create a map of the ids and indicies of the timers (used for searching)
    std::map<id_struct,size_t> id_map;
    for (size_t i=0; i<data.size(); i++)
        id_map.insert( std::pair<id_struct,size_t>(data[i].id,i) );
    // Parse the data (this take most of the time)
    N_procs=-1;
    int rank=-1;
    trace_data = false;
    memory_data = false;
    date = std::string();
    std::vector<std::pair<char*,char*> > fields;
    std::vector<id_struct> active;
    char *line = buffer;
    while ( line<buffer+file_length ) {
        // Check if we are reading a dummy (human-readable) line
        if ( line[0] != '<' ) {
            while ( *line>=32 ) { line++; }
            line++;
            continue;
        }
        // Read the next line and split the fields
        line = getFieldArray(line,fields);
        if ( strcmp(fields[0].first,"N_procs")==0 ) {
            // We are loading the header
            N_procs = atoi(fields[0].second);
            // Load the remaining fields
            for (size_t i=1; i<fields.size(); i++) {
                if ( strcmp(fields[i].first,"id")==0 || strcmp(fields[i].first,"rank")==0 ) {
                    // Load the id/rank
                    rank = atoi(fields[i].second);
                } else if ( strcmp(fields[i].first,"store_trace")==0 ) {
                    // Check if we stored the trace file
                    trace_data = atoi(fields[i].second)==1;
                } else if ( strcmp(fields[i].first,"store_memory")==0 ) {
                    // Check if we stored the memory file (optional)
                    memory_data = atoi(fields[i].second)==1;
                } else if ( strcmp(fields[i].first,"date")==0 ) {
                    // Load the date (optional)
                    date = std::string(fields[i].second);
                } else {
                    ERROR_MSG(std::string("Unknown field (header): ")+fields[i].first);
                }
            }
        } else if ( strcmp(fields[0].first,"timer:id")==0 ) {
            // We are loading a timer field
            id_struct id( fields[0].second );
            // Find the timer
            std::map<id_struct,size_t>::iterator it = id_map.find(id);
            if ( it==id_map.end() ) {
                // Create a new timer
                size_t k = data.size();
                id_map.insert( std::pair<id_struct,size_t>(id,k) );
                data.resize(k+1);
                TimerResults& timer = data[k];
                timer.id = id;
                // Load the remaining fields
                for (size_t i=1; i<fields.size(); i++) {
                    if ( strcmp(fields[i].first,"message")==0 ) {
                        // Load the message
                        timer.message = std::string(fields[i].second);
                    } else if ( strcmp(fields[i].first,"file")==0 ) {
                        // Load the filename
                        timer.file = std::string(fields[i].second);
                    } else if ( strcmp(fields[i].first,"path")==0 ) {
                        // Load the path
                        timer.path = std::string(fields[i].second);
                    } else if ( strcmp(fields[i].first,"start")==0 ) {
                        // Load the start line
                        timer.start = atoi(fields[i].second);
                    } else if ( strcmp(fields[i].first,"stop")==0 ) {
                        // Load the stop line
                        timer.stop = atoi(fields[i].second);
                    } else if ( strcmp(fields[i].first,"thread")==0 ||
                                strcmp(fields[i].first,"N")==0 ||
                                strcmp(fields[i].first,"min")==0 ||
                                strcmp(fields[i].first,"max")==0 ||
                                strcmp(fields[i].first,"tot")==0 )
                    {
                        // Obsolete fields
                    } else {
                        ERROR_MSG(std::string("Unknown field (timer): ")+fields[i].first);
                    }
                }
            }
        } else if ( strcmp(fields[0].first,"trace:id")==0 ) {
            // We are loading a trace field
            id_struct id( fields[0].second );
            // Find the trace
            std::map<id_struct,size_t>::iterator it = id_map.find(id);
            if ( it==id_map.end() )
                ERROR_MSG("trace did not find matching timer");
            size_t index = it->second;
            data[index].trace.resize(data[index].trace.size()+1);
            TraceResults& trace = data[index].trace.back();
            trace.id = id;
            trace.N_trace = 0;
            trace.rank = rank;
            // Load the remaining fields
            for (size_t i=1; i<fields.size(); i++) {
                if ( strcmp(fields[i].first,"thread")==0 ) {
                    // Load the thread id
                    trace.thread = atoi(fields[i].second);
                } else if ( strcmp(fields[i].first,"rank")==0 ) {
                    // Load the rank id
                    trace.rank = atoi(fields[i].second);
                } else if ( strcmp(fields[i].first,"N")==0 ) {
                    // Load N
                    trace.N = atoi(fields[i].second);
                } else if ( strcmp(fields[i].first,"min")==0 ) {
                    // Load min
                    trace.min = static_cast<float>(atof(fields[i].second));
                } else if ( strcmp(fields[i].first,"max")==0 ) {
                    // Load max
                    trace.max = static_cast<float>(atof(fields[i].second));
                } else if ( strcmp(fields[i].first,"tot")==0 ) {
                    // Load tot
                    trace.tot = static_cast<float>(atof(fields[i].second));
                } else if ( strcmp(fields[i].first,"active")==0 ) {
                    // Load the active timers
                    get_active_ids(fields[i].second,active);
                    trace.N_active = static_cast<unsigned short>(active.size());
                    trace.allocate();
                    for (size_t j=0; j<active.size(); j++)
                        trace.active()[j] = active[j];
                } else {
                    ERROR_MSG(std::string("Unknown field (trace): ")+fields[i].first);
                }
            }
        } else {
            ERROR_MSG("Unknown data field");
        }
    }
    delete [] buffer;
}
void ProfilerApp::load_trace( const std::string& filename, std::vector<TimerResults>& data )
{
    // Create a map of the ids and indicies of the timers (used for searching)
    std::map<id_struct,size_t> id_map;
    for (size_t i=0; i<data.size(); i++)
        id_map.insert( std::pair<id_struct,size_t>(data[i].id,i) );
    // Open the file for reading
    FILE *fid = fopen(filename.c_str(),"rb");
    if (fid==NULL)
        ERROR_MSG("Error opening file: "+filename);
    std::vector<id_struct> active;
    const size_t MAX_LINE = 0x10000;
    char *line = new char[MAX_LINE];
    char *field = new char[MAX_LINE];
    memset(line,0,MAX_LINE);
    memset(field,0,MAX_LINE);
    while ( 1 ) {
        // Read the header
        char *rtn = fgets(line,MAX_LINE-1,fid);
        if ( rtn==NULL )
            break;
        if ( line[0] <= 10 )
            continue;
        // Get the id and find the appropriate timer
        get_field(line,MAX_LINE,"id=",field);
        ASSERT(field[0]!=0);
        id_struct id( field );
        std::map<id_struct,size_t>::iterator it = id_map.find(id);
        if ( it==id_map.end() )
            ERROR_MSG("Did not find matching timer");
        TimerResults& timer = data[it->second];
        // Read the remaining trace header data
        get_field(line,MAX_LINE,"thread=",field);
        ASSERT(field[0]!=0);
        unsigned int thread = static_cast<unsigned int>(atoi(field));
        get_field(line,MAX_LINE,"rank=",field);
        ASSERT(field[0]!=0);
        unsigned int rank = static_cast<unsigned int>(atoi(field));
        get_field(line,MAX_LINE,"active=",field);
        ASSERT(field[0]!=0);
        get_active_ids(field,active);
        get_field(line,MAX_LINE,"N=",field);
        ASSERT(field[0]!=0);
        unsigned long int N = strtoul(field,NULL,10);
        // Find the appropriate trace
        int index = -1;
        for (size_t i=0; i<timer.trace.size(); i++) {
            if ( timer.trace[i].thread != thread ||
                 timer.trace[i].rank != rank ||
                 timer.trace[i].N_active != active.size() )
                continue;
            bool match = true;
            for (size_t j=0; j<active.size(); j++)
                match = match && active[j]==timer.trace[i].active()[j];
            if ( !match )
                continue;
            index = static_cast<int>(i);
        }
        ASSERT(index!=-1);
        TraceResults& trace = timer.trace[index];
        ASSERT(trace.N_trace==0);  // Check that we do not have previous data
        trace.N_trace = N;
        trace.allocate();
        for (size_t i=0; i<active.size(); i++)
            trace.active()[i] = active[i];
        // Read the data
        size_t N1 = fread(trace.start(),sizeof(double),N,fid);
        size_t N2 = fread(trace.stop(),sizeof(double),N,fid);
        ASSERT(N1==N&&N2==N);
        size_t rtn2 = fread(field,1,1,fid);
        ASSERT(rtn2==1);
    }
    fclose(fid);
    delete [] line;
    delete [] field;
}
inline size_t get_scale( const std::string& units )
{
    size_t scale = 1;
    if ( units == "bytes" ) {
        scale = 1;
    } else if ( units == "kB" ) {
        scale = 1024;
    } else if ( units == "MB" ) {
        scale = 1024*1024;
    } else if ( units == "GB" ) {
        scale = 1024*1024*1024;
    } else {
        ERROR_MSG("Not finished\n");
    }
    return scale;
}
void ProfilerApp::load_memory( const std::string& filename, std::vector<MemoryResults>& data )
{
    // Open the file for reading
    FILE *fid = fopen(filename.c_str(),"rb");
    if (fid==NULL)
        ERROR_MSG("Error opening file: "+filename);
    const size_t MAX_LINE = 0x10000;
    char *line = new char[MAX_LINE];
    char *field = new char[MAX_LINE];
    memset(line,0,MAX_LINE);
    memset(field,0,MAX_LINE);
    while ( 1 ) {
        // Read the header
        char *rtn = fgets(line,MAX_LINE-1,fid);
        if ( rtn==NULL )
            break;
        if ( line[0] <= 10 )
            continue;
        data.resize(data.size()+1);
        MemoryResults& memory = data.back();
        // Get the header fields
        get_field(line,MAX_LINE,"N=",field);
        ASSERT(field[0]!=0);
        unsigned long int N = strtoul(field,NULL,10);
        get_field(line,MAX_LINE,"type1=",field);
        ASSERT(field[0]!=0);
        std::string type1(field);
        get_field(line,MAX_LINE,"type2=",field);
        ASSERT(field[0]!=0);
        std::string type2(field);
        get_field(line,MAX_LINE,"units=",field);
        ASSERT(field[0]!=0);
        size_t scale = get_scale(field);
        get_field(line,MAX_LINE,"rank=",field);
        ASSERT(field[0]!=0);
        memory.rank = atoi(field);
        // Get the data
        if ( type1=="double" && type2=="uint32" ) {
            double *time = new double[N];
            unsigned int *size = new unsigned int[N];
            size_t N1 = fread(time,sizeof(double),N,fid);
            size_t N2 = fread(size,sizeof(unsigned int),N,fid);
            ASSERT(N1==N&&N2==N);
            memory.time.resize(N);
            memory.bytes.resize(N);
            for (size_t i=0; i<N; i++) {
                memory.time[i] = time[i];
                memory.bytes[i] = scale*size[i];
            }
            delete [] time;
            delete [] size;
        } else {
            ERROR_MSG("Not finished\n");
        }
    }
    fclose(fid);
    delete [] line;
    delete [] field;
}


/***********************************************************************
* Function to get the list of active timers                            *
***********************************************************************/
std::vector<id_struct> ProfilerApp::get_active_list( TRACE_TYPE *active, unsigned int myIndex, const thread_info *head )
{
    std::vector<id_struct> active_list;
    for (unsigned int i=0; i<TRACE_SIZE; i++) {
        for (unsigned int j=0; j<TRACE_TYPE_size; j++) {
            unsigned int k = i*TRACE_TYPE_size + j;
            if ( k == myIndex )
                continue;
            TRACE_TYPE mask = ((TRACE_TYPE)0x1)<<j;
            if ( (mask&active[i])!=0 ) {
                // The kth timer is active, find the index and write it to the file
                store_timer* timer_tmp = NULL;
                for (int m=0; m<TIMER_HASH_SIZE; m++) {
                    timer_tmp = head->head[m];
                    while ( timer_tmp!=NULL ) {
                        if ( timer_tmp->trace_index==k )
                            break;
                        timer_tmp = timer_tmp->next;
                    }
                    if ( timer_tmp!=NULL )
                        break;
                }
                if ( timer_tmp==NULL )
                    ERROR_MSG("Internal Error");
                active_list.push_back( convert_timer_id(timer_tmp->id) );
            }
        }
    }
    return active_list;
}


/***********************************************************************
* Function to return a pointer to the global timer info and create it  *
* if necessary.                                                        *
* Note: if we are creating the timer we need to intialize the          *
*    the std::strings since the assignment operator and the destructor *
*    may not be thread safe (see Note 1).                              *
***********************************************************************/
ProfilerApp::store_timer_data_info* ProfilerApp::get_timer_data( 
    uint64_t id, const char* message, const char* filename, int start, int stop )
{
    size_t key = GET_TIMER_HASH( id );    // Get the hash index
    if ( timer_table[key]==NULL ) {
        // The global timer does not exist, create it (requires blocking)
        // Acquire the lock (neccessary for modifying the timer_table)
        bool error = GET_LOCK(&lock);
        if ( error )
            return NULL;
        // Check if the entry is still NULL
        if ( timer_table[key]==NULL ) {
            // Create a new entry
            // Note: we must initialize the std::string within a lock for thread safety
            store_timer_data_info *info_tmp = new store_timer_data_info;
            TimerUtility::atomic::int64_atomic size = sizeof(store_timer_data_info);
            TimerUtility::atomic::atomic_add(&d_bytes,size);
            const char* filename2 = strip_path(filename);
            info_tmp->id = id;
            info_tmp->start_line = start;
            info_tmp->stop_line = stop;
            info_tmp->next = NULL;
            info_tmp->message = std::string(message);
            info_tmp->filename = std::string(filename2);
            info_tmp->path = std::string(filename,0,filename2-filename);
            timer_table[key] = info_tmp;
            N_timers++;
        }
        // Release the lock
        RELEASE_LOCK(&lock);
    }
    volatile store_timer_data_info *info = timer_table[key];
    while ( info->id != id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( info->next==NULL ) {
            // Acquire the lock
            // Acquire the lock (neccessary for modifying the timer_table)
            bool error = GET_LOCK(&lock);
            if ( error )
                return NULL;
            // Check if another thread created an entry while we were waiting for the lock
            if ( info->next==NULL ) {
                // Create a new entry
                // Note: we must initialize the std::string within a lock for thread safety
                store_timer_data_info *info_tmp = new store_timer_data_info;
                const char* filename2 = strip_path(filename);
                info_tmp->id = id;
                info_tmp->start_line = start;
                info_tmp->stop_line = stop;
                info_tmp->next = NULL;
                info_tmp->message = std::string(message);
                info_tmp->filename = std::string(filename2);
                info_tmp->path = std::string(filename,0,filename2-filename);
                info->next = info_tmp;
                N_timers++;
            }
            // Release the lock
            RELEASE_LOCK(&lock);
        } 
        // Advance to the next entry
        info = info->next;
    }
    return const_cast<store_timer_data_info*>(info);
}


/***********************************************************************
* Gather all timers on rank 0                                          *
***********************************************************************/
void ProfilerApp::gather_timers( std::vector<TimerResults>& timers )
{
    comm_barrier();
    int rank = comm_rank();
    int N_procs = comm_size();
    if ( rank==0 ) {
        for (int r=1; r<N_procs; r++) {
            char *buffer = comm_recv1<char>(r,0);
            size_t pos = 0;
            size_t N_timers = 0;
            memcpy(&N_timers,&buffer[pos],sizeof(N_timers));
            pos += sizeof(N_timers);
            ASSERT(N_timers<0x100000);
            std::vector<TimerResults> add(N_timers);
            for (size_t i=0; i<add.size(); i++) {
                add[i].unpack(&buffer[pos]);
                pos += add[i].size();
            }
            add_timers( timers, add );
            delete [] buffer;
        }
    } else {
        size_t N_bytes = sizeof(size_t);
        for (size_t i=0; i<timers.size(); i++)
            N_bytes += timers[i].size();
        char *buffer = new char[N_bytes];
        size_t pos = 0;
        size_t N_timers = timers.size();
        memcpy(&buffer[pos],&N_timers,sizeof(N_timers));
        pos += sizeof(N_timers);
        for (size_t i=0; i<timers.size(); i++) {
            timers[i].pack(&buffer[pos]);
            pos += timers[i].size();
        }
        ASSERT(pos==N_bytes);
        comm_send1<char>( buffer, N_bytes, 0, 0 );
        delete [] buffer;
        timers.clear();
    }
    comm_barrier();
}
void ProfilerApp::gather_memory( std::vector<MemoryResults>& memory )
{
    comm_barrier();
    int rank = comm_rank();
    int N_procs = comm_size();
    ASSERT(memory.size()==1);
    if ( rank==0 ) {
        memory.resize(N_procs);
        for (int r=1; r<N_procs; r++) {
            memory[r].rank = r;
            memory[r].time = comm_recv2<double>(r,1);
            memory[r].bytes = comm_recv2<uint64_t>(r,2);
        }
    } else {
        ASSERT(memory[0].time.size()==memory[0].bytes.size());
        comm_send2<double>( memory[0].time,  0, 1 );
        comm_send2<uint64_t>( memory[0].bytes, 0, 2 );
        memory.clear();
    }
    comm_barrier();
}
void ProfilerApp::add_timers( std::vector<TimerResults>& timers, 
    const std::vector<TimerResults>& add )
{
    std::map<id_struct,size_t> id_map;
    for (size_t i=0; i<timers.size(); i++)
        id_map.insert( std::pair<id_struct,size_t>(timers[i].id,i) );
    for (size_t i=0; i<add.size(); i++) {
        std::map<id_struct,size_t>::iterator it = id_map.find(add[i].id);
        if ( it == id_map.end() ) {
            size_t j = timers.size();
            timers.push_back( add[i] );
            id_map.insert( std::pair<id_struct,size_t>(timers[j].id,j) );
        } else {
            size_t j = it->second;
            for (size_t k=0; k<add[i].trace.size(); k++)
                timers[j].trace.push_back( add[i].trace[k] );
        }
    }
}


/***********************************************************************
* Subroutine to perform a quicksort                                    *
***********************************************************************/
template <class type_a, class type_b>
static inline void quicksort2(int n, type_a *arr, type_b *brr)
{
    bool test;
    int i, ir, j, jstack, k, l, istack[100];
    type_a a, tmp_a;
    type_b b, tmp_b;
    jstack = 0;
    l = 0;
    ir = n-1;
    while (1) {
        if ( ir-l < 7 ) {             // Insertion sort when subarray small enough.
            for ( j=l+1; j<=ir; j++ ) {
                a = arr[j];
                b = brr[j];
                test = true;
                for (i=j-1; i>=0; i--) {
                    if ( arr[i] < a ) {
                        arr[i+1] = a;
                        brr[i+1] = b;
                        test = false;
                        break;
                    }
                    arr[i+1] = arr[i];
                    brr[i+1] = brr[i];
                }
                if ( test ) {
                    i = l-1;
                    arr[i+1] = a;
                    brr[i+1] = b;
                }
            }
            if ( jstack==0 )
                return;
            ir = istack[jstack];    // Pop stack and begin a new round of partitioning.
            l = istack[jstack-1];
            jstack -= 2;
        } else {
            k = (l+ir)/2;           // Choose median of left, center and right elements as partitioning
                                    // element a. Also rearrange so that a(l) ? a(l+1) ? a(ir).
            tmp_a = arr[k];
            arr[k] = arr[l+1];
            arr[l+1] = tmp_a;
            tmp_b = brr[k];
            brr[k] = brr[l+1];
            brr[l+1] = tmp_b;
            if ( arr[l]>arr[ir] ) {
                tmp_a = arr[l];
                arr[l] = arr[ir];
                arr[ir] = tmp_a;
                tmp_b = brr[l];
                brr[l] = brr[ir];
                brr[ir] = tmp_b;
            }
            if ( arr[l+1] > arr[ir] ) {
                tmp_a = arr[l+1];
                arr[l+1] = arr[ir];
                arr[ir] = tmp_a;
                tmp_b = brr[l+1];
                brr[l+1] = brr[ir];
                brr[ir] = tmp_b;
            }
            if ( arr[l] > arr[l+1] ) {
                tmp_a = arr[l];
                arr[l] = arr[l+1];
                arr[l+1] = tmp_a;
                tmp_b = brr[l];
                brr[l] = brr[l+1];
                brr[l+1] = tmp_b;
            }
            // Scan up to find element > a
            j = ir;
            a = arr[l+1];           // Partitioning element.
            b = brr[l+1];
            for (i=l+2; i<=ir; i++) { 
                if ( arr[i]<a ) 
                    continue;
                while ( arr[j]>a )  // Scan down to find element < a.
                    j--;
                if ( j < i )
                    break;          // Pointers crossed. Exit with partitioning complete.
                tmp_a = arr[i];     // Exchange elements of both arrays.
                arr[i] = arr[j];
                arr[j] = tmp_a;
                tmp_b = brr[i];
                brr[i] = brr[j];
                brr[j] = tmp_b;
            }
            arr[l+1] = arr[j];      // Insert partitioning element in both arrays.
            arr[j] = a;
            brr[l+1] = brr[j];
            brr[j] = b;
            jstack += 2;
            // Push pointers to larger subarray on stack, process smaller subarray immediately.
            if ( ir-i+1 >= j-l ) {
                istack[jstack] = ir;
                istack[jstack-1] = i;
                ir = j-1;
            } else {
                istack[jstack] = j-1;
                istack[jstack-1] = l;
                l = i;
            }
        }
    }
}


/***********************************************************************
* Subroutine to perform a merge between two or more sorted lists       *
***********************************************************************/
template <class type_a, class type_b>
static inline void mergeArrays( size_t N_list, size_t *N, type_a **arr, type_b **brr,
    size_t *N_result, type_a **arr_result, type_b **brr_result )
{
    // Check that the inputs are sorted
    for (size_t i=0; i<N_list; i++) {
        for (size_t j=1; j<N[i]; j++) {
            if ( arr[i][j] < arr[i][j-1] )
                ERROR_MSG("Input arrays must be sorted\n");
        }
    }
    // Allocate enough memory to store the results
    *N_result = 0;
    for (size_t i=0; i<N_list; i++)
        *N_result += N[i];
    if ( *N_result==0 ) {
        *arr_result = NULL;
        *brr_result = NULL;
        return;
    }
    *arr_result = new type_a[*N_result];
    *brr_result = new type_b[*N_result];
    // Get the global max
    type_a max_v = arr[0][N[0]-1];
    for (size_t j=1; j<N_list; j++) {
        if ( arr[j][N[j]-1] > max_v )
            max_v = arr[j][N[j]-1];
    }
    // Merge the lists
    std::vector<size_t> index(N_list,0);
    for (size_t i=0; i<*N_result; i++) {
        size_t index2 = 0;
        type_a min_v = max_v+1;
        for (size_t j=0; j<N_list; j++) {
            if ( index[j] == N[j] )
                continue;
            if ( arr[j][index[j]] < min_v ) {
                index2 = j;
                min_v = arr[j][index[j]];
            }
        }
        (*arr_result)[i] = arr[index2][index[index2]];
        (*brr_result)[i] = brr[index2][index[index2]];
        index[index2]++;
    }
}


/***********************************************************************
* C interfaces                                                         *
***********************************************************************/
extern "C" {
    void global_profiler_enable( int level )
    {
        global_profiler.enable(level);
    }
    void global_profiler_disable( )
    {
        global_profiler.disable();
    }
    int global_profiler_get_level( )
    {
        return global_profiler.get_level();
    }
    void global_profiler_synchronize( )
    {
        global_profiler.synchronize();
    }
    void global_profiler_set_store_trace( int flag )
    {
        global_profiler.set_store_trace(flag!=0);
    }
    void global_profiler_set_store_memory( int flag )
    {
        global_profiler.set_store_memory(flag!=0);
    }
    void global_profiler_start( const char* name, const char* file, int line, int level )
    {
        global_profiler.start(name,file,line,level);
    }
    void global_profiler_stop( const char* name, const char* file, int line, int level )
    {
        global_profiler.stop(name,file,line,level);
    }
    void global_profiler_save( const char* name, int global )
    {
        global_profiler.save(name,global!=0);
    }
}




