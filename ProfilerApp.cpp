#include "ProfilerApp.h"

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <time.h>
#include <stdexcept>
#include <vector>
#include <map>
#include "mex.h"


inline void ERROR_MSG( const std::string& msg ) {
    mexErrMsgTxt(msg.c_str());
}

#define MONITOR_PROFILER_PERFORMANCE 0

#define pout    std::cout
#define perr    std::cerr
#define printp  mexPrintf


ProfilerApp global_profiler;

extern "C" {
    #include "assert.h"
}

// Include system dependent headers and define some functions
#ifdef __WORDSIZE
    #define ARCH_SIZE __WORDSIZE
#elif defined(_WIN64)
    #define ARCH_SIZE 64
#elif defined(_WIN32) // Note: WIN64 also defines WIN32
    #define ARCH_SIZE 32
#endif
#ifdef USE_WINDOWS
    #include <windows.h>
    #include <stdio.h>   
    #include <tchar.h>
    #include <Psapi.h>
    #define get_time(x) QueryPerformanceCounter(x)
    #define get_frequency(f) QueryPerformanceFrequency(f)
    #define get_diff(start,end,f) \
        static_cast<double>(end.QuadPart-start.QuadPart)/static_cast<double>(f.QuadPart)
#elif defined(USE_LINUX)
    #include <signal.h>
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <malloc.h>
    #define get_time(x) gettimeofday(x,NULL);
    #define get_frequency(f) (*f=timeval())
    #define get_diff(start,end,f) 1e-6*static_cast<double>( \
        0xF4240*(static_cast<int64_t>(end.tv_sec)-static_cast<int64_t>(start.tv_sec)) + \
                (static_cast<int64_t>(end.tv_usec)-static_cast<int64_t>(start.tv_usec)) )
#elif defined(USE_MAC)
    #include <signal.h>
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <mach/mach.h>
    #define get_time(x) gettimeofday(x,NULL);
    #define get_frequency(f) (*f=timeval())
    #define get_diff(start,end,f) 1e-6*static_cast<double>( \
        0xF4240*(static_cast<int64_t>(end.tv_sec)-static_cast<int64_t>(start.tv_sec)) + \
                (static_cast<int64_t>(end.tv_usec)-static_cast<int64_t>(start.tv_usec)) )
    #ifdef __LP64__
        #define ARCH_SIZE 64
    #else
        #define ARCH_SIZE 32
    #endif
#else
    #error Unknown OS
#endif


// Check the ARCH_SIZE and set macros
// Note: ARCH_SIZE must match the number of bits in size_t
#if ARCH_SIZE == 64
    // 32-bit macros
    // Use the hashing function 2^32*0.5*(sqrt(5)-1)
    #define GET_TIMER_HASH(id)  (((id*0x9E3779B9)>>16)%TIMER_HASH_SIZE)
    #define GET_THREAD_HASH(id) (((id*0x9E3779B9)>>16)%THREAD_HASH_SIZE)
#elif ARCH_SIZE == 32
    // 64-bit macros
    // Use the hashing function 2^32*0.5*(sqrt(5)-1)
    #define GET_TIMER_HASH(id)  (((id*0x9E3779B97F4A7C15)>>48)%TIMER_HASH_SIZE)
    #define GET_THREAD_HASH(id) (((id*0x9E3779B97F4A7C15)>>48)%THREAD_HASH_SIZE)
#else
    #error Cannot identify 32 vs 64-bit
#endif


// Check the limits of the define variables
#if MAX_TRACE_MEMORY > 0xFFFFFFFF
    #error MAX_TRACE_MEMORY must be < 2^32
#endif


#define ASSERT(EXP) do {                                            \
    if ( !(EXP) ) {                                                 \
        std::stringstream stream;                                   \
        stream << "Failed assertion: " << #EXP                      \
            << " " << __FILE__ << " " << __LINE__;                  \
        ERROR_MSG(stream.str());                                    \
    }                                                               \
}while(0)


template <class type_a, class type_b>
static inline void quicksort2(int n, type_a *arr, type_b *brr);

template <class type_a, class type_b>
static inline void mergeArrays( size_t N_list, 
    size_t *N, type_a **arr, type_b **brr,
    size_t *N_result, type_a **arr_result, type_b **brr_result );


#if MONITOR_PROFILER_PERFORMANCE > 0
    double total_start_time = 0;
    double total_stop_time = 0;
    double total_block_time = 0;
    double total_thread_time = 0;
    double total_trace_id_time = 0;
#endif


// Inline function to get the current time/date string (without the newline character)
static inline std::string getDateString() 
{
    time_t rawtime;
    time ( &rawtime );
    std::string tmp(ctime(&rawtime));
    return tmp.substr(0,tmp.length()-1);
}


// Inline function to get the filename without the path
static inline const char* strip_path( const char* filename_in )
{
    const char *s = filename_in;
    int length = 1;
    while(*(++s)) { ++length; }
    const char* filename = filename_in;
    for (int i=length-1; i>=0; --i) {
        if ( filename[i]==47 || filename[i]==92 ) {
            filename = &filename[i+1];
            break;
        }
    }
    return filename;
}


// Helper function to check (and if necessary resize) an array
template<class type> void check_allocate_array( type** data, size_t N_current, size_t N_max )
{
    size_t size_old, size_new;
    const size_t size_min = 256;
    if ( *data==NULL ) {
        // We haven't allocated any memory yet
        size_old = 0;
        size_new = size_min;
    } else {
        // We want to allocate memory in powers of 2
        // The current allocated size is the smallest power of 2 that is >= N
        size_old = size_min;
        while ( size_old < N_current )
            size_old *= 2;
        // Double the storage space (if needed)
        if ( N_current == size_old )
            size_new = 2*size_old;
        else
            size_new = size_old;
        // Stop allocating memory if we reached the limit
        if ( size_new > N_max ) 
            size_new = N_max;
        if ( size_old > N_max ) 
            size_old = N_max;
    }
    if ( size_old != size_new ) {
        // Expand the trace list
        type* data_new = new type[size_new];
        memset(data_new,0,size_new*sizeof(type));
        if ( *data!=NULL ) {
            memcpy(data_new,*data,size_old*sizeof(type));
            delete [] *data;
        }
        *data = data_new;
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
            MPI_Allreduce(&val,&result,1,MPI_DOUBLE,MPI_MAX,MPI_COMM_WORLD);
    #endif
    return result;
}
static inline void comm_send1( const void *buf, size_t bytes, int dest, int tag )
{
    #ifdef USE_MPI
        int N_send = (bytes+sizeof(double)-1)/sizeof(double);
        int err = MPI_Send( buf, N_send, MPI_DOUBLE, dest, tag, MPI_COMM_WORLD );
        ASSERT(err==MPI_SUCCESS);
    #endif
}
static inline void* comm_recv1( int source, int tag )
{
    #ifdef USE_MPI
        MPI_Status status;
        int err = MPI_Probe( source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        int count;
        MPI_Get_count( &status, MPI_DOUBLE, &count );
        double *buf = new double[count];
        err = MPI_Recv( buf, count, MPI_DOUBLE, source, tag, MPI_COMM_WORLD, &status );
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
        ASSERT(sizeof(TYPE)==sizeof(double));
        int count = data.size();
        const TYPE *ptr = NULL;
        if ( !data.empty() ) { ptr = &data[0]; }
        int err = MPI_Send( ptr, count, MPI_DOUBLE, dest, tag, MPI_COMM_WORLD );
        ASSERT(err==MPI_SUCCESS);
    #endif
}
template<class TYPE>
static inline std::vector<TYPE> comm_recv2( int source, int tag )
{
    #ifdef USE_MPI
        ASSERT(sizeof(TYPE)==sizeof(double));
        MPI_Status status;
        int err = MPI_Probe( source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        int count;
        MPI_Get_count( &status, MPI_DOUBLE, &count );
        std::vector<TYPE> data(count);
        TYPE *ptr = NULL;
        if ( !data.empty() ) { ptr = &data[0]; }
        err = MPI_Recv( ptr, count, MPI_DOUBLE, source, tag, MPI_COMM_WORLD, &status );
        ASSERT(err==MPI_SUCCESS);
        return data;
    #else
        return std::vector<TYPE>();
    #endif
}


/***********************************************************************
* Inline functions to set or unset the ith bit of the bit array trace  *
***********************************************************************/
static inline void set_trace_bit( unsigned int i, unsigned int N, size_t *trace ) {
    unsigned int j = i/ARCH_SIZE;
    unsigned int k = i%ARCH_SIZE;
    size_t mask = ((size_t)0x1)<<k;
    if ( i < N*ARCH_SIZE )
        trace[j] |= mask;
}
static inline void unset_trace_bit( unsigned int i, unsigned int N, size_t *trace ) {
    unsigned int j = i/ARCH_SIZE;
    unsigned int k = i%ARCH_SIZE;
    size_t mask = ((size_t)0x1)<<k;
    if ( i < N*ARCH_SIZE )
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
        int N = std::max(4,(N_bits+5)/6);    // The number of digits we need to use
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


/***********************************************************************
* TraceResults                                                         *
***********************************************************************/
TraceResults::TraceResults( ):
    N_active(0), thread(0), rank(0), N_trace(0), 
    min(1e100), max(0), tot(0), N(0), mem(NULL)
{
}
TraceResults::~TraceResults( )
{
    delete [] reinterpret_cast<double*>(mem);
    mem = NULL;
}
TraceResults::TraceResults(const TraceResults& rhs):
    id(rhs.id), N_active(rhs.N_active), thread(rhs.thread), 
    rank(rhs.rank), N_trace(rhs.N_trace), min(rhs.min), 
    max(rhs.max), tot(rhs.tot), N(rhs.N), mem(NULL)
{
    allocate();
    if ( mem!=NULL ) {
        size_t N_mem = (N_active*sizeof(id_struct))/sizeof(double)+1 + 2*N_trace;
        size_t N_bytes = N_mem*sizeof(double);
        memcpy(mem,rhs.mem,N_bytes);
    }
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
    if ( mem!=NULL ) {
        size_t N_mem = (this->N_active*sizeof(id_struct))/sizeof(double)+1 + 2*this->N_trace;
        size_t N_bytes = N_mem*sizeof(double);
        memcpy(this->mem,rhs.mem,N_bytes);
    }
    return *this;
}
void TraceResults::allocate( )
{
    delete [] reinterpret_cast<double*>(mem);
    mem = NULL;
    if ( N_active>0 || N_trace>0 ) {
        size_t bytes = N_active*sizeof(id_struct) + 2*N_trace*sizeof(double);
        mem = new double[bytes/sizeof(double)+1];
        memset(mem,0,bytes);
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
    size_t offset = (N_active*sizeof(id_struct))/sizeof(double)+1;
    return N_trace>0 ? &reinterpret_cast<double*>(mem)[offset]:NULL;
}
const double* TraceResults::start( ) const
{
    size_t offset = (N_active*sizeof(id_struct))/sizeof(double)+1;
    return N_trace>0 ? &reinterpret_cast<const double*>(mem)[offset]:NULL;
}
double* TraceResults::stop( )
{
    size_t offset = (N_active*sizeof(id_struct))/sizeof(double)+1 + N_trace;
    return N_trace>0 ? &reinterpret_cast<double*>(mem)[offset]:NULL;
}
const double* TraceResults::stop( ) const
{
    size_t offset = (N_active*sizeof(id_struct))/sizeof(double)+1 + N_trace;
    return N_trace>0 ? &reinterpret_cast<const double*>(mem)[offset]:NULL;
}
size_t TraceResults::size( bool store_trace ) const
{
    size_t bytes = sizeof(TraceResults);
    bytes += N_active*sizeof(id_struct);
    if ( store_trace )
        bytes += 2*N_trace*sizeof(double);
    return bytes;
}
void TraceResults::pack( void* data_out ) const
{
    char *data = reinterpret_cast<char*>(data_out);
    memcpy(data,this,sizeof(TraceResults));
    if ( N_trace > 0 ) {
        size_t pos = sizeof(TraceResults);
        memcpy(&data[pos],start(),N_active);
        pos += N_trace*sizeof(double);
        memcpy(&data[pos],stop(),N_active);
    }
}
void TraceResults::unpack( const void* data_in )
{
    const char *data = reinterpret_cast<const char*>(data_in);
    delete [] reinterpret_cast<double*>(mem);
    memcpy(this,data,sizeof(TraceResults));
    mem = NULL;
    allocate();
    if ( N_trace > 0 ) {
        size_t pos = sizeof(TraceResults);
        memcpy(start(),&data[pos],N_active);
        pos += N_trace*sizeof(double);
        memcpy(stop(),&data[pos],N_active);
    }
}


/***********************************************************************
* TimerResults                                                         *
***********************************************************************/
size_t TimerResults::size( bool store_trace ) const
{
    size_t bytes = sizeof(id);              // id
    bytes += sizeof(int) + message.size();  // message
    bytes += sizeof(int) + file.size();     // file
    bytes += sizeof(int) + path.size();     // path
    bytes += 2*sizeof(int);                 // start/stop
    bytes += sizeof(int);                   // trace
    for (size_t i=0; i<trace.size(); i++)
        bytes += trace[i].size(store_trace);
    return bytes;
}
void TimerResults::pack( void* data_out ) const
{
    char *data = reinterpret_cast<char*>(data_out);
    memcpy(data,&id,sizeof(id));
    size_t pos = sizeof(id);
    int *tmp = reinterpret_cast<int*>(&data[pos]);
    tmp[0] = message.size();
    tmp[1] = file.size();
    tmp[2] = path.size();
    tmp[3] = start;
    tmp[4] = stop;
    tmp[5] = trace.size();
    pos += 6*sizeof(int);
    memcpy(&data[pos],message.c_str(),tmp[0]);
    pos += message.size();
    memcpy(&data[pos],file.c_str(),tmp[1]);
    pos += file.size();
    memcpy(&data[pos],path.c_str(),tmp[2]);
    pos += path.size();
    for (size_t i=0; i<trace.size(); i++) {
        trace[i].pack(&data[pos]);
        pos += trace[i].size();
    }
}
void TimerResults::unpack( const void* data_in )
{
    const char *data = reinterpret_cast<const char*>(data_in);
    memcpy(&id,data,sizeof(id));
    size_t pos = sizeof(id);
    const int *tmp = reinterpret_cast<const int*>(&data[pos]);
    pos += 6*sizeof(int);
    message = std::string(&data[pos],tmp[0]);
    pos += message.size();
    file = std::string(&data[pos],tmp[1]);
    pos += file.size();
    path = std::string(&data[pos],tmp[2]);
    pos += path.size();
    start = tmp[3];
    start = tmp[4];
    trace.resize(tmp[5]);
    for (size_t i=0; i<trace.size(); i++) {
        trace[i].unpack(&data[pos]);
        pos += trace[i].size();
    }
}


/***********************************************************************
* TimerResults                                                         *
***********************************************************************/
size_t MemoryResults::size( ) const
{
    size_t N_bytes = 2*sizeof(size_t);
    N_bytes += time.size()*sizeof(double);
    N_bytes += bytes.size()*sizeof(size_t);
    return N_bytes;
}
void MemoryResults::pack( void* data_out ) const
{
    char *data = reinterpret_cast<char*>(data_out);
    size_t *tmp = reinterpret_cast<size_t*>(data);
    tmp[0] = static_cast<size_t>(rank);
    tmp[1] = time.size();
    if ( !time.empty() ) {
        size_t pos = 2*sizeof(size_t);
        memcpy(&data[pos],&time[0],tmp[1]*sizeof(double));
        pos += time.size()*sizeof(double);
        memcpy(&data[pos],&bytes[0],tmp[1]*sizeof(size_t));
    }
}
void MemoryResults::unpack( const void* data_in )
{
    const char *data = reinterpret_cast<const char*>(data_in);
    const size_t *tmp = reinterpret_cast<const size_t*>(data);
    rank = static_cast<int>(tmp[0]);
    size_t N = tmp[1];
    time.resize(N);
    bytes.resize(N);
    if ( !time.empty() ) {
        size_t pos = 2*sizeof(size_t);
        memcpy(&time[0],&data[pos],tmp[1]*sizeof(double));
        pos += time.size()*sizeof(double);
        memcpy(&bytes[0],&data[pos],tmp[1]*sizeof(size_t));
    }
}


/***********************************************************************
* Consructor                                                           *
***********************************************************************/
ProfilerApp::ProfilerApp() {
    if ( 8*sizeof(size_t) != ARCH_SIZE )
        ERROR_MSG("Incorrectly identified architecture?\n");
    if ( sizeof(id_struct)!=8 )
        ERROR_MSG("id_struct is an unexpected size\n");
    get_frequency( &d_frequency );
    #ifdef USE_WINDOWS
        lock = CreateMutex (NULL, FALSE, NULL);
    #elif defined(USE_LINUX) || defined(USE_MAC)
        pthread_mutex_init (&lock,NULL);
    #else
        #error Unknown OS
    #endif
    for (int i=0; i<THREAD_HASH_SIZE; i++)
        thread_head[i] = NULL;
    for (int i=0; i<TIMER_HASH_SIZE; i++)
        timer_table[i] = NULL;
    get_time(&d_construct_time);
    N_threads = 0;
    N_timers = 0;
    d_level = 0;
    d_shift = 0.0;
    d_store_trace_data = false;
    d_store_memory_data = false;
    d_max_trace_remaining = static_cast<size_t>(MAX_TRACE_MEMORY);
    d_N_memory_steps = 0;
    d_time_memory = NULL;
    d_size_memory = NULL;
}
void ProfilerApp::set_store_trace( bool profile ) { 
    if ( N_timers==0 ) 
        d_store_trace_data = profile;
    else
        ERROR_MSG("Cannot change trace status after a timer is started\n");
}
void ProfilerApp::set_store_memory( bool memory ) { 
    if ( N_timers==0 ) 
        d_store_memory_data = memory;
    else
        ERROR_MSG("Cannot change memory status after a timer is started\n");
}


/***********************************************************************
* Deconsructor                                                         *
***********************************************************************/
ProfilerApp::~ProfilerApp() {
    // Disable and delete the timers
    disable();
}


/***********************************************************************
* Function to syncronize the timers                                    *
***********************************************************************/
void ProfilerApp::syncronize() {
    GET_LOCK(&lock);
	comm_barrier();
    TIME_TYPE sync_time_local;
    get_time(&sync_time_local);
    double current_time = get_diff(d_construct_time,sync_time_local,d_frequency);
    double max_current_time = comm_max_reduce(current_time);
    d_shift = max_current_time - current_time;
    RELEASE_LOCK(&lock);
}


/***********************************************************************
* Function to start profiling a block of code                          *
***********************************************************************/
void ProfilerApp::start( const std::string& message, const char* filename, 
    const int line, const int level, const size_t timer_id ) 
{
    if ( level<0 || level>=128 )
        ERROR_MSG("level must be in the range 0-127");
    if ( this->d_level<level )
        return;
    #if MONITOR_PROFILER_PERFORMANCE > 0
        TIME_TYPE start_time_local;
        get_time(&start_time_local);
    #endif
    // Get the thread data
    thread_info* thread_data = get_thread_data();
    // Get the appropriate timer
    store_timer* timer = get_block(thread_data,message.c_str(),filename,timer_id,line,-1);
    if ( timer->is_active ) {
        if ( d_check_timer_error ) {
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
        size_t N = thread_data->N_memory_steps;
        size_t N_max = d_max_trace_remaining;
        // Check the memory allocation
        check_allocate_array(&thread_data->time_memory,N,N_max);
        check_allocate_array(&thread_data->size_memory,N,N_max);
        // Get the current memroy usage
        thread_data->time_memory[N] = -1.0;
        thread_data->size_memory[N] = get_memory_usage();
    }
    // Start the timer 
    memcpy(timer->trace,thread_data->active,TRACE_SIZE*sizeof(size_t));
    timer->is_active = true;
    timer->N_calls++;
    set_trace_bit(timer->trace_index,TRACE_SIZE,thread_data->active);
    get_time(&timer->start_time);
    // Record the time of the memory usage
    if ( d_store_memory_data && thread_data->N_memory_steps<d_max_trace_remaining ) {
        thread_data->time_memory[thread_data->N_memory_steps] = 
            get_diff(d_construct_time,timer->start_time,d_frequency);
        thread_data->N_memory_steps++;
    }
    #if MONITOR_PROFILER_PERFORMANCE > 0
        TIME_TYPE stop_time_local;
        get_time(&stop_time_local);
        total_start_time += get_diff(start_time_local,stop_time_local,d_frequency);
    #endif
}


/***********************************************************************
* Function to stop profiling a block of code                           *
***********************************************************************/
void ProfilerApp::stop( const std::string& message, const char* filename, 
    const int line, const int level, const size_t timer_id ) 
{
    if ( level<0 || level>=128 )
        ERROR_MSG("level must be in the range 0-127");
    if ( this->d_level<level )
        return;
    #if MONITOR_PROFILER_PERFORMANCE > 0
        TIME_TYPE start_time_local;
        get_time(&start_time_local);
    #endif
    // Use the current time (minimize the effects of the overhead of the timer)
    TIME_TYPE end_time;
    get_time(&end_time);
    // Get the thread data
    thread_info* thread_data = get_thread_data();
    // Get the appropriate timer
    store_timer* timer = get_block(thread_data,message.c_str(),filename,timer_id,-1,line);
    if ( !timer->is_active ) {
        if ( d_check_timer_error) {
            // Stop the timer before starting
            this->start( message, filename, -1, level, timer_id );
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
    size_t active[TRACE_SIZE]={0};
    for (size_t i=0; i<TRACE_SIZE; i++)
        active[i] = thread_data->active[i] & timer->trace[i];
    size_t trace_id = get_trace_id( active );
    // Find the trace to save
    store_trace *trace = timer->trace_head;
    while ( trace != NULL) {
        if ( trace_id==trace->id )
            break;
        trace = trace->next;
    }
    if ( trace == NULL ) {
        trace = new store_trace;
        memcpy(trace->trace,active,TRACE_SIZE*sizeof(size_t));
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
        check_allocate_array(&trace->start_time,trace->N_calls,static_cast<size_t>(MAX_TRACE_TRACE));
        check_allocate_array(&trace->end_time,trace->N_calls,static_cast<size_t>(MAX_TRACE_TRACE));
        // Calculate the time elapsed since the profiler was created
        trace->start_time[trace->N_calls] = get_diff(d_construct_time,timer->start_time,d_frequency);
        trace->end_time[trace->N_calls]   = get_diff(d_construct_time,end_time,d_frequency);
    }
    // Save the minimum, maximum, and total times
    timer->max_time = std::max(timer->max_time,time);
    timer->min_time = std::min(timer->min_time,time);
    timer->total_time += time;
    // Save the new time info to the trace
    trace->max_time = std::max(trace->max_time,time);
    trace->min_time = std::min(trace->min_time,time);
    trace->total_time += time;
    trace->N_calls++;
    // Get the memory usage
    if ( d_store_memory_data && thread_data->N_memory_steps<d_max_trace_remaining ) {
        size_t N = thread_data->N_memory_steps;
        size_t N_max = d_max_trace_remaining;
        // Check the memory allocation
        check_allocate_array(&thread_data->time_memory,N,N_max);
        check_allocate_array(&thread_data->size_memory,N,N_max);
        // Get the current memroy usage
        thread_data->time_memory[N] = get_diff(d_construct_time,end_time,d_frequency);
        thread_data->size_memory[N] = get_memory_usage();
        thread_data->N_memory_steps++;
    }
    #if MONITOR_PROFILER_PERFORMANCE > 0
        TIME_TYPE stop_time_local;
        get_time(&stop_time_local);
        total_stop_time += get_diff(start_time_local,stop_time_local,d_frequency);
    #endif
}


/***********************************************************************
* Function to check if a timer is active                               *
***********************************************************************/
bool ProfilerApp::active( const std::string& message, const char* filename, const size_t timer_id )
{
    thread_info* thread_data = get_thread_data();
    store_timer* timer = get_block(thread_data,message.c_str(),filename,timer_id,-1,-1);
    return timer->is_active;
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
    // Stop ALL timers
    TIME_TYPE end_time;
    get_time(&end_time);
    // Delete the thread structures
    for (int i=0; i<THREAD_HASH_SIZE; i++) {
        delete thread_head[i];
        thread_head[i] = NULL;
    }
    N_threads = 0;
    // Delete the timer data structures
    for (int i=0; i<TIMER_HASH_SIZE; i++) {
        delete timer_table[i];
        timer_table[i] = NULL;
    }
    N_timers = 0;
    // Delete the memory info
    d_max_trace_remaining = static_cast<size_t>(MAX_TRACE_MEMORY);
    d_N_memory_steps = 0;
    delete [] d_time_memory;
    delete [] d_size_memory;
    d_time_memory = NULL;
    d_size_memory = NULL;
    RELEASE_LOCK(&lock);
}


/***********************************************************************
* Function to return the profiling info                                *
***********************************************************************/
std::vector<TimerResults> ProfilerApp::getTimerResults() const
{
    // Get the current time in case we need to "stop" and timers
    TIME_TYPE end_time;
    get_time(&end_time);
    int rank = comm_rank();
    // Get the mutex for thread safety (we don't want the list changing while we are saving the data)
    // Note: Because we don't block for most operations in the timer this is not full proof but should help
    bool error = GET_LOCK(&lock);
    if ( error )
        return std::vector<TimerResults>();
    // Get the thread specific data for each thread
    int N_threads2 = N_threads;     // Cache the number of threads since we are holing the lock
    thread_info **thread_data = new thread_info*[N_threads2];
    for (int i=0; i<N_threads2; i++)
        thread_data[i] = NULL;
    for (int i=0; i<THREAD_HASH_SIZE; i++) {
        // It is safe to case to a non-volatile object since we hold the lock
        thread_info *ptr = const_cast<thread_info*>(thread_head[i]);  
        while ( ptr != NULL ) {
            if ( ptr->thread_num >= N_threads2 )
                ERROR_MSG("Internal error (1)");
            if ( thread_data[ptr->thread_num] != NULL )
                ERROR_MSG("Internal error (2)");
            thread_data[ptr->thread_num] = ptr;
            ptr = const_cast<thread_info*>(ptr->next);
        }
    }
    for (int i=0; i<N_threads2; i++) {
        if ( thread_data[i] == NULL ) {
            delete [] thread_data;
            RELEASE_LOCK(&lock);
            ERROR_MSG("Internal error (3)");
        }
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
    for (size_t i=0; i<ids.size(); i++) {
        const size_t id = ids[i];
        const size_t key = GET_TIMER_HASH( id );
        results[i].id = convert_timer_id(id);
        // Search for the global timer info
        store_timer_data_info *timer_global = const_cast<store_timer_data_info*>(timer_table[key]);
        while ( timer_global!=NULL ) {
            if ( timer_global->id == id ) 
                break;
            timer_global = const_cast<store_timer_data_info*>(timer_global->next);
        }
        if ( timer_global==NULL ) {
            delete [] thread_data;
            RELEASE_LOCK(&lock);
            ERROR_MSG("Internal error");
        }
        // Copy the basic timer info
        results[i].message = timer_global->message;
        results[i].file = timer_global->filename;
        results[i].path = timer_global->path;
        results[i].start = timer_global->start_line;
        results[i].stop = timer_global->stop_line;
        // Loop through the thread entries
        for (int thread_id=0; thread_id<N_threads2; thread_id++) {
            thread_info *head = thread_data[thread_id];
            // Search for a timer that matches the current id
            store_timer* timer = head->head[key];
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
            size_t active[TRACE_SIZE];
            if ( timer->is_active ) {
                add_trace = true;
                time = get_diff(timer->start_time,end_time,d_frequency);
                // The timer is only a calling timer if it was active before and after the current timer
                for (size_t i=0; i<TRACE_SIZE; i++)
                    active[i] = head->active[i] & timer->trace[i];
                unset_trace_bit(timer->trace_index,TRACE_SIZE,active);
                trace_id = get_trace_id( active );
            }
            // Loop through the trace entries
            store_trace *trace = timer->trace_head;
            while ( trace != NULL ) {
                size_t k = results[i].trace.size();
                results[i].trace.resize(k+1);
                // Get the running times of the trace
                size_t N_stored_trace = 0;
                if ( d_store_trace_data ) 
                    N_stored_trace = std::min(trace->N_calls,static_cast<size_t>(MAX_TRACE_TRACE));
                std::vector<id_struct> list = get_active_list( trace->trace, timer->trace_index, head );
                results[i].trace[k].id = results[i].id;
                results[i].trace[k].thread = thread_id;
                results[i].trace[k].rank = rank;
                results[i].trace[k].N = trace->N_calls;
                results[i].trace[k].N_active = list.size();
                results[i].trace[k].N_trace = N_stored_trace;
                results[i].trace[k].min = trace->min_time;
                results[i].trace[k].max = trace->max_time;
                results[i].trace[k].tot = trace->total_time;
                results[i].trace[k].allocate();
                for (size_t j=0; j<list.size(); j++)
                    results[i].trace[k].active()[j] = list[j];
                // Determine if we need to add the running trace
                if ( add_trace ) {
                    if ( trace_id == trace->id ) {
                        results[i].trace[k].min = std::min<float>(results[i].trace[k].min,time);
                        results[i].trace[k].max = std::max<float>(results[i].trace[k].max,time);
                        results[i].trace[k].tot += time;
                        add_trace = false;
                    }
                }
                // Save the detailed trace results (this is a binary file)
                double *start = results[i].trace[k].start();
                double *stop  = results[i].trace[k].stop();
                for (size_t m=0; m<N_stored_trace; m++) {
                    start[m] = trace->start_time[m] + d_shift;
                    stop[m]  = trace->end_time[m]   + d_shift;
                }
                // Advance to the next trace
                trace = trace->next;
            }
            // Create a new trace if necessary
            if ( add_trace ) { 
                size_t k = results[i].trace.size();
                results[i].trace.resize(k+1);
                size_t N_stored_trace = 0;
                if ( d_store_trace_data ) 
                    N_stored_trace = 1;
                std::vector<id_struct> list = get_active_list( active, timer->trace_index, head );
                results[i].trace[k].id = results[i].id;
                results[i].trace[k].thread = thread_id;
                results[i].trace[k].rank = rank;
                results[i].trace[k].N = 1;
                results[i].trace[k].N_active = list.size();
                results[i].trace[k].N_trace = N_stored_trace;
                results[i].trace[k].min = time;
                results[i].trace[k].max = time;
                results[i].trace[k].tot = time;
                results[i].trace[k].allocate();
                for (size_t j=0; j<list.size(); j++)
                    results[i].trace[k].active()[j] = list[j];
                if ( d_store_trace_data ) { 
                    double *start = results[i].trace[k].start();
                    double *stop  = results[i].trace[k].stop();
                    start[0] = get_diff(d_construct_time,timer->start_time,d_frequency) + d_shift;
                    stop[0]  = get_diff(d_construct_time,end_time,d_frequency) + d_shift;
                }
            }
        }
    }
    // Release the mutex
    RELEASE_LOCK(&lock);
    return results;
}


/***********************************************************************
* Function to return the memory usage as a function of time            *
***********************************************************************/
MemoryResults ProfilerApp::getMemoryResults() const
{
    // Get the mutex for thread safety (we don't want the list changing while we are saving the data)
    // Note: Because we don't block for most operations in the timer this is not full proof but should help
    MemoryResults data;
    data.rank = comm_rank();
    bool error = GET_LOCK(&lock);
    if ( error )
        return data;
    // First unify the memory info from the different threads
    std::vector<size_t> N_time;
    std::vector<double*> data_time;
    std::vector<size_t*> size_time;
    for (int i=0; i<THREAD_HASH_SIZE; i++) {
        volatile thread_info *thread_data = thread_head[i];
        while ( thread_data != NULL ) {
            // Copy the pointers so that we minimize the chance of the data being modified
            size_t N = thread_data->N_memory_steps;
            if ( N>0 ) { 
                double* time = thread_data->time_memory;
                size_t* size = thread_data->size_memory;
                thread_data->N_memory_steps = 0;
                thread_data->time_memory = NULL;
                thread_data->size_memory = NULL;
                N_time.push_back(N);
                data_time.push_back(time);
                size_time.push_back(size);
            }
            thread_data = thread_data->next;
        }
    }
    if ( d_N_memory_steps>0 ) {
        N_time.push_back(d_N_memory_steps);
        data_time.push_back(d_time_memory);
        size_time.push_back(d_size_memory);
    }
    mergeArrays<double,size_t>( N_time.size(), &N_time[0], &data_time[0], &size_time[0],
        &d_N_memory_steps, &d_time_memory, &d_size_memory );
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
    d_max_trace_remaining = std::max((size_t)MAX_TRACE_MEMORY-d_N_memory_steps,(size_t)0);
    // Release the mutex
    RELEASE_LOCK(&lock);
    // Copy the results to the output vector
    data.time.resize(d_N_memory_steps);
    data.bytes.resize(d_N_memory_steps);
    for (size_t i=0; i<d_N_memory_steps; i++) {
        data.time[i]  = d_time_memory[i];
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
            std::vector<double> time_thread(N_threads,0);
            for (size_t j=0; j<results[i].trace.size(); j++)
                time_thread[results[i].trace[j].thread] += results[i].trace[j].tot;
            for (int j=0; j<N_threads; j++)
                total_time[i] = std::max(total_time[i],time_thread[j]);
        }
        quicksort2(N_timers,&total_time[0],&id_order[0]);
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
        for (int ii=N_timers-1; ii>=0; ii--) {
            size_t i=id_order[ii];
            std::vector<int> N_thread(N_threads,0);
            std::vector<float> min_thread(N_threads,1e200);
            std::vector<float> max_thread(N_threads,0);
            std::vector<double> tot_thread(N_threads,0);
            for (size_t j=0; j<results[i].trace.size(); j++) {
                int k = results[i].trace[j].thread;
                N_thread[k] += results[i].trace[j].N;
                min_thread[k] = std::min(min_thread[k],results[i].trace[j].min);
                max_thread[k] = std::max(max_thread[k],results[i].trace[j].max);
                tot_thread[k] += results[i].trace[j].tot;
            }
            for (int j=0; j<N_threads; j++) {
                if ( N_thread[j]==0 )
                    continue;
                // Save the timer to the file
                fprintf(timerFile,"%30s  %30s   %4i   %7i    %7i  %8i     %8.3f  %8.3f  %10.3f\n",
                    results[i].message.c_str(),results[i].file.c_str(),j,results[i].start,
                    results[i].stop,N_thread[j],min_thread[j],max_thread[j],tot_thread[j]);
            }
        }
        // Loop through all of the entries, saving the detailed data and the trace logs
        fprintf(timerFile,"\n\n");
        fprintf(timerFile,"<N_procs=%i,id=%i",N_procs,rank);
        fprintf(timerFile,",store_trace=%i",d_store_trace_data?1:0);
        fprintf(timerFile,",store_memory=%i",d_store_memory_data?1:0);
        fprintf(timerFile,",date='%s'>\n",getDateString().c_str());
        // Loop through the list of timers, storing the most expensive first
        for (int ii=N_timers-1; ii>=0; ii--) {
            size_t i=id_order[ii];
            // Store the basic timer info
            fprintf(timerFile,"<timer:id=%s,message=%s,file=%s,path=%s,start=%i,stop=%i>\n",
                results[i].id.c_str(),results[i].message.c_str(),results[i].file.c_str(),
                results[i].path.c_str(),results[i].start,results[i].stop);
            // Store the trace data
            for (size_t j=0; j<results[i].trace.size(); j++) {
                const TraceResults& trace = results[i].trace[j];
                std::string active;
                for (size_t k=0; k<trace.N_active; k++)
                    active += trace.active()[k].string() + " ";
                fprintf(timerFile,"<trace:id=%s,thread=%i,rank=%i,N=%lu,min=%e,max=%e,tot=%e,active=[ %s]>\n",
                    trace.id.c_str(),trace.thread,trace.rank,trace.N,trace.min,trace.max,trace.tot,active.c_str());
                // Save the detailed trace results (this is a binary file)
                if ( trace.N_trace > 0 ) { 
                    fprintf(traceFile,"<id=%s,thread=%i,rank=%i,active=[ %s],N=%lu>\n",
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
            // Determine a scale factor so we can use unsigned int to store the memory
            size_t max_mem_size = 0;
            for (size_t i=0; i<data[k].bytes.size(); i++)
                max_mem_size = std::max(max_mem_size,data[k].bytes[i]);
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
            double *time = new double[data[k].time.size()];
            unsigned int *size = new unsigned int[data[k].bytes.size()];
            for (size_t i=0; i<data[k].time.size(); i++) {
                time[i] = data[k].time[i];
                size[i] = static_cast<unsigned int>(data[k].bytes[i]/scale);
            }
            // Save the results
            ASSERT(sizeof(unsigned int)==4);
            fprintf(memoryFile,"<N=%zi,type1=%s,type2=%s,units=%s,rank=%i>\n",
                data[k].time.size(),"double","uint32",units.c_str(),rank);
            size_t N1 = fwrite(time,sizeof(double),d_N_memory_steps,memoryFile);
            size_t N2 = fwrite(size,sizeof(unsigned int),d_N_memory_steps,memoryFile);
            fprintf(memoryFile,"\n");
            delete [] time;
            delete [] size;
            if ( N1!=(size_t)d_N_memory_steps || N2!=(size_t)d_N_memory_steps )
                ERROR_MSG("Failed to write memory results\n");
        }
        fclose(memoryFile);
    }
    #if MONITOR_PROFILER_PERFORMANCE > 0
        printp("start = %e, stop = %e, block = %e, thread = %e, trace_id = %e\n",
            total_start_time,total_stop_time,total_block_time,total_thread_time,total_trace_id_time);
    #endif
}


/***********************************************************************
* Load the timer and trace data                                        *
***********************************************************************/
static inline void get_field( const char* line, const char* name, char* data )
{
    const char* ptr = strstr( line, name );
    if ( ptr==NULL ) {
        data[0] = 0;
    } else {
        int i1 = -1;
        int i2 = -1;
        for (size_t i=0; i<1000; i++) {
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
static std::vector<id_struct> get_active_ids( const char* active_list )
{
    std::vector<id_struct> ids;
    size_t i1 = 0;
    while ( active_list[i1] != 0 ) {
        if ( active_list[i1]==' ' || active_list[i1]=='[' || active_list[i1]==']' ) {
            i1++;
            continue;
        }
        size_t i2 = i1+1;
        while ( active_list[i2]!=' ' && active_list[i2]!=']' && active_list[i2]!=0 )
            i2++;
        char tmp[10]={0};
        memcpy(tmp,&active_list[i1],i2-i1);
        tmp[i2-i1] = 0;
        ids.push_back(id_struct(tmp));
        i1 = i2;
    }
    return ids;
}
TimerMemoryResults ProfilerApp::load( const std::string& filename, int rank )
{
    TimerMemoryResults data;
    data.timers.clear();
    data.memory.clear();
    int N_procs;
    std::string date;
    bool trace_data, memory_data;
    char timer[200], trace[200], memory[200];
    if ( rank==-1 ) {
        sprintf(timer,"%s.1.timer",filename.c_str());
        sprintf(trace,"%s.1.trace",filename.c_str());
        sprintf(memory,"%s.1.memory",filename.c_str());
        load_timer(timer,data.timers,N_procs,date,trace_data,memory_data);
        if ( trace_data )
            load_trace(trace,data.timers);
        if ( memory_data )
            load_memory(memory,data.memory);
        for (int i=1; i<N_procs; i++) {
            sprintf(timer,"%s.%i.timer",filename.c_str(),i+1);
            sprintf(trace,"%s.%i.trace",filename.c_str(),i+1);
            sprintf(memory,"%s.%i.memory",filename.c_str(),i+1);
            load_timer(timer,data.timers,N_procs,date,trace_data,memory_data);
            if ( trace_data )
                load_trace(trace,data.timers);
            if ( memory_data )
                load_memory(memory,data.memory);
        }
    } else {
        sprintf(timer,"%s.%i.timer",filename.c_str(),rank+1);
        sprintf(trace,"%s.%i.trace",filename.c_str(),rank+1);
        sprintf(memory,"%s.%i.memory",filename.c_str(),rank+1);
        load_timer(timer,data.timers,N_procs,date,trace_data,memory_data);
        if ( trace_data )
            load_trace(trace,data.timers);
        if ( memory_data )
            load_memory(memory,data.memory);
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
    }
    char *buffer = new char[file_length+10];
    memset(buffer,0,file_length+10);
    rewind(fid);
    size_t result = fread(buffer,1,file_length,fid);
    if (result!=file_length) {
        delete [] buffer;
        ERROR_MSG("error reading file");
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
    size_t pos = 0;
    char field[128];
    memset(field,0,128);
    while ( pos<file_length ) {
        char *line = &buffer[pos];
        int length = 0;
        while ( line[length]>=32 )
            ++length;
        line[length] = 0;
        if ( strncmp(line,"<N_procs=",9)==0 ) {
            // Load the number of processors
            get_field(line,"N_procs=",field);
            N_procs = atoi(field);
            if ( N_procs==0 )
                ERROR_MSG("Error reading N_procs");
            // Load the id/rank
            get_field(line,"id=",field);
            if ( field[0] != 0 )
                rank = atoi(field);
            get_field(line,"rank=",field);
            if ( field[0] != 0 )
                rank = atoi(field);
            // Check if we stored the trace file (optional)
            get_field(line,"store_trace=",field);
            if ( field[0] != 0 )
                trace_data = atoi(field)==1;
            // Check if we stored the memory file (optional)
            get_field(line,"store_memory=",field);
            if ( field[0] != 0 )
                memory_data = atoi(field)==1;
            // Load the date (optional)
            get_field(line,"date=",field);
            if ( field[0] != 0 )
                date = std::string(field);
        } else if ( strncmp(line,"<timer:",7)==0 ) {
            // Load the id
            get_field(line,"id=",field);
            ASSERT(field[0]!=0);
            id_struct id( field );
            // Find the timer
            std::map<id_struct,size_t>::iterator it = id_map.find(id);
            if ( it==id_map.end() ) {
                // Create a new timer
                size_t k = data.size();
                id_map.insert( std::pair<id_struct,size_t>(id,k) );
                data.resize(k+1);
                TimerResults& timer = data[k];
                timer.id = id;
                // Load the message
                get_field(line,"message=",field);
                ASSERT(field[0]!=0);
                timer.message = std::string(field);
                // Load the filename
                get_field(line,"file=",field);
                ASSERT(field[0]!=0);
                timer.file = std::string(field);
                // Load the path
                get_field(line,"path=",field);
                if ( field[0]!=0 )
                    timer.path = std::string(field);
                // Load the start line
                get_field(line,"start=",field);
                ASSERT(field[0]!=0);
                timer.start = atoi(field);
                // Load the stop line
                get_field(line,"stop=",field);
                ASSERT(field[0]!=0);
                timer.stop = atoi(field);
            }
        } else if ( strncmp(line,"<trace:",7)==0 ) {
            // Load a trace
            get_field(line,"id=",field);
            ASSERT(field[0]!=0);
            id_struct id( field );
            std::map<id_struct,size_t>::iterator it = id_map.find(id);
            if ( it==id_map.end() )
                ERROR_MSG("trace did not find matching timer");
            size_t index = it->second;
            data[index].trace.resize(data[index].trace.size()+1);
            TraceResults& trace = data[index].trace.back();
            // Load the id
            trace.id = id;
            // Load the thread id
            get_field(line,"thread=",field);
            ASSERT(field[0]!=0);
            trace.thread = atoi(field);
            // Load the rank id
            get_field(line,"rank=",field);
            if ( field[0]!=0 )
                trace.rank = atoi(field);
            else
                trace.rank = rank;
            ASSERT(trace.rank>=0&&static_cast<int>(trace.rank)<N_procs);
            // Load N
            get_field(line,"N=",field);
            ASSERT(field[0]!=0);
            trace.N = atoi(field);
            // Load min
            get_field(line,"min=",field);
            ASSERT(field[0]!=0);
            trace.min = atof(field);
            // Load max
            get_field(line,"max=",field);
            ASSERT(field[0]!=0);
            trace.max = atof(field);
            // Load tot
            get_field(line,"tot=",field);
            ASSERT(field[0]!=0);
            trace.tot = atof(field);
            // Load the active timers
            get_field(line,"active=",field);
            ASSERT(field[0]!=0);
            std::vector<id_struct> active = get_active_ids(field);
            trace.N_active = active.size();
            // Load the trace data
            trace.N_trace = 0;
            // Save the active timers and trace data
            trace.allocate();
            for (size_t i=0; i<active.size(); i++)
                trace.active()[i] = active[i];
        }
        pos += length+1;
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
    while ( 1 ) {
        // Read the header
        char line[512], field[128];
        memset(line,0,512);
        memset(field,0,128);
        char *rtn = fgets(line,512,fid);
        if ( rtn==NULL )
            break;
        if ( line[0] <= 10 )
            continue;
        // Get the id and find the appropriate timer
        get_field(line,"id=",field);
        ASSERT(field[0]!=0);
        id_struct id( field );
        std::map<id_struct,size_t>::iterator it = id_map.find(id);
        if ( it==id_map.end() )
            ERROR_MSG("Did not find matching timer");
        TimerResults& timer = data[it->second];
        // Read the remaining trace header data
        get_field(line,"thread=",field);
        ASSERT(field[0]!=0);
        unsigned int thread = static_cast<unsigned int>(atoi(field));
        get_field(line,"rank=",field);
        ASSERT(field[0]!=0);
        unsigned int rank = static_cast<unsigned int>(atoi(field));
        get_field(line,"active=",field);
        ASSERT(field[0]!=0);
        std::vector<id_struct> active = get_active_ids(field);
        get_field(line,"N=",field);
        ASSERT(field[0]!=0);
        unsigned long int N = strtoul(field,NULL,10);
        // Find the appropriate trace
        int index = -1;
        for (size_t i=0; i<timer.trace.size(); i++) {
            if ( timer.trace[i].thread != thread ||
                 timer.trace[i].rank != rank ||
                 timer.trace[i].N_active != active.size() )
                continue;
            bool active_match = true;
            for (size_t j=0; j<active.size(); j++) {
                if ( active[j] != timer.trace[i].active()[j] )
                    active_match = false;
            }
            if ( !active_match )
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
        fread(field,1,1,fid);
    }
    fclose(fid);
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
    while ( 1 ) {
        // Read the header
        char line[512], field[128];
        char *rtn = fgets(line,512,fid);
        if ( rtn==NULL )
            break;
        if ( line[0] <= 10 )
            continue;
        data.resize(data.size()+1);
        MemoryResults& memory = data.back();
        // Get the header fields
        get_field(line,"N=",field);
        ASSERT(field[0]!=0);
        unsigned long int N = strtoul(field,NULL,10);
        get_field(line,"type1=",field);
        ASSERT(field[0]!=0);
        std::string type1(field);
        get_field(line,"type2=",field);
        ASSERT(field[0]!=0);
        std::string type2(field);
        get_field(line,"units=",field);
        ASSERT(field[0]!=0);
        size_t scale = get_scale(field);
        get_field(line,"rank=",field);
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
}


/***********************************************************************
* Function to get the list of active timers                            *
***********************************************************************/
std::vector<id_struct> ProfilerApp::get_active_list( size_t *active, unsigned int myIndex, thread_info *head )
{
    unsigned int size_t_size = 8*sizeof(size_t);
    std::vector<id_struct> active_list;
    for (unsigned int i=0; i<TRACE_SIZE; i++) {
        for (unsigned int j=0; j<size_t_size; j++) {
            unsigned int k = i*size_t_size + j;
            if ( k == myIndex )
                continue;
            size_t mask = ((size_t)0x1)<<j;
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


/************************************************************************
* Function to get the data for the current thread                       *
* Note:  If a thread has called this function at some time in the past  *
* then it will be able to return without blocking. When a thread enters *
*  this function for the first time then it will block as necessary.    *
***********************************************************************/
ProfilerApp::thread_info* ProfilerApp::get_thread_data( ) 
{
    #if MONITOR_PROFILER_PERFORMANCE > 1
        TIME_TYPE start_time_local;
        get_time(&start_time_local);
    #endif
    // Get the thread id (as an integer)
    #ifdef USE_WINDOWS
        DWORD tmp_thread_id = GetCurrentThreadId();
        size_t thread_id = (size_t) tmp_thread_id;
    #elif defined(USE_LINUX) || defined(USE_MAC)
        pthread_t tmp_thread_id = pthread_self();
        size_t thread_id = (size_t) tmp_thread_id;
    #else
        #error Unknown OS
    #endif
    // Get the hash key for the thread
    size_t key = GET_THREAD_HASH( thread_id );
    // Find the first entry with the given key (creating one if necessary)
    if ( thread_head[key]==NULL ) {
        // The entry in the hash table is empty
        // Acquire the lock
        bool error = GET_LOCK(&lock);
        if ( error )
            return NULL;
        // Check if the entry is still NULL
        if ( thread_head[key]==NULL ) {
            // Create a new entry
            thread_head[key] = new thread_info;
            thread_head[key]->id = thread_id;
            thread_head[key]->N_timers = 0;
            thread_head[key]->next = NULL;
            thread_head[key]->thread_num = N_threads;
            N_threads++;
        }
        // Release the lock
        RELEASE_LOCK(&lock);
    }
    volatile thread_info* head = thread_head[key];
    // Find the entry by looking through the list (creating the entry if necessary)
    while ( head->id != thread_id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( head->next==NULL ) {
            // Acquire the lock
            bool error = GET_LOCK(&lock);
            if ( error )
                return NULL;
            // Check if another thread created an entry while we were waiting for the lock
            if ( head->next==NULL ) {
                // Create a new entry
                thread_info* new_data = new thread_info;
                new_data->id = thread_id;
                new_data->N_timers = 0;
                new_data->next = NULL;
                new_data->thread_num = N_threads;
                N_threads++;
                head->next = new_data;
            }
            // Release the lock
            RELEASE_LOCK(&lock);
        } 
        // Advance to the next entry
        head = head->next;
    }
    // Return the pointer (Note: we no longer need volatile since we are accessing it from the creating thread)
    #if MONITOR_PROFILER_PERFORMANCE > 1
        TIME_TYPE stop_time_local;
        get_time(&stop_time_local);
        total_thread_time += get_diff(start_time_local,stop_time_local,frequency);
    #endif
    return const_cast<thread_info*>(head);
}


/***********************************************************************
* Function to get the timmer for a particular block of code            *
* Note: This function performs some blocking as necessary.             *
***********************************************************************/
inline ProfilerApp::store_timer* ProfilerApp::get_block( thread_info *thread_data, 
    const char* message, const char* filename, size_t id, const int start, const int stop ) 
{
    #if MONITOR_PROFILER_PERFORMANCE > 1
        TIME_TYPE start_time_local;
        get_time(&start_time_local);
    #endif
    if ( id==0 )
        id = get_timer_id(message,filename);
    // Get the id for the timer
    size_t key = GET_TIMER_HASH( id );    // Get the hash index
    // Search for the thread-specific timer and create it if necessary (does not need blocking)
    if ( thread_data->head[key]==NULL ) {
        // The timer does not exist, create it
        store_timer *new_timer = new store_timer;
        new_timer->id = id;
        new_timer->is_active = false;
        new_timer->trace_index = thread_data->N_timers;
        thread_data->N_timers++;
        thread_data->head[key] = new_timer;
    }
    store_timer *timer = thread_data->head[key];
    while ( timer->id != id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( timer->next==NULL ) {
            store_timer *new_timer = new store_timer;
            new_timer->id = id;
            new_timer->is_active = false;
            new_timer->trace_index = thread_data->N_timers;
            thread_data->N_timers++;
            timer->next = new_timer;
        } 
        // Advance to the next entry
        timer = timer->next;
    }
    // Get the global timer info and create if necessary
    store_timer_data_info* global_info = timer->timer_data;
    if ( global_info == NULL ) {
        global_info = get_timer_data( id );
        timer->timer_data = global_info;
        if ( global_info->start_line==-2 ) {
            const char* filename2 = strip_path(filename);
            global_info->start_line = start;
            global_info->stop_line = stop;
            global_info->message = std::string(message);
            global_info->filename = std::string(filename2);
            global_info->path = std::string(filename,0,filename2-filename);
        }
    }
    // Check the status of the timer
    int global_start = global_info->start_line;
    int global_stop = global_info->stop_line;
    if ( ( start!=-1 && global_start==-1 ) || ( stop!=-1 && global_stop==-1 ) ) {
        // The global timer is incomplete, modify accordingly
        // Note:  Technically this should be a blocking call, however it
        //        is possible to update the start/stop lines directly.  
        if ( start!=-1 && global_start==-1 )
            global_info->start_line = start;
        if ( stop!=-1 && global_stop==-1 )
            global_info->stop_line = stop;
        global_start = global_info->start_line;
        global_stop = global_info->stop_line;
    }
    if ( start!=-1 && global_start!=start ) {
        // Multiple start lines were detected indicating duplicate timers
        std::stringstream msg;
        msg << "Multiple start calls with the same message are not allowed ("
            << message << " in " << filename << " at lines " 
            << start << ", " << global_info->start_line << ")\n";
        ERROR_MSG(msg.str());
    }
    if ( stop!=-1 && global_stop!=stop ) {
        // Multiple start lines were detected indicating duplicate timers
        std::stringstream msg;
        msg << "Multiple start calls with the same message are not allowed ("
            << message << " in " << filename << " at lines " << stop 
            << ", " << global_info->stop_line << ")\n";
        ERROR_MSG(msg.str());
    }
    #if MONITOR_PROFILER_PERFORMANCE > 1
        TIME_TYPE stop_time_local;
        get_time(&stop_time_local);
        total_block_time += get_diff(start_time_local,stop_time_local,frequency);
    #endif
    return timer;
}


/***********************************************************************
* Function to return a pointer to the global timer info and create it  *
* if necessary.                                                        *
***********************************************************************/
ProfilerApp::store_timer_data_info* ProfilerApp::get_timer_data( size_t id )
{
    size_t key = GET_TIMER_HASH( id );    // Get the hash index
    if ( timer_table[key]==NULL ) {
        // The global timer does not exist, create it (requires blocking)
        // Acquire the lock
        bool error = GET_LOCK(&lock);
        if ( error )
            return NULL;
        // Check if the entry is still NULL
        if ( timer_table[key]==NULL ) {
            // Create a new entry
            store_timer_data_info *info_tmp = new store_timer_data_info;
            info_tmp->id = id;
            info_tmp->start_line = -2;
            info_tmp->stop_line = -1;
            info_tmp->next = NULL;
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
            bool error = GET_LOCK(&lock);
            if ( error )
                return NULL;
            // Check if another thread created an entry while we were waiting for the lock
            if ( info->next==NULL ) {
                // Create a new entry
                store_timer_data_info *info_tmp = new store_timer_data_info;
                info_tmp->id = id;
                info_tmp->start_line = -2;
                info_tmp->stop_line = -1;
                info_tmp->next = NULL;
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
            const char *buffer = reinterpret_cast<const char*>(comm_recv1(r,0));
            const size_t *tmp = reinterpret_cast<const size_t*>(buffer);
            size_t N_timers = tmp[0];
            ASSERT(N_timers<0x100000);
            std::vector<TimerResults> add(N_timers);
            size_t pos = sizeof(size_t);
            for (size_t i=0; i<add.size(); i++) {
                add[i].unpack(&buffer[pos]);
                pos += add[i].size();
            }
            add_timers( timers, add );
            delete [] reinterpret_cast<const double*>(buffer);
        }
    } else {
        size_t N_bytes = sizeof(size_t);
        for (size_t i=0; i<timers.size(); i++)
            N_bytes += timers[i].size();
        char *buffer = new char[N_bytes+8];
        size_t *tmp = reinterpret_cast<size_t*>(buffer);
        tmp[0] = timers.size();
        size_t pos = sizeof(size_t);
        for (size_t i=0; i<timers.size(); i++) {
            timers[i].pack(&buffer[pos]);
            pos += timers[i].size();
        }
        comm_send1( buffer, N_bytes, 0, 0 );
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
            memory[r].bytes = comm_recv2<size_t>(r,2);
        }
    } else {
        ASSERT(memory[0].time.size()==memory[0].bytes.size());
        comm_send2( memory[0].time,  0, 1 );
        comm_send2( memory[0].bytes, 0, 2 );
        memory.clear();
    }
    comm_barrier();
}
void ProfilerApp::add_timers( std::vector<TimerResults>& timers, 
    const std::vector<TimerResults> add )
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
* Function to return a unique id based on the message and filename.    *
* Note:  We want to return a unique (but deterministic) id for each    *
* filename/message pair.  We want each process or thread to return the *
* same id independent of the other calls.                              *
***********************************************************************/
size_t ProfilerApp::get_timer_id( const char* message, const char* filename )
{
    unsigned int c;
    // Hash the filename using DJB2
    const char *s = strip_path(filename);
    unsigned int hash1 = 5381;
    while((c = *s++)) {
        // hash = hash * 33 ^ c
        hash1 = ((hash1 << 5) + hash1) ^ c;
    }
    // Hash the message using DJB2
    s = message;
    unsigned int hash2 = 5381;
    while((c = *s++)) {
        // hash = hash * 33 ^ c
        hash2 = ((hash2 << 5) + hash2) ^ c;
    }
    // Combine the two hashes
    size_t key = 0;
    if ( sizeof(unsigned int)==sizeof(size_t) )
        key = hash1^hash2;
    else if ( sizeof(unsigned int)==4 && sizeof(size_t)==8 )
        key = (static_cast<size_t>(hash1)<<16) + static_cast<size_t>(hash2);
    else 
        ERROR_MSG("Unhandled case");
    return key;
}


/***********************************************************************
* Function to return a unique id based on the active timer bit array.  *
* This function works by performing a DJB2 hash on the bit array       *
***********************************************************************/
inline size_t ProfilerApp::get_trace_id( const size_t *trace ) 
{
    #if TRACE_SIZE%4 != 0
        #error TRACE_SIZE must be a multiple of 4
    #endif
    #if MONITOR_PROFILER_PERFORMANCE > 1
        TIME_TYPE start_time_local;
        get_time(&start_time_local);
    #endif
    size_t hash1 = 5381;
    size_t hash2 = 104729;
    size_t hash3 = 1299709;
    size_t hash4 = 15485863;
    const size_t* s1 = &trace[0];
    const size_t* s2 = &trace[1];
    const size_t* s3 = &trace[2];
    const size_t* s4 = &trace[3];
    #if ARCH_SIZE==32
        hash1 = ((hash1 << 5) + hash1) ^ (*s1);
        hash2 = ((hash2 << 5) + hash2) ^ (*s2);
        hash3 = ((hash3 << 5) + hash3) ^ (*s3);
        hash4 = ((hash4 << 5) + hash4) ^ (*s4);
        for (size_t i=4; i<TRACE_SIZE; i+=4) {
            // hash = hash * 33 ^ s[i]
            size_t c1 = *(s1+=4);
            size_t c2 = *(s2+=4);
            size_t c3 = *(s3+=4);
            size_t c4 = *(s4+=4);
            hash1 = ((hash1 << 5) + hash1) ^ c1;
            hash2 = ((hash2 << 5) + hash2) ^ c2;
            hash3 = ((hash3 << 5) + hash3) ^ c3;
            hash4 = ((hash4 << 5) + hash4) ^ c4;
        }
    #elif ARCH_SIZE==64
        hash1 = ((hash1 << 16) + hash1) ^ (*s1);
        hash2 = ((hash2 << 16) + hash2) ^ (*s2);
        hash3 = ((hash3 << 16) + hash3) ^ (*s3);
        hash4 = ((hash4 << 16) + hash4) ^ (*s4);
        for (size_t i=4; i<TRACE_SIZE; i+=4) {
            // hash = hash * 65537 ^ s[i]
            size_t c1 = *(s1+=4);
            size_t c2 = *(s2+=4);
            size_t c3 = *(s3+=4);
            size_t c4 = *(s4+=4);
            hash1 = ((hash1 << 16) + hash1) ^ c1;
            hash2 = ((hash2 << 16) + hash2) ^ c2;
            hash3 = ((hash3 << 16) + hash3) ^ c3;
            hash4 = ((hash4 << 16) + hash4) ^ c4;
        }
    #else 
        #error Not implimented
    #endif
    size_t hash = hash1 ^ hash2 ^ hash3 ^ hash4;
    #if MONITOR_PROFILER_PERFORMANCE > 1
        TIME_TYPE stop_time_local;
        get_time(&stop_time_local);
        total_trace_id_time += get_diff(start_time_local,stop_time_local,frequency);
    #endif
    return hash;
}


/***********************************************************************
* Function to return the current memory usage                          *
* Note: this function should be thread-safe                            *
***********************************************************************/
#if defined(USE_MAC)
    // Get the page size on mac
    static size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
inline size_t ProfilerApp::get_memory_usage( )
{
    size_t N_bytes = 0;
    #if defined(USE_LINUX)
        struct mallinfo meminfo = mallinfo();
        size_t size_hblkhd = static_cast<unsigned int>( meminfo.hblkhd );
        size_t size_uordblks = static_cast<unsigned int>( meminfo.uordblks );
        N_bytes = size_hblkhd + size_uordblks;
    #elif defined(USE_MAC)
        struct task_basic_info t_info;
        mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
        kern_return_t rtn = task_info( mach_task_self(), 
            TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count );
        if ( rtn != KERN_SUCCESS ) { return 0; }
        N_bytes = t_info.virtual_size;
    #elif defined(USE_WINDOWS)
        PROCESS_MEMORY_COUNTERS memCounter;
        GetProcessMemoryInfo( GetCurrentProcess(), &memCounter, sizeof(memCounter) );
        N_bytes = memCounter.WorkingSetSize;
    #endif
    ASSERT(N_bytes<1e12);
    return N_bytes;
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
static inline void mergeArrays( size_t N_list, 
    size_t *N, type_a **arr, type_b **brr,
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


