#include "ProfilerApp.h"
#include "MemoryApp.h"
#include "ProfilerAtomicHelpers.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>


#ifdef USE_MPI
#include <mpi.h>
#endif


#define ASSERT( EXP )                                                                    \
    do {                                                                                 \
        if ( !( EXP ) ) {                                                                \
            std::stringstream stream;                                                    \
            stream << "Failed assertion: " << #EXP << " " << __FILE__ << " " << __LINE__ \
                   << std::endl;                                                         \
            throw std::logic_error( stream.str() );                                      \
        }                                                                                \
                                                                                         \
    } while ( false )

#define diff_ns( X, Y ) std::chrono::duration_cast<std::chrono::nanoseconds>( X - Y ).count()


/******************************************************************
 * Define global variables                                         *
 ******************************************************************/
ProfilerApp global_profiler;
constexpr size_t ProfilerApp::StoreActive::TRACE_SIZE;
constexpr size_t ProfilerApp::MAX_TRACE_TRACE;
constexpr size_t ProfilerApp::MAX_THREADS;
constexpr size_t ProfilerApp::TIMER_HASH_SIZE;


// Declare quicksort
template<class type_a, class type_b>
static inline void quicksort2( int n, type_a* arr, type_b* brr );

// Inline function to get the current time/date string (without the newline character)
static inline std::string getDateString()
{
    time_t rawtime;
    time( &rawtime );
    std::string tmp( ctime( &rawtime ) );
    return tmp.substr( 0, tmp.length() - 1 );
}


/******************************************************************
 * Some inline functions to get the rank and comm size             *
 * Note: we want these functions to be safe to use, even if MPI    *
 *    has not been initialized.                                    *
 ******************************************************************/
#ifdef USE_MPI
static inline int comm_size()
{
    int size = 1;
    int flag = 0;
    MPI_Initialized( &flag );
    if ( flag )
        MPI_Comm_size( MPI_COMM_WORLD, &size );
    ASSERT( size > 0 );
    return size;
}
static inline int comm_rank()
{
    int rank = 0;
    int flag = 0;
    MPI_Initialized( &flag );
    if ( flag ) {
        int size = 0;
        MPI_Comm_size( MPI_COMM_WORLD, &size );
        MPI_Comm_rank( MPI_COMM_WORLD, &rank );
        ASSERT( size > 0 );
        ASSERT( rank < size && rank >= 0 );
    }
    return rank;
}
static inline void comm_barrier()
{
    if ( comm_size() > 1 )
        MPI_Barrier( MPI_COMM_WORLD );
}
static inline double comm_max_reduce( const double val )
{
    double result = val;
    if ( comm_size() > 1 )
        MPI_Allreduce( (double*) &val, &result, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD );
    return result;
}
template<class TYPE>
inline MPI_Datatype getType();
template<>
inline MPI_Datatype getType<char>()
{
    return MPI_CHAR;
}
template<>
inline MPI_Datatype getType<unsigned char>()
{
    return MPI_UNSIGNED_CHAR;
}
template<>
inline MPI_Datatype getType<int32_t>()
{
    return MPI_INT;
}
template<>
inline MPI_Datatype getType<uint32_t>()
{
    static_assert( sizeof( unsigned int ) == sizeof( uint32_t ), "Unexpected size" );
    return MPI_UNSIGNED;
}
template<>
inline MPI_Datatype getType<int64_t>()
{
    static_assert( sizeof( long long ) == sizeof( int64_t ), "Unexpected size" );
    return MPI_LONG_LONG;
}
template<>
inline MPI_Datatype getType<uint64_t>()
{
    static_assert( sizeof( unsigned long long ) == sizeof( int64_t ), "Unexpected size" );
    return MPI_UNSIGNED_LONG_LONG;
}
template<>
inline MPI_Datatype getType<double>()
{
    return MPI_DOUBLE;
}
template<class TYPE>
static inline void comm_send1( const TYPE* buf, size_t size, int dest, int tag )
{
    ASSERT( size < 0x80000000 );
    int err = MPI_Send( buf, (int) size, getType<TYPE>(), dest, tag, MPI_COMM_WORLD );
    ASSERT( err == MPI_SUCCESS );
}
template<class TYPE>
static inline TYPE* comm_recv1( int source, int tag )
{
    MPI_Status status;
    int err = MPI_Probe( source, tag, MPI_COMM_WORLD, &status );
    ASSERT( err == MPI_SUCCESS );
    int count;
    MPI_Get_count( &status, getType<TYPE>(), &count );
    auto* buf = new TYPE[count];
    err       = MPI_Recv( buf, count, getType<TYPE>(), source, tag, MPI_COMM_WORLD, &status );
    ASSERT( err == MPI_SUCCESS );
    return buf;
}
template<class TYPE>
static inline void comm_send2( const std::vector<TYPE>& data, int dest, int tag )
{
    int err =
        MPI_Send( data.data(), (int) data.size(), getType<TYPE>(), dest, tag, MPI_COMM_WORLD );
    ASSERT( err == MPI_SUCCESS );
}
template<class TYPE>
static inline std::vector<TYPE> comm_recv2( int source, int tag )
{
    MPI_Status status;
    int err = MPI_Probe( source, tag, MPI_COMM_WORLD, &status );
    ASSERT( err == MPI_SUCCESS );
    int count;
    MPI_Get_count( &status, getType<TYPE>(), &count );
    std::vector<TYPE> data( count );
    err = MPI_Recv( data.data(), count, getType<TYPE>(), source, tag, MPI_COMM_WORLD, &status );
    ASSERT( err == MPI_SUCCESS );
    return data;
}
#else
static inline int comm_size() { return 1; }
static inline int comm_rank() { return 0; }
static inline void comm_barrier() {}
static inline double comm_max_reduce( const double val ) { return val; }
template<class TYPE>
static inline void comm_send1( const TYPE*, size_t, int, int )
{
    throw std::logic_error( "Calling MPI routine in no-mpi build" );
}
template<class TYPE>
static inline TYPE* comm_recv1( int, int )
{
    throw std::logic_error( "Calling MPI routine in no-mpi build" );
    return nullptr;
}
template<class TYPE>
static inline void comm_send2( const std::vector<TYPE>&, int, int )
{
    throw std::logic_error( "Calling MPI routine in no-mpi build" );
}
template<class TYPE>
static inline std::vector<TYPE> comm_recv2( int, int )
{
    throw std::logic_error( "Calling MPI routine in no-mpi build" );
    return std::vector<TYPE>();
}
#endif


/***********************************************************************
 * StoreActive                                                          *
 ***********************************************************************/
inline void ProfilerApp::StoreActive::set( size_t i )
{
    constexpr uint64_t C = 0x61C8864680B583EBull;
    constexpr size_t N   = 64 * TRACE_SIZE;
    if ( i >= N )
        return;
    size_t j     = i >> 6;
    size_t k     = i & 0x3F;
    size_t mask  = ( (uint64_t) 0x1 ) << k;
    uint64_t tmp = ( trace[j] & mask ) == 0 ? ( C * i ) : 0;
    trace[j] |= mask;
    hash ^= tmp;
}
inline void ProfilerApp::StoreActive::unset( size_t i )
{
    constexpr uint64_t C = 0x61C8864680B583EBull;
    constexpr size_t N   = 64 * TRACE_SIZE;
    if ( i >= N )
        return;
    size_t j     = i >> 6;
    size_t k     = i & 0x3F;
    size_t mask  = ( (uint64_t) 0x1 ) << k;
    uint64_t tmp = ( trace[j] & mask ) == 0 ? 0 : ( C * i );
    trace[j] &= ~mask;
    hash ^= tmp;
}
inline ProfilerApp::StoreActive& ProfilerApp::StoreActive::operator=( const StoreActive& rhs )
{
    if ( rhs.hash != hash )
        memcpy( this, &rhs, sizeof( *this ) );
    return *this;
}
inline ProfilerApp::StoreActive& ProfilerApp::StoreActive::operator&=( const StoreActive& rhs )
{
    if ( rhs.hash == hash )
        return *this;
    // Find differences and unset them
    constexpr uint64_t C = 0x61C8864680B583EBull;
    for ( size_t i = 0; i < TRACE_SIZE; i++ ) {
        uint64_t mask = 0x01;
        for ( size_t j = 0; j < 64; j++ ) {
            if ( ( trace[i] & mask ) != 0 && ( rhs.trace[i] & mask ) == 0 ) {
                uint64_t tmp = ( trace[i] & mask ) == 0 ? 0 : ( C * i );
                trace[i] &= ~mask;
                hash ^= tmp;
            }
        }
    }
    return *this;
}
std::vector<uint32_t> ProfilerApp::StoreActive::getSet() const
{
    std::vector<uint32_t> x;
    for ( size_t i = 0; i < TRACE_SIZE; i++ ) {
        uint64_t mask = 0x01;
        for ( size_t j = 0; j < 64; j++, mask <<= 1 ) {
            if ( ( trace[i] & mask ) != 0 )
                x.push_back( i * 64 + j );
        }
    }
    return x;
}


/***********************************************************************
 * StoreTimes                                                           *
 ***********************************************************************/
inline void ProfilerApp::StoreTimes::push_back( uint64_t time )
{
    if ( d_size == d_capacity ) {
        auto old   = d_data;
        d_capacity = std::max<size_t>( d_capacity * 2, 1024 );
        d_data     = new uint64_t[d_capacity];
        memcpy( d_data, old, d_size * sizeof( int64_t ) );
        delete[] old;
    }
    d_data[d_size++] = time;
}


/***********************************************************************
 * StoreMemory                                                          *
 ***********************************************************************/
constexpr size_t ProfilerApp::StoreMemory::MAX_ENTRIES;
inline void ProfilerApp::StoreMemory::add( uint64_t time, ProfilerApp::MemoryLevel level,
    volatile TimerUtility::atomic::int64_atomic* bytes_profiler )
{
    static_assert( MAX_ENTRIES <= 0xFFFFFFFF, "MAX_ENTRIES must be < 2^32" );
    if ( d_size == MAX_ENTRIES )
        return;
    // Get the current memory usage
    uint64_t bytes = 0;
#ifndef TIMER_DISABLE_NEW_OVERLOAD
    if ( level == MemoryLevel::Fast )
        bytes = MemoryApp::getMemoryUsage();
    else
#endif
        bytes = MemoryApp::getTotalMemoryUsage();
    bytes -= *bytes_profiler;
    // Check if we need to allocate more memory
    if ( d_size == d_capacity ) {
        auto old_t = d_time;
        auto old_b = d_bytes;
        auto old_c = d_capacity;
        d_capacity *= 2;
        d_capacity = std::max<size_t>( d_capacity, 1024 );
        d_capacity = std::min<size_t>( d_capacity, MAX_ENTRIES );
        d_time     = new uint64_t[d_capacity];
        d_bytes    = new uint64_t[d_capacity];
        memcpy( d_time, old_t, d_size * sizeof( uint64_t ) );
        memcpy( d_bytes, old_b, d_size * sizeof( uint64_t ) );
        delete[] old_t;
        delete[] old_b;
        TimerUtility::atomic::atomic_add( bytes_profiler, ( d_capacity - old_c ) * 16 );
    }
    // Store the entry
    size_t i = d_size;
    if ( i < 2 ) {
        d_time[i]  = time;
        d_bytes[i] = bytes;
        d_size++;
    } else if ( bytes == d_bytes[i - 1] && bytes == d_bytes[i - 2] ) {
        d_time[i - 1] = time;
    } else {
        d_time[i]  = time;
        d_bytes[i] = bytes;
        d_size++;
    }
}
void ProfilerApp::StoreMemory::reset()
{
    d_size     = 0;
    d_capacity = 0;
    delete[] d_time;
    delete[] d_bytes;
    d_time  = nullptr;
    d_bytes = nullptr;
}
void ProfilerApp::StoreMemory::get(
    std::vector<uint64_t>& time, std::vector<uint64_t>& bytes ) const
{
    if ( d_size == 0 ) {
        time.clear();
        bytes.clear();
    }
    time.resize( d_size );
    bytes.resize( d_size );
    memcpy( time.data(), d_time, d_size * sizeof( uint64_t ) );
    memcpy( bytes.data(), d_bytes, d_size * sizeof( uint64_t ) );
}


/***********************************************************************
 * Functions to convert between character sets and hash keys            *
 * Note: we use the 64 character set { 0-9 a-z A-Z & $ }                *
 ***********************************************************************/
static constexpr inline char to_char( uint8_t x )
{
    char y = 0;
    if ( x < 10 )
        y = x + 48; // 0-9 (0-9)
    else if ( x < 36 )
        y = x + 87; // a-z (10-35)
    else if ( x < 62 )
        y = x + 29; // A-Z (36-61)
    else if ( x < 63 )
        y = '&'; // & (62)
    else if ( x < 64 )
        y = '$'; // $ (63)
    return y;
}
static constexpr inline uint8_t to_int( char x )
{
    char y = 0;
    if ( x >= 48 && x < 58 )
        y = x - 48; // 0-9 (0-9)
    else if ( x >= 97 && x < 123 )
        y = x - 87; // a-z (10-35)
    else if ( x >= 65 && x < 91 )
        y = x - 29; // A-Z (36-61)
    else if ( x == '&' )
        y = 62; // & (62)
    else if ( x == '$' )
        y = 63; // $ (63)
    return y;
}
static inline uint32_t str_to_hash( const char* str )
{
    uint32_t key = 0;
    int N        = 0;
    for ( int i = 0; i < 5 && str[i] != 0; i++ )
        N++;
    for ( int i = 0; i < N; i++ )
        key = ( key << 6 ) + to_int( str[N - i - 1] );
    return key;
}
static inline std::array<char, 6> hash_to_str( uint32_t key )
{
    // We will store the id use the 64 character set { 0-9 a-z A-Z & $ }
    // We can represent 2^30 values with 5 characters
    ASSERT( key < 0x40000000 );
    // Convert the key to a string
    std::array<char, 6> id = { 0, 0, 0, 0, 0, 0 };
    for ( int i = 0; key > 0; i++ ) {
        id[i] = to_char( key & 0x3F );
        key >>= 6;
    }
    return id;
}
static constexpr bool check_char_conversion()
{
    bool pass = true;
    for ( int i = 0; i < 64; i++ )
        pass = pass && to_int( to_char( i ) ) == i;
    return pass;
}


/***********************************************************************
 * id_struct                                                            *
 * Class to convert the timer id to a string                            *
 * The probability of a collision is ~N^2/2^N_bits                      *
 *   (N is the number of timers, we have 30 bits)                       *
 ***********************************************************************/
static_assert( sizeof( id_struct ) == 4, "Unexpected size for id_struct" );
static_assert( check_char_conversion(), "Internal error" );
// static_assert( str_to_hash( hash_to_str( 0x32eb809d ).data() ) == 0x32eb809d );
id_struct::id_struct( uint64_t id ) { data = ( id * 0x9E3779B97F4A7C15 ) >> 34; }
id_struct::id_struct( const std::string& rhs ) { data = str_to_hash( rhs.c_str() ); }
id_struct::id_struct( const char* rhs ) { data = str_to_hash( rhs ); }
std::array<char, 6> id_struct::str() const { return hash_to_str( data ); }


/***********************************************************************
 * TraceResults                                                         *
 ***********************************************************************/
TraceResults::TraceResults()
    : N_active( 0 ),
      thread( 0 ),
      rank( 0 ),
      N_trace( 0 ),
      min( std::numeric_limits<uint64_t>::max() ),
      max( 0 ),
      tot( 0 ),
      N( 0 ),
      mem( nullptr )
{
    static_assert( sizeof( id_struct ) <= sizeof( uint64_t ), "Unexpected size for id_struct" );
    ASSERT( str_to_hash( hash_to_str( 0x32eb809d ).data() ) == 0x32eb809d );
}
TraceResults::~TraceResults()
{
    delete[] mem;
    mem = nullptr;
}
TraceResults::TraceResults( const TraceResults& rhs )
    : id( rhs.id ),
      N_active( rhs.N_active ),
      thread( rhs.thread ),
      rank( rhs.rank ),
      N_trace( rhs.N_trace ),
      min( rhs.min ),
      max( rhs.max ),
      tot( rhs.tot ),
      N( rhs.N ),
      mem( nullptr )
{
    allocate();
    if ( mem != nullptr )
        memcpy( mem, rhs.mem, ( N_active + 2 * N_trace ) * sizeof( uint64_t ) );
}
TraceResults& TraceResults::operator=( const TraceResults& rhs )
{
    if ( this == &rhs )
        return *this;
    this->id       = rhs.id;
    this->thread   = rhs.thread;
    this->rank     = rhs.rank;
    this->N_active = rhs.N_active;
    this->N_trace  = rhs.N_trace;
    this->N        = rhs.N;
    this->min      = rhs.min;
    this->max      = rhs.max;
    this->tot      = rhs.tot;
    allocate();
    if ( mem != nullptr )
        memcpy( this->mem, rhs.mem, ( N_active + 2 * N_trace ) * sizeof( uint64_t ) );
    return *this;
}
void TraceResults::allocate()
{
    delete[] mem;
    mem = nullptr;
    if ( N_active > 0 || N_trace > 0 ) {
        mem = new uint64_t[N_active + 2 * N_trace];
        memset( mem, 0, ( N_active + 2 * N_trace ) * sizeof( uint64_t ) );
    }
}
id_struct* TraceResults::active()
{
    return N_active > 0 ? reinterpret_cast<id_struct*>( mem ) : nullptr;
}
const id_struct* TraceResults::active() const
{
    return N_active > 0 ? reinterpret_cast<const id_struct*>( mem ) : nullptr;
}
uint64_t* TraceResults::start() { return N_trace > 0 ? &mem[N_active] : nullptr; }
const uint64_t* TraceResults::start() const { return N_trace > 0 ? &mem[N_active] : nullptr; }
uint64_t* TraceResults::stop() { return N_trace > 0 ? &mem[N_active + N_trace] : nullptr; }
const uint64_t* TraceResults::stop() const
{
    return N_trace > 0 ? &mem[N_active + N_trace] : nullptr;
}
size_t TraceResults::size( bool store_trace ) const
{
    size_t bytes = sizeof( TraceResults );
    bytes += N_active * sizeof( id_struct );
    if ( store_trace )
        bytes += 2 * N_trace * sizeof( uint64_t );
    return bytes;
}
size_t TraceResults::pack( char* data, bool store_trace ) const
{
    auto* this2  = const_cast<TraceResults*>( this );
    int N_trace2 = N_trace;
    if ( !store_trace ) {
        this2->N_trace = 0;
    }
    memcpy( data, this, sizeof( TraceResults ) );
    this2->N_trace = N_trace2;
    size_t pos     = sizeof( TraceResults );
    if ( N_active > 0 ) {
        memcpy( &data[pos], active(), N_active * sizeof( id_struct ) );
        pos += N_active * sizeof( id_struct );
    }
    if ( N_trace > 0 && store_trace ) {
        memcpy( &data[pos], start(), N_trace * sizeof( uint64_t ) );
        pos += N_trace * sizeof( uint64_t );
        memcpy( &data[pos], stop(), N_trace * sizeof( uint64_t ) );
        pos += N_trace * sizeof( uint64_t );
    }
    return pos;
}
size_t TraceResults::unpack( const char* data )
{
    delete[] mem;
    memcpy( this, data, sizeof( TraceResults ) );
    mem = nullptr;
    allocate();
    size_t pos = sizeof( TraceResults );
    if ( N_active > 0 ) {
        memcpy( active(), &data[pos], N_active * sizeof( id_struct ) );
        pos += N_active * sizeof( id_struct );
    }
    if ( N_trace > 0 ) {
        memcpy( start(), &data[pos], N_trace * sizeof( uint64_t ) );
        pos += N_trace * sizeof( uint64_t );
        memcpy( stop(), &data[pos], N_trace * sizeof( uint64_t ) );
        pos += N_trace * sizeof( uint64_t );
    }
    return pos;
}
bool TraceResults::operator==( const TraceResults& rhs ) const
{
    bool equal          = id == rhs.id;
    equal               = equal && N_active == rhs.N_active;
    equal               = equal && thread == rhs.thread;
    equal               = equal && rank == rhs.rank;
    equal               = equal && N_trace == rhs.N_trace;
    equal               = equal && min == rhs.min;
    equal               = equal && max == rhs.max;
    equal               = equal && tot == rhs.tot;
    equal               = equal && N == rhs.N;
    const id_struct* a1 = active();
    const id_struct* a2 = rhs.active();
    for ( size_t i = 0; i < N_active; i++ )
        equal = equal && a1[i] == a2[i];
    return equal;
}


/***********************************************************************
 * TimerResults                                                         *
 ***********************************************************************/
TimerResults::TimerResults() : start( 0 ), stop( 0 )
{
    memset( message, 0, sizeof( message ) );
    memset( file, 0, sizeof( file ) );
    memset( path, 0, sizeof( path ) );
}
size_t TimerResults::size( bool store_trace ) const
{
    size_t bytes = sizeof( id );
    bytes += 3 * sizeof( int );
    bytes += sizeof( message );
    bytes += sizeof( file );
    bytes += sizeof( path );
    for ( const auto& i : trace )
        bytes += i.size( store_trace );
    return bytes;
}
size_t TimerResults::pack( char* data, bool store_trace ) const
{
    memcpy( data, &id, sizeof( id ) );
    size_t pos       = sizeof( id );
    const int tmp[3] = { start, stop, (int) trace.size() };
    memcpy( &data[pos], &tmp, sizeof( tmp ) );
    pos += sizeof( tmp );
    memcpy( &data[pos], message, sizeof( message ) );
    pos += sizeof( message );
    memcpy( &data[pos], file, sizeof( file ) );
    pos += sizeof( file );
    memcpy( &data[pos], path, sizeof( path ) );
    pos += sizeof( path );
    for ( const auto& i : trace )
        pos += i.pack( &data[pos], store_trace );
    return pos;
}
size_t TimerResults::unpack( const char* data )
{
    memcpy( &id, data, sizeof( id ) );
    size_t pos = sizeof( id );
    int tmp[3];
    memcpy( &tmp, &data[pos], sizeof( tmp ) );
    pos += sizeof( tmp );
    memcpy( message, &data[pos], sizeof( message ) );
    pos += sizeof( message );
    memcpy( file, &data[pos], sizeof( file ) );
    pos += sizeof( file );
    memcpy( path, &data[pos], sizeof( path ) );
    pos += sizeof( path );
    start = tmp[0];
    stop  = tmp[1];
    trace.resize( tmp[2] );
    for ( auto& i : trace )
        pos += i.unpack( &data[pos] );
    return pos;
}
bool TimerResults::operator==( const TimerResults& rhs ) const
{
    bool equal = id == rhs.id;
    equal      = equal && strcmp( message, rhs.message ) == 0;
    equal      = equal && strcmp( file, rhs.file ) == 0;
    equal      = equal && strcmp( path, rhs.path ) == 0;
    equal      = equal && start == rhs.start;
    equal      = equal && stop == rhs.stop;
    for ( size_t i = 0; i < trace.size(); i++ )
        equal = equal && trace[i] == rhs.trace[i];
    return equal;
}


/***********************************************************************
 * MemoryResults                                                        *
 ***********************************************************************/
size_t MemoryResults::size() const
{
    size_t N_bytes = 2 * sizeof( size_t );
    N_bytes += time.size() * sizeof( uint64_t );
    N_bytes += bytes.size() * sizeof( uint64_t );
    return N_bytes;
}
size_t MemoryResults::pack( char* data ) const
{
    const size_t tmp[2] = { (size_t) rank, time.size() };
    memcpy( data, &tmp, sizeof( tmp ) );
    size_t pos = sizeof( tmp );
    if ( !time.empty() ) {
        memcpy( &data[pos], &time[0], tmp[1] * sizeof( uint64_t ) );
        pos += time.size() * sizeof( uint64_t );
        memcpy( &data[pos], &bytes[0], tmp[1] * sizeof( uint64_t ) );
        pos += time.size() * sizeof( uint64_t );
    }
    return pos;
}
size_t MemoryResults::unpack( const char* data )
{
    size_t tmp[2];
    memcpy( &tmp, data, sizeof( tmp ) );
    rank     = static_cast<int>( tmp[0] );
    size_t N = tmp[1];
    time.resize( N );
    bytes.resize( N );
    size_t pos = sizeof( tmp );
    if ( !time.empty() ) {
        memcpy( &time[0], &data[pos], tmp[1] * sizeof( uint64_t ) );
        pos += time.size() * sizeof( uint64_t );
        memcpy( &bytes[0], &data[pos], tmp[1] * sizeof( uint64_t ) );
        pos += time.size() * sizeof( uint64_t );
    }
    return pos;
}
bool MemoryResults::operator==( const MemoryResults& rhs ) const
{
    bool equal = rank == rhs.rank;
    equal      = equal && time.size() == rhs.time.size();
    equal      = equal && bytes.size() == rhs.bytes.size();
    if ( !equal )
        return false;
    for ( size_t i = 0; i < time.size(); i++ )
        equal = equal && time[i] == rhs.time[i];
    for ( size_t i = 0; i < bytes.size(); i++ )
        equal = equal && bytes[i] == rhs.bytes[i];
    return equal;
}


/***********************************************************************
 * TimerMemoryResults                                                   *
 ***********************************************************************/
size_t TimerMemoryResults::size() const
{
    size_t bytes = 3 * sizeof( int );
    for ( const auto& timer : timers )
        bytes += timer.size();
    for ( const auto& i : memory )
        bytes += i.size();
    return bytes;
}
size_t TimerMemoryResults::pack( char* data ) const
{
    const int tmp[3] = { N_procs, (int) timers.size(), (int) memory.size() };
    memcpy( data, &tmp, sizeof( tmp ) );
    size_t pos = sizeof( tmp );
    for ( const auto& timer : timers )
        pos += timer.pack( &data[pos] );
    for ( const auto& i : memory )
        pos += i.pack( &data[pos] );
    return pos;
}
size_t TimerMemoryResults::unpack( const char* data )
{
    int tmp[3];
    memcpy( &tmp, data, sizeof( tmp ) );
    size_t pos = sizeof( tmp );
    N_procs    = tmp[0];
    timers.resize( tmp[1] );
    memory.resize( tmp[2] );
    for ( auto& timer : timers )
        pos += timer.unpack( &data[pos] );
    for ( auto& i : memory )
        pos += i.unpack( &data[pos] );
    return pos;
}
bool TimerMemoryResults::operator==( const TimerMemoryResults& rhs ) const
{
    bool equal = N_procs == rhs.N_procs;
    equal      = equal && timers.size() == rhs.timers.size();
    equal      = equal && memory.size() == rhs.memory.size();
    if ( !equal )
        return false;
    for ( size_t i = 0; i < timers.size(); i++ )
        equal = equal && timers[i] == rhs.timers[i];
    for ( size_t i = 0; i < memory.size(); i++ )
        equal = equal && memory[i] == rhs.memory[i];
    return equal;
}


/***********************************************************************
 * Constructor/Destructor                                               *
 ***********************************************************************/
ProfilerApp::ProfilerApp() : d_N_threads( 0 ), d_construct_time( std::chrono::steady_clock::now() )
{
    for ( auto& i : thread_table )
        i = nullptr;
    for ( auto& i : timer_table )
        i = nullptr;
    N_timers              = 0;
    d_level               = 0;
    d_shift               = 0;
    d_store_trace_data    = false;
    d_store_memory_data   = MemoryLevel::None;
    d_disable_timer_error = false;
    d_bytes               = sizeof( ProfilerApp );
}
ProfilerApp::~ProfilerApp() { disable(); }
void ProfilerApp::setStoreTrace( bool profile )
{
    if ( N_timers == 0 )
        d_store_trace_data = profile;
    else
        throw std::logic_error( "Cannot change trace status after a timer is started" );
}
void ProfilerApp::setStoreMemory( MemoryLevel memory )
{
    if ( N_timers == 0 )
        d_store_memory_data = memory;
    else
        throw std::logic_error( "Cannot change memory status after a timer is started" );
}


/***********************************************************************
 * Function to syncronize the timers                                    *
 ***********************************************************************/
void ProfilerApp::synchronize()
{
    d_lock.lock();
    comm_barrier();
    uint64_t ns     = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
    uint64_t offset = comm_max_reduce( static_cast<double>( ns ) );
    d_shift         = offset - ns;
    d_lock.unlock();
}


/***********************************************************************
 * Function to start profiling a block of code                          *
 ***********************************************************************/
void ProfilerApp::activeErrStart( thread_info* thread, store_timer* timer )
{
    if ( d_disable_timer_error ) {
        // Stop the timer before starting
        this->stop( thread, timer );
    } else {
        // Throw an error
        char msg[2048];
        const auto& data = *( timer->timer_data );
        sprintf( msg, "Timer is not active, did you forget to call stop? (%s in %s at line %i)",
            data.message, data.filename, data.start_line );
        throw std::logic_error( msg );
    }
}
void ProfilerApp::start( thread_info* thread, store_timer* timer )
{
    if ( timer->is_active )
        activeErrStart( thread, timer );
    // Start the timer
    timer->trace     = thread->active;
    timer->is_active = true;
    thread->active.set( timer->trace_index );
    timer->start_time = std::chrono::steady_clock::now();
    // Record the memory usage
    if ( d_store_memory_data != MemoryLevel::None ) {
        int64_t ns = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
        thread->memory.add( ns, d_store_memory_data, &d_bytes );
    }
}


/***********************************************************************
 * Function to stop profiling a block of code                           *
 ***********************************************************************/
void ProfilerApp::activeErrStop( thread_info* thread, store_timer* timer )
{
    if ( d_disable_timer_error ) {
        // Start the timer before stopping
        this->start( thread, timer );
    } else {
        char msg[2048];
        const auto& data = *( timer->timer_data );
        sprintf( msg, "Timer is not active, did you forget to call start? (%s in %s at line %i)",
            data.message, data.filename, data.stop_line );
        throw std::logic_error( msg );
    }
}
void ProfilerApp::stop( thread_info* thread, store_timer* timer, time_point end_time )
{
    if ( !timer->is_active ) {
        activeErrStop( thread, timer );
        end_time = std::chrono::steady_clock::now();
    }
    timer->is_active = false;
    // Update the active trace log
    thread->active.unset( timer->trace_index );
    timer->trace &= thread->active;
    // Find the trace to save
    auto trace = timer->trace_head;
    while ( trace ) {
        if ( timer->trace == trace->trace )
            break;
        trace = trace->next;
    }
    if ( trace == nullptr ) {
        trace                 = new store_trace;
        constexpr size_t size = sizeof( store_trace );
        TimerUtility::atomic::atomic_add( &d_bytes, size );
        trace->trace = timer->trace;
        if ( !timer->trace_head ) {
            timer->trace_head = trace;
        } else {
            auto trace_list = timer->trace_head;
            while ( trace_list->next )
                trace_list = trace_list->next;
            trace_list->next = trace;
        }
    }
    // Calculate the time elapsed since start was called
    uint64_t ns = diff_ns( end_time, timer->start_time );
    // Save the starting and ending time if we are storing the detailed traces
    if ( d_store_trace_data && trace->N_calls < MAX_TRACE_TRACE ) {
        uint64_t start = diff_ns( timer->start_time, d_construct_time );
        uint64_t stop  = diff_ns( end_time, d_construct_time );
        trace->times.push_back( start );
        trace->times.push_back( stop );
    }
    // Save the new time info to the trace
    trace->max_time = std::max( trace->max_time, ns );
    trace->min_time = std::min( trace->min_time, ns );
    trace->total_time += ns;
    trace->N_calls++;
    // Get the memory usage
    if ( d_store_memory_data != MemoryLevel::None ) {
        uint64_t ns = diff_ns( end_time, d_construct_time );
        thread->memory.add( ns, d_store_memory_data, &d_bytes );
    }
}


/***********************************************************************
 * Function to enable/disable the timers                                *
 ***********************************************************************/
void ProfilerApp::memory()
{
    if ( d_store_memory_data != MemoryLevel::None ) {
        int64_t ns = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
        getThreadData()->memory.add( ns, d_store_memory_data, &d_bytes );
    }
}


/***********************************************************************
 * Function to enable/disable the timers                                *
 ***********************************************************************/
void ProfilerApp::enable( int level )
{
    // This is a blocking function so it cannot be called at the same time as disable
    if ( level < 0 || level >= 128 )
        throw std::logic_error( "level must be in the range 0-127" );
    d_lock.lock();
    d_level = level;
    d_lock.unlock();
}
void ProfilerApp::disable()
{
    // First, change the status flag
    d_lock.lock();
    d_level = -1;
    // delete the thread structures
    for ( auto& i : thread_table ) {
        delete i;
        i = nullptr;
    }
    // delete the timer data structures
    for ( auto& i : timer_table ) {
        delete i;
        i = nullptr;
    }
    N_timers = 0;
    d_bytes  = 0;
    d_lock.unlock();
}


/***********************************************************************
 * Function to return the profiling info                                *
 ***********************************************************************/
inline void ProfilerApp::getTimerResultsID( uint64_t id,
    std::vector<const thread_info*>& thread_list, int rank, const time_point& end_time,
    TimerResults& results ) const
{
    const size_t key = GET_TIMER_HASH( id );
    results.id       = id_struct( id );
    // Search for the global timer info
    auto* timer_global = const_cast<store_timer_data_info*>( timer_table[key] );
    while ( timer_global != nullptr ) {
        if ( timer_global->id == id )
            break;
        timer_global = const_cast<store_timer_data_info*>( timer_global->next );
    }
    if ( timer_global == nullptr )
        return;
    // Copy the basic timer info
    memcpy( results.message, timer_global->message, sizeof( results.message ) );
    memcpy( results.file, timer_global->filename, sizeof( results.file ) );
    memcpy( results.path, timer_global->path, sizeof( results.file ) );
    results.start = timer_global->start_line;
    results.stop  = timer_global->stop_line;
    // Loop through the thread entries
    for ( auto thread_data : thread_list ) {
        int thread_id = thread_data->id;
        // Search for a timer that matches the current id
        store_timer* timer = thread_data->head[key];
        while ( timer != nullptr ) {
            if ( timer->id == id )
                break;
            timer = timer->next;
        }
        if ( timer == nullptr ) {
            // The current thread does not have a copy of this timer, move on
            continue;
        }
        // If the timer is still running, add the current processing time to the totals
        bool create_trace = false;
        uint64_t time     = 0;
        uint64_t trace_id = 0;
        if ( timer->is_active ) {
            time        = diff_ns( end_time, timer->start_time );
            auto active = thread_data->active;
            active.unset( timer->trace_index );
            active &= timer->trace;
            trace_id     = active.id();
            create_trace = true;
        }
        // Loop through the trace entries
        store_trace* trace = timer->trace_head;
        while ( trace != nullptr ) {
            size_t k = results.trace.size();
            results.trace.resize( k + 1 );
            // Get the active list
            auto list = getActiveList( trace->trace, timer->trace_index, thread_data );
            // Get the running times of the trace
            size_t N_trace = 0;
            if ( d_store_trace_data )
                N_trace = std::min<size_t>( trace->N_calls, MAX_TRACE_TRACE );
            results.trace[k].id       = results.id;
            results.trace[k].thread   = thread_id;
            results.trace[k].rank     = rank;
            results.trace[k].N        = trace->N_calls;
            results.trace[k].N_active = list.size();
            results.trace[k].N_trace  = N_trace;
            results.trace[k].min      = trace->min_time;
            results.trace[k].max      = trace->max_time;
            results.trace[k].tot      = trace->total_time;
            results.trace[k].allocate();
            for ( size_t j = 0; j < list.size(); j++ )
                results.trace[k].active()[j] = list[j];
            // Determine if we need to add the running trace
            if ( trace_id == trace->trace.id() ) {
                results.trace[k].min = std::min<float>( results.trace[k].min, time );
                results.trace[k].max = std::max<float>( results.trace[k].max, time );
                results.trace[k].tot += time;
                trace_id     = 0;
                create_trace = false;
            }
            // Save the detailed trace results (this is a binary file)
            uint64_t* start = results.trace[k].start();
            uint64_t* stop  = results.trace[k].stop();
            auto it         = trace->times.begin();
            for ( size_t m = 0; m < N_trace; m++ ) {
                start[m] = *it + d_shift;
                ++it;
                stop[m] = *it + d_shift;
                ++it;
            }
            // Advance to the next trace
            trace = trace->next;
        }
        // Create a new trace if necessary
        if ( create_trace ) {
            size_t k = results.trace.size();
            results.trace.resize( k + 1 );
            size_t N_trace = 0;
            if ( d_store_trace_data )
                N_trace = 1;
            auto active = thread_data->active;
            active &= timer->trace;
            active.unset( timer->trace_index );
            auto list                 = getActiveList( active, timer->trace_index, thread_data );
            results.trace[k].id       = results.id;
            results.trace[k].thread   = thread_id;
            results.trace[k].rank     = rank;
            results.trace[k].N        = 1;
            results.trace[k].N_active = list.size();
            results.trace[k].N_trace  = N_trace;
            results.trace[k].min      = time;
            results.trace[k].max      = time;
            results.trace[k].tot      = time;
            results.trace[k].allocate();
            for ( size_t j = 0; j < list.size(); j++ )
                results.trace[k].active()[j] = list[j];
            if ( d_store_trace_data ) {
                uint64_t* start   = results.trace[k].start();
                uint64_t* stop    = results.trace[k].stop();
                int64_t start_tmp = diff_ns( timer->start_time, d_construct_time );
                int64_t end_tmp   = diff_ns( end_time, d_construct_time );
                *start            = start_tmp + d_shift;
                *stop             = end_tmp + d_shift;
            }
        }
    }
}
std::vector<TimerResults> ProfilerApp::getTimerResults() const
{
    // Get the current time in case we need to "stop" and timers
    auto end_time = std::chrono::steady_clock::now();
    int rank      = comm_rank();
    // Get a lock
    d_lock.lock();
    // Get the thread data
    // This can safely be done lock-free
    std::vector<const thread_info*> thread_list;
    for ( auto thread : thread_table ) {
        if ( thread != nullptr )
            thread_list.push_back( thread );
    }
    // Get a list of all timer ids
    std::vector<size_t> ids;
    for ( auto i : timer_table ) {
        auto* timer_global = const_cast<store_timer_data_info*>( i );
        while ( timer_global != nullptr ) {
            ids.push_back( timer_global->id );
            timer_global = const_cast<store_timer_data_info*>( timer_global->next );
        }
    }
    if ( (int) ids.size() != N_timers )
        throw std::logic_error( "Not all timers were found" );
    // Begin storing the timers
    std::vector<TimerResults> results( ids.size() );
    for ( size_t i = 0; i < ids.size(); i++ )
        getTimerResultsID( ids[i], thread_list, rank, end_time, results[i] );
    // Release the mutex
    d_lock.unlock();
    return results;
}
TimerResults ProfilerApp::getTimerResults( uint64_t id ) const
{
    // Get the current time in case we need to "stop" and timers
    auto end_time = std::chrono::steady_clock::now();
    int rank      = comm_rank();
    // Get a lock
    d_lock.lock();
    // Get the thread data
    // This can safely be done lock-free
    std::vector<const thread_info*> thread_list;
    for ( auto thread : thread_table ) {
        if ( thread != nullptr )
            thread_list.push_back( thread );
    }
    // Get the timer
    TimerResults results;
    getTimerResultsID( id, thread_list, rank, end_time, results );
    // Release the mutex
    d_lock.unlock();
    return results;
}


/***********************************************************************
 * Load the memory data                                                 *
 ***********************************************************************/
MemoryResults ProfilerApp::getMemoryResults() const
{
    // Get the current memory usage
    if ( d_store_memory_data != MemoryLevel::None ) {
        auto thread = const_cast<ProfilerApp*>( this )->getThreadData();
        int64_t ns  = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
        thread->memory.add( ns, d_store_memory_data, &d_bytes );
    }
    // Get the memory info for each thread
    size_t N = 0;
    std::vector<std::vector<uint64_t>> time_list, bytes_list;
    for ( auto thread : thread_table ) {
        if ( thread != nullptr ) {
            std::vector<uint64_t> time, bytes;
            thread->memory.get( time, bytes );
            if ( !time.empty() ) {
                N += time.size();
                time_list.emplace_back( std::move( time ) );
                bytes_list.emplace_back( std::move( bytes ) );
            }
        }
    }
    // Merge the data
    std::vector<uint64_t> time, bytes;
    if ( time_list.empty() ) {
        return MemoryResults();
    } else if ( time_list.size() == 1 ) {
        std::swap( time, time_list[0] );
        std::swap( bytes, bytes_list[0] );
    } else {
        time.reserve( N );
        bytes.reserve( N );
        int N2                    = time_list.size();
        size_t index[MAX_THREADS] = { 0 };
        for ( size_t i = 0; i < N; i++ ) {
            int k        = 0;
            uint64_t val = std::numeric_limits<uint64_t>::max();
            for ( int j = 0; j < N2; j++ ) {
                if ( index[j] == time_list[j].size() )
                    continue;
                if ( time_list[j][index[j]] < val ) {
                    k   = j;
                    val = time_list[j][index[j]];
                }
            }
            time.push_back( time_list[k][index[k]] );
            bytes.push_back( bytes_list[k][index[k]] );
            index[k]++;
        }
    }
    // Compress the results by removing values that have not changed
    // Note: we will always keep the first and last values
    size_t i = 1;
    for ( size_t j = 1; j < N - 1; j++ ) {
        if ( bytes[j] == bytes[i - 1] && bytes[j] == bytes[j + 1] )
            continue;
        time[i]  = time[j];
        bytes[i] = bytes[j];
        i++;
    }
    time[i]  = time[N - 1];
    bytes[i] = bytes[N - 1];
    N        = i + 1;
    // Create the memory results
    MemoryResults data;
    data.time.resize( N );
    data.bytes.resize( N );
    for ( size_t i = 0; i < N; i++ ) {
        data.time[i]  = time[i];
        data.bytes[i] = bytes[i];
    }
    return data;
}


/***********************************************************************
 * Function to save the profiling info                                  *
 ***********************************************************************/
void ProfilerApp::save( const std::string& filename, bool global ) const
{
    if ( this->d_level < 0 ) {
        std::cout << "Warning: Timers are not enabled, no data will be saved\n";
        return;
    }
    int N_procs = comm_size();
    int rank    = comm_rank();
    // Set the filenames
    char filename_timer[1000], filename_trace[1000], filename_memory[1000];
    if ( !global ) {
        sprintf( filename_timer, "%s.%i.timer", filename.c_str(), rank + 1 );
        sprintf( filename_trace, "%s.%i.trace", filename.c_str(), rank + 1 );
        sprintf( filename_memory, "%s.%i.memory", filename.c_str(), rank + 1 );
    } else {
        sprintf( filename_timer, "%s.0.timer", filename.c_str() );
        sprintf( filename_trace, "%s.0.trace", filename.c_str() );
        sprintf( filename_memory, "%s.0.memory", filename.c_str() );
    }
    // Get the current results
    auto results = getTimerResults();
    if ( (int) results.size() != N_timers )
        throw std::logic_error( "Not all timers were found" );
    if ( global ) {
        // Gather the timers from all files (rank 0 will do all writing)
        gatherTimers( results );
    }
    if ( !results.empty() ) {
        // Get the timer ids and sort the ids by the total time
        // to create a global order to save the results
        std::vector<size_t> id_order( results.size(), 0 );
        std::vector<double> total_time( results.size(), 0 );
        for ( size_t i = 0; i < results.size(); i++ ) {
            id_order[i]   = i;
            total_time[i] = 0.0;
            int N_threads = d_N_threads;
            std::vector<double> time_thread( N_threads, 0 );
            for ( auto& j : results[i].trace )
                time_thread[j.thread] += j.tot;
            for ( int j = 0; j < N_threads; j++ )
                total_time[i] = std::max<double>( total_time[i], time_thread[j] );
        }
        quicksort2( (int) results.size(), &total_time[0], &id_order[0] );
        // Open the file(s) for writing
        FILE* timerFile = fopen( filename_timer, "wb" );
        if ( timerFile == nullptr ) {
            std::cerr << "Error opening file for writing (timer)";
            return;
        }
        FILE* traceFile = nullptr;
        if ( d_store_trace_data ) {
            traceFile = fopen( filename_trace, "wb" );
            if ( traceFile == nullptr ) {
                std::cerr << "Error opening file for writing (trace)";
                fclose( timerFile );
                return;
            }
        }
        // Create the file header
        fprintf( timerFile, "                  Message                      Filename        " );
        fprintf( timerFile,
            "  Thread  Start Line  Stop Line  N_calls  Min Time  Max Time  Total Time\n" );
        fprintf( timerFile, "---------------------------------------------------------------" );
        fprintf( timerFile,
            "------------------------------------------------------------------------\n" );
        // Loop through the list of timers, storing the most expensive first
        for ( int ii = static_cast<int>( results.size() ) - 1; ii >= 0; ii-- ) {
            size_t i      = id_order[ii];
            int N_threads = d_N_threads;
            std::vector<int> N_thread( N_threads, 0 );
            std::vector<float> min_thread( N_threads, 1e38f );
            std::vector<float> max_thread( N_threads, 0.0f );
            std::vector<double> tot_thread( N_threads, 0.0 );
            for ( auto& trace : results[i].trace ) {
                int k = trace.thread;
                N_thread[k] += trace.N;
                min_thread[k] = std::min( min_thread[k], 1e-9f * trace.min );
                max_thread[k] = std::max( max_thread[k], 1e-9f * trace.max );
                tot_thread[k] += 1e-9f * trace.tot;
            }
            for ( int j = 0; j < N_threads; j++ ) {
                if ( N_thread[j] == 0 )
                    continue;
                // Save the timer to the file
                // Note: we always want one space in front in case the timer starts
                //    with '<' and is long.
                fprintf( timerFile,
                    " %29s  %30s   %4i   %7i    %7i  %8i     %8.3f  %8.3f  %10.3f\n",
                    results[i].message, results[i].file, j, results[i].start, results[i].stop,
                    N_thread[j], min_thread[j], max_thread[j], tot_thread[j] );
            }
        }
        // Loop through all of the entries, saving the detailed data and the trace logs
        fprintf( timerFile, "\n\n\n" );
        fprintf( timerFile, "<N_procs=%i,id=%i", N_procs, rank );
        fprintf( timerFile, ",store_trace=%i", d_store_trace_data ? 1 : 0 );
        fprintf( timerFile, ",store_memory=%i", d_store_memory_data != MemoryLevel::None ? 1 : 0 );
        fprintf( timerFile, ",date='%s'>\n", getDateString().c_str() );
        // Loop through the list of timers, storing the most expensive first
        for ( int ii = static_cast<int>( results.size() ) - 1; ii >= 0; ii-- ) {
            size_t i = id_order[ii];
            // Store the basic timer info
            const char e = 0x0E; // Escape character for printing strings
            fprintf( timerFile,
                "<timer:id=%s,message=%c%s%c,file=%c%s%c,path=%c%s%c,start=%i,stop=%i>\n",
                results[i].id.str().data(), e, results[i].message, e, e, results[i].file, e, e,
                results[i].path, e, results[i].start, results[i].stop );
            // Store the trace data
            for ( const auto& trace : results[i].trace ) {
                char active[4096];
                size_t pos = 0;
                for ( size_t k = 0; k < trace.N_active; k++ )
                    pos += sprintf( &active[pos], "%s ", trace.active()[k].str().data() );
                active[pos]     = 0;
                unsigned long N = trace.N;
                fprintf( timerFile,
                    "<trace:id=%s,thread=%u,rank=%u,N=%lu,min=%e,max=%e,tot=%e,active=[ %s]>\n",
                    trace.id.str().data(), trace.thread, trace.rank, N, 1e-9 * trace.min,
                    1e-9 * trace.max, 1e-9 * trace.tot, active );
                // Save the detailed trace results (this is a binary file)
                if ( trace.N_trace > 0 ) {
                    unsigned long Nt = trace.N_trace;
                    fprintf( traceFile, "<id=%s,thread=%u,rank=%u,active=[ %s],N=%lu>\n",
                        trace.id.str().data(), trace.thread, trace.rank, active, Nt );
                    std::vector<double> start( Nt ), stop( Nt );
                    const uint64_t* start0 = trace.start();
                    const uint64_t* stop0  = trace.stop();
                    for ( size_t i = 0; i < Nt; i++ ) {
                        start[i] = 1e-9 * start0[i];
                        stop[i]  = 1e-9 * stop0[i];
                    }
                    fwrite( start.data(), sizeof( double ), Nt, traceFile );
                    fwrite( stop.data(), sizeof( double ), Nt, traceFile );
                    fprintf( traceFile, "\n" );
                }
            }
        }
        // Close the file(s)
        fclose( timerFile );
        if ( traceFile != nullptr )
            fclose( traceFile );
    }
    results.clear();
    // Store the memory trace info
    if ( d_store_memory_data != MemoryLevel::None ) {
        FILE* memoryFile = fopen( filename_memory, "wb" );
        if ( memoryFile == nullptr ) {
            d_lock.unlock();
            std::cerr << "Error opening memory file" << std::endl;
            return;
        }
        // Get the memory usage
        std::vector<MemoryResults> data( 1, getMemoryResults() );
        if ( global ) {
            gatherMemory( data );
        }
        for ( auto& mem : data ) {
            size_t count = mem.time.size();
            if ( mem.bytes.size() != count ) {
                fclose( memoryFile );
                throw std::logic_error( "data size does not match count" );
            }
            // Determine a scale factor so we can use unsigned int to store the memory
            size_t max_mem_size = 0;
            for ( size_t i = 0; i < count; i++ )
                max_mem_size = std::max<uint64_t>( max_mem_size, mem.bytes[i] );
            size_t scale;
            std::string units;
            if ( max_mem_size < 0xFFFFFFFF ) {
                scale = 1;
                units = "bytes";
            } else if ( max_mem_size < 0x3FFFFFFFFFF ) {
                scale = 1024;
                units = "kB";
            } else if ( max_mem_size < 0xFFFFFFFFFFFFF ) {
                scale = 1024 * 1024;
                units = "MB";
            } else {
                scale = 1024 * 1024 * 1024;
                units = "GB";
            }
            // Copy the time and size to new buffers
            auto* time = new double[count];
            auto* size = new unsigned int[count];
            for ( size_t i = 0; i < count; i++ ) {
                time[i] = 1e-9 * mem.time[i];
                size[i] = mem.bytes[i] / scale;
            }
            // Save the results
            // Note: Visual studio has an issue with type %zi
            ASSERT( sizeof( unsigned int ) == 4 );
            fprintf( memoryFile, "<N=%li,type1=%s,type2=%s,units=%s,rank=%i>\n",
                static_cast<long int>( count ), "double", "uint32", units.c_str(), rank );
            size_t N1 = fwrite( time, sizeof( double ), count, memoryFile );
            size_t N2 = fwrite( size, sizeof( unsigned int ), count, memoryFile );
            fprintf( memoryFile, "\n" );
            delete[] time;
            delete[] size;
            if ( N1 != (size_t) count || N2 != (size_t) count ) {
                fclose( memoryFile );
                throw std::logic_error( "Failed to write memory results" );
            }
        }
        fclose( memoryFile );
    }
}


/***********************************************************************
 * Load the timer and trace data                                        *
 ***********************************************************************/
static inline void getField( const char* line, size_t N, const char* name, char* data )
{
    const char* ptr = strstr( line, name );
    if ( ptr == nullptr ) {
        data[0] = 0;
    } else {
        int i1 = -1;
        int i2 = -1;
        for ( int i = 0; i < (int) N; i++ ) {
            if ( ptr[i] < 32 )
                break;
            if ( ptr[i] == '=' )
                i1 = i + 1;
            if ( i1 == -1 )
                continue;
            if ( ptr[i] == ',' || ptr[i] == '>' || ptr[i] == '\n' ) {
                i2 = i;
                break;
            }
        }
        ASSERT( i2 != -1 && i2 >= i1 );
        memcpy( data, &ptr[i1], i2 - i1 );
        data[i2 - i1] = 0;
    }
}
static inline char* getFieldArray( char* line, std::vector<std::pair<char*, char*>>& data )
{
    // This function parses a line of the form <field=value,field=value>
    const char e = 0x0E; // Escape character for printing strings
    ASSERT( *line == '<' );
    data.clear();
    line++;
    while ( *line >= 32 ) {
        int j1, j2;
        for ( j1 = 0; line[j1] != '='; j1++ ) {}
        line[j1] = 0;
        j1++;
        if ( line[j1] == e ) {
            line[j1] = 0;
            j1++;
            for ( j2 = j1; line[j2] != e; j2++ ) {}
            line[j2]     = 0;
            line[j2 + 1] = 0;
            j2++;
        } else {
            for ( j2 = j1; line[j2] != '>' && line[j2] != ','; j2++ ) {}
            line[j2] = 0;
        }
        data.emplace_back( line, &line[j1] );
        line += j2 + 1;
    }
    return line + 1;
}
static inline void getActiveIds( const char* active_list, std::vector<id_struct>& ids )
{
    ids.resize( 0 );
    const char* p1 = active_list;
    while ( *p1 != 0 ) {
        while ( *p1 == ' ' || *p1 == '[' || *p1 == ']' ) {
            p1++;
        }
        const char* p2 = p1;
        while ( *p2 != ' ' && *p2 != ']' && *p2 != 0 ) {
            p2++;
        }
        if ( p2 > p1 ) {
            char tmp[10];
            for ( int i = 0; i < p2 - p1; i++ )
                tmp[i] = p1[i];
            tmp[p2 - p1] = 0;
            ids.emplace_back( tmp );
            p1 = p2;
        }
    }
}
int ProfilerApp::loadFiles( const std::string& filename, int index, TimerMemoryResults& data )
{
    int N_procs = 0;
    std::string date;
    bool trace_data, memory_data;
    char timer[200], trace[200], memory[200];
    sprintf( timer, "%s.%i.timer", filename.c_str(), index );
    sprintf( trace, "%s.%i.trace", filename.c_str(), index );
    sprintf( memory, "%s.%i.memory", filename.c_str(), index );
    loadTimer( timer, data.timers, N_procs, date, trace_data, memory_data );
    if ( trace_data )
        loadTrace( trace, data.timers );
    if ( memory_data )
        loadMemory( memory, data.memory );
    return N_procs;
}
template<class TYPE>
inline void keepRank( std::vector<TYPE>& data, int rank )
{
    size_t i2 = 0;
    for ( size_t i1 = 0; i1 < data.size(); i1++ ) {
        if ( (int) data[i1].rank == rank ) {
            data[i2] = data[i1];
            i2++;
        }
    }
    data.resize( i2 );
}
TimerMemoryResults ProfilerApp::load( const std::string& filename, int rank, bool global )
{
    TimerMemoryResults data;
    data.timers.clear();
    data.memory.clear();
    int N_procs = 0;
    if ( global ) {
        N_procs = loadFiles( filename, 0, data );
        if ( rank != -1 ) {
            for ( auto& timer : data.timers )
                keepRank( timer.trace, rank );
            keepRank( data.memory, rank );
        }
    } else {
        if ( rank == -1 ) {
            // Load the root file
            N_procs = loadFiles( filename, 1, data );
            // Reserve trace memory for all ranks
            for ( auto& timer : data.timers )
                timer.trace.reserve( N_procs * timer.trace.size() );
            // Load the remaining files
            for ( int i = 1; i < N_procs; i++ )
                loadFiles( filename, i + 1, data );
        } else {
            N_procs = loadFiles( filename, rank + 1, data );
        }
    }
    data.N_procs = N_procs;
    return data;
}
void ProfilerApp::loadTimer( const std::string& filename, std::vector<TimerResults>& data,
    int& N_procs, std::string& date, bool& trace_data, bool& memory_data )
{
    // Load the file to memory for reading
    FILE* fid = fopen( filename.c_str(), "rb" );
    if ( fid == nullptr )
        throw std::logic_error( "Error opening file: " + filename );
    fseek( fid, 0, SEEK_END );
    size_t file_length = ftell( fid );
    if ( file_length > 0x80000000 ) {
        // We do not yet support large files, we need to read the data in chunks
        fclose( fid );
        throw std::logic_error( "Large timer files are not yet supported (likely to exhaust ram)" );
    }
    auto* buffer = new char[file_length + 10];
    memset( buffer, 0, file_length + 10 );
    rewind( fid );
    size_t result = fread( buffer, 1, file_length, fid );
    if ( result != file_length ) {
        delete[] buffer;
        fclose( fid );
        throw std::logic_error( "error reading file" );
    }
    fclose( fid );
    // Create a map of the ids and indicies of the timers (used for searching)
    std::map<id_struct, size_t> id_map;
    for ( size_t i = 0; i < data.size(); i++ )
        id_map.insert( std::pair<id_struct, size_t>( data[i].id, i ) );
    // Parse the data (this take most of the time)
    N_procs     = -1;
    int rank    = -1;
    trace_data  = false;
    memory_data = false;
    date        = std::string();
    std::vector<std::pair<char*, char*>> fields;
    std::vector<id_struct> active;
    char* line = buffer;
    while ( line < buffer + file_length ) {
        // Check if we are reading a dummy (human-readable) line
        if ( line[0] != '<' ) {
            while ( *line >= 32 ) {
                line++;
            }
            line++;
            continue;
        }
        // Read the next line and split the fields
        line = getFieldArray( line, fields );
        if ( strcmp( fields[0].first, "N_procs" ) == 0 ) {
            // We are loading the header
            N_procs = atoi( fields[0].second );
            // Load the remaining fields
            for ( size_t i = 1; i < fields.size(); i++ ) {
                if ( strcmp( fields[i].first, "id" ) == 0 ||
                     strcmp( fields[i].first, "rank" ) == 0 ) {
                    // Load the id/rank
                    rank = atoi( fields[i].second );
                } else if ( strcmp( fields[i].first, "store_trace" ) == 0 ) {
                    // Check if we stored the trace file
                    trace_data = atoi( fields[i].second ) == 1;
                } else if ( strcmp( fields[i].first, "store_memory" ) == 0 ) {
                    // Check if we stored the memory file (optional)
                    memory_data = atoi( fields[i].second ) == 1;
                } else if ( strcmp( fields[i].first, "date" ) == 0 ) {
                    // Load the date (optional)
                    date = std::string( fields[i].second );
                } else {
                    throw std::logic_error(
                        std::string( "Unknown field (header): " ) + fields[i].first );
                }
            }
        } else if ( strcmp( fields[0].first, "timer:id" ) == 0 ) {
            // We are loading a timer field
            id_struct id( fields[0].second );
            // Find the timer
            auto it = id_map.find( id );
            if ( it == id_map.end() ) {
                // Create a new timer
                size_t k = data.size();
                id_map.insert( std::pair<id_struct, size_t>( id, k ) );
                data.resize( k + 1 );
                TimerResults& timer = data[k];
                timer.id            = id;
                // Load the remaining fields
                for ( size_t i = 1; i < fields.size(); i++ ) {
                    if ( strcmp( fields[i].first, "message" ) == 0 ) {
                        // Load the message
                        memset( timer.message, 0, sizeof( timer.message ) );
                        strncpy( timer.message, fields[i].second, sizeof( timer.message ) );
                    } else if ( strcmp( fields[i].first, "file" ) == 0 ) {
                        // Load the filename
                        memset( timer.file, 0, sizeof( timer.file ) );
                        strncpy( timer.file, fields[i].second, sizeof( timer.file ) );
                    } else if ( strcmp( fields[i].first, "path" ) == 0 ) {
                        // Load the path
                        memset( timer.path, 0, sizeof( timer.path ) );
                        strncpy( timer.path, fields[i].second, sizeof( timer.path ) );
                    } else if ( strcmp( fields[i].first, "start" ) == 0 ) {
                        // Load the start line
                        timer.start = atoi( fields[i].second );
                    } else if ( strcmp( fields[i].first, "stop" ) == 0 ) {
                        // Load the stop line
                        timer.stop = atoi( fields[i].second );
                    } else if ( strcmp( fields[i].first, "thread" ) == 0 ||
                                strcmp( fields[i].first, "N" ) == 0 ||
                                strcmp( fields[i].first, "min" ) == 0 ||
                                strcmp( fields[i].first, "max" ) == 0 ||
                                strcmp( fields[i].first, "tot" ) == 0 ) {
                        // Obsolete fields
                    } else {
                        throw std::logic_error(
                            std::string( "Unknown field (timer): " ) + fields[i].first );
                    }
                }
            }
        } else if ( strcmp( fields[0].first, "trace:id" ) == 0 ) {
            // We are loading a trace field
            id_struct id( fields[0].second );
            // Find the trace
            auto it = id_map.find( id );
            if ( it == id_map.end() )
                throw std::logic_error( "trace did not find matching timer" );
            size_t index = it->second;
            data[index].trace.resize( data[index].trace.size() + 1 );
            TraceResults& trace = data[index].trace.back();
            trace.id            = id;
            trace.N_trace       = 0;
            trace.rank          = rank;
            // Load the remaining fields
            for ( size_t i = 1; i < fields.size(); i++ ) {
                if ( strcmp( fields[i].first, "thread" ) == 0 ) {
                    // Load the thread id
                    trace.thread = atoi( fields[i].second );
                } else if ( strcmp( fields[i].first, "rank" ) == 0 ) {
                    // Load the rank id
                    trace.rank = atoi( fields[i].second );
                } else if ( strcmp( fields[i].first, "N" ) == 0 ) {
                    // Load N
                    trace.N = atoi( fields[i].second );
                } else if ( strcmp( fields[i].first, "min" ) == 0 ) {
                    // Load min
                    trace.min = 1e9 * atof( fields[i].second );
                } else if ( strcmp( fields[i].first, "max" ) == 0 ) {
                    // Load max
                    trace.max = 1e9 * atof( fields[i].second );
                } else if ( strcmp( fields[i].first, "tot" ) == 0 ) {
                    // Load tot
                    trace.tot = 1e9 * atof( fields[i].second );
                } else if ( strcmp( fields[i].first, "active" ) == 0 ) {
                    // Load the active timers
                    getActiveIds( fields[i].second, active );
                    trace.N_active = static_cast<unsigned short>( active.size() );
                    trace.allocate();
                    for ( size_t j = 0; j < active.size(); j++ )
                        trace.active()[j] = active[j];
                } else {
                    throw std::logic_error(
                        std::string( "Unknown field (trace): " ) + fields[i].first );
                }
            }
        } else {
            throw std::logic_error( "Unknown data field" );
        }
    }
    delete[] buffer;
}
void ProfilerApp::loadTrace( const std::string& filename, std::vector<TimerResults>& data )
{
    // Create a map of the ids and indicies of the timers (used for searching)
    std::map<id_struct, size_t> id_map;
    for ( size_t i = 0; i < data.size(); i++ )
        id_map.insert( std::pair<id_struct, size_t>( data[i].id, i ) );
    // Open the file for reading
    FILE* fid = fopen( filename.c_str(), "rb" );
    if ( fid == nullptr )
        throw std::logic_error( "Error opening file: " + filename );
    std::vector<id_struct> active;
    const size_t MAX_LINE = 0x10000;
    auto* line            = new char[MAX_LINE];
    auto* field           = new char[MAX_LINE];
    memset( line, 0, MAX_LINE );
    memset( field, 0, MAX_LINE );
    while ( true ) {
        // Read the header
        char* rtn = fgets( line, MAX_LINE - 1, fid );
        if ( rtn == nullptr )
            break;
        if ( line[0] <= 10 )
            continue;
        // Get the id and find the appropriate timer
        getField( line, MAX_LINE, "id=", field );
        ASSERT( field[0] != 0 );
        id_struct id( field );
        auto it = id_map.find( id );
        if ( it == id_map.end() )
            throw std::logic_error( "Did not find matching timer" );
        TimerResults& timer = data[it->second];
        // Read the remaining trace header data
        getField( line, MAX_LINE, "thread=", field );
        ASSERT( field[0] != 0 );
        auto thread = static_cast<unsigned int>( atoi( field ) );
        getField( line, MAX_LINE, "rank=", field );
        ASSERT( field[0] != 0 );
        auto rank = static_cast<unsigned int>( atoi( field ) );
        getField( line, MAX_LINE, "active=", field );
        ASSERT( field[0] != 0 );
        getActiveIds( field, active );
        getField( line, MAX_LINE, "N=", field );
        ASSERT( field[0] != 0 );
        unsigned long int N = strtoul( field, nullptr, 10 );
        // Find the appropriate trace
        int index = -1;
        for ( size_t i = 0; i < timer.trace.size(); i++ ) {
            if ( timer.trace[i].thread != thread || timer.trace[i].rank != rank ||
                 timer.trace[i].N_active != active.size() )
                continue;
            bool match = true;
            for ( size_t j = 0; j < active.size(); j++ )
                match = match && active[j] == timer.trace[i].active()[j];
            if ( !match )
                continue;
            index = static_cast<int>( i );
        }
        ASSERT( index != -1 );
        TraceResults& trace = timer.trace[index];
        ASSERT( trace.N_trace == 0 ); // Check that we do not have previous data
        trace.N_trace = N;
        trace.allocate();
        for ( size_t i = 0; i < active.size(); i++ )
            trace.active()[i] = active[i];
        // Read the data
        std::vector<double> tmp1( N ), tmp2( N );
        size_t N1 = fread( tmp1.data(), sizeof( double ), N, fid );
        size_t N2 = fread( tmp2.data(), sizeof( double ), N, fid );
        ASSERT( N1 == N && N2 == N );
        size_t rtn2 = fread( field, 1, 1, fid );
        ASSERT( rtn2 == 1 );
        // Copy the data to the trace
        uint64_t* start = trace.start();
        uint64_t* stop  = trace.stop();
        for ( size_t i = 0; i < N; i++ ) {
            start[i] = 1e9 * tmp1[i];
            stop[i]  = 1e9 * tmp2[i];
        }
    }
    fclose( fid );
    delete[] line;
    delete[] field;
}
inline size_t getScale( const std::string& units )
{
    size_t scale = 1;
    if ( units == "bytes" ) {
        scale = 1;
    } else if ( units == "kB" ) {
        scale = 1024;
    } else if ( units == "MB" ) {
        scale = 1024 * 1024;
    } else if ( units == "GB" ) {
        scale = 1024 * 1024 * 1024;
    } else {
        throw std::logic_error( "Not finished\n" );
    }
    return scale;
}
void ProfilerApp::loadMemory( const std::string& filename, std::vector<MemoryResults>& data )
{
    // Open the file for reading
    FILE* fid = fopen( filename.c_str(), "rb" );
    if ( fid == nullptr )
        throw std::logic_error( "Error opening file: " + filename );
    const size_t MAX_LINE = 0x10000;
    auto* line            = new char[MAX_LINE];
    auto* field           = new char[MAX_LINE];
    memset( line, 0, MAX_LINE );
    memset( field, 0, MAX_LINE );
    while ( true ) {
        // Read the header
        char* rtn = fgets( line, MAX_LINE - 1, fid );
        if ( rtn == nullptr )
            break;
        if ( line[0] <= 10 )
            continue;
        data.resize( data.size() + 1 );
        MemoryResults& memory = data.back();
        // Get the header fields
        getField( line, MAX_LINE, "N=", field );
        ASSERT( field[0] != 0 );
        unsigned long int N = strtoul( field, nullptr, 10 );
        getField( line, MAX_LINE, "type1=", field );
        ASSERT( field[0] != 0 );
        std::string type1( field );
        getField( line, MAX_LINE, "type2=", field );
        ASSERT( field[0] != 0 );
        std::string type2( field );
        getField( line, MAX_LINE, "units=", field );
        ASSERT( field[0] != 0 );
        size_t scale = getScale( field );
        getField( line, MAX_LINE, "rank=", field );
        ASSERT( field[0] != 0 );
        memory.rank = atoi( field );
        // Get the data
        if ( type1 == "double" && type2 == "uint32" ) {
            auto* time = new double[N];
            auto* size = new unsigned int[N];
            size_t N1  = fread( time, sizeof( double ), N, fid );
            size_t N2  = fread( size, sizeof( unsigned int ), N, fid );
            if ( N1 != N || N2 != N ) {
                delete[] time;
                delete[] size;
                throw std::logic_error( "error in memory" );
            }
            memory.time.resize( N );
            memory.bytes.resize( N );
            for ( size_t i = 0; i < N; i++ ) {
                memory.time[i]  = 1e9 * time[i];
                memory.bytes[i] = scale * size[i];
            }
            delete[] time;
            delete[] size;
        } else {
            throw std::logic_error( "Not finished" );
        }
    }
    fclose( fid );
    delete[] line;
    delete[] field;
}


/***********************************************************************
 * Function to get the list of active timers                            *
 ***********************************************************************/
std::vector<id_struct> ProfilerApp::getActiveList(
    const StoreActive& active, unsigned int myIndex, const thread_info* head )
{
    auto list = active.getSet();
    std::vector<id_struct> active_list;
    active_list.reserve( list.size() );
    for ( auto k : list ) {
        if ( k == myIndex )
            continue;
        store_timer* timer_tmp = nullptr;
        for ( auto m : head->head ) {
            timer_tmp = m;
            while ( timer_tmp != nullptr ) {
                if ( timer_tmp->trace_index == k )
                    break;
                timer_tmp = timer_tmp->next;
            }
            if ( timer_tmp != nullptr )
                break;
        }
        if ( timer_tmp == nullptr )
            throw std::logic_error( "Internal Error" );
        active_list.push_back( id_struct( timer_tmp->id ) );
    }
    return active_list;
}


/***********************************************************************
 * store_timer_data_info                                                *
 ***********************************************************************/
ProfilerApp::store_timer_data_info::store_timer_data_info(
    const char* msg, const char* filepath, uint64_t id0, int start, int stop )
{
    // Zero all data
    memset( this, 0, sizeof( store_timer_data_info ) );
    // Initialize key entries
    start_line = start;
    stop_line  = stop;
    id         = id0;
    next       = nullptr;
    // Fill the message
    static_assert( sizeof( message ) == 64, "Size changed" );
    int N = 0;
    for ( N = 0; N < 63 && msg[N] != 0; N++ )
        message[N] = msg[N];
    if ( N == 63 )
        message[60] = message[61] = message[62] = '.';
    // filename, path
    static_assert( sizeof( filename ) == 64, "Size changed" );
    static_assert( sizeof( path ) == 64, "Size changed" );
    const char* file = filepath;
    N                = 0;
    while ( filepath[N] != 0 ) {
        if ( filepath[N] == 47 || filepath[N] == 92 )
            file = &filepath[N + 1];
        N++;
    }
    int Nf = file == filepath ? 0 : ( file - filepath - 1 );
    if ( Nf < 64 ) {
        memcpy( path, filepath, Nf );
    } else {
        memcpy( path, filepath, 60 );
        path[60] = path[61] = path[62] = '.';
    }
    N = N - ( file - filepath );
    if ( N < 64 ) {
        memcpy( filename, file, N );
    } else {
        memcpy( filename, file, 60 );
        filename[60] = filename[61] = filename[62] = '.';
    }
}


/***********************************************************************
 * Return pointer to the global timer info creating if necessary        *
 ***********************************************************************/
ProfilerApp::store_timer_data_info* ProfilerApp::getTimerData(
    uint64_t id, const char* message, const char* filename, int start, int stop )
{
    size_t key = GET_TIMER_HASH( id ); // Get the hash index
    if ( timer_table[key] == nullptr ) {
        // The global timer does not exist, create it (requires blocking)
        // Acquire the lock (neccessary for modifying the timer_table)
        d_lock.lock();
        // Check if the entry is still nullptr
        if ( timer_table[key] == nullptr ) {
            // Create a new entry
            auto* info_tmp = new store_timer_data_info( message, filename, id, start, stop );
            TimerUtility::atomic::atomic_add( &d_bytes, sizeof( store_timer_data_info ) );
            timer_table[key] = info_tmp;
            N_timers++;
        }
        // Release the lock
        d_lock.unlock();
    }
    volatile store_timer_data_info* info = timer_table[key];
    while ( info->id != id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( info->next == nullptr ) {
            // Acquire the lock
            // Acquire the lock (neccessary for modifying the timer_table)
            d_lock.lock();
            // Check if another thread created an entry while we were waiting for the lock
            if ( info->next == nullptr ) {
                // Create a new entry
                auto* info_tmp = new store_timer_data_info( message, filename, id, start, stop );
                info->next     = info_tmp;
                N_timers++;
            }
            // Release the lock
            d_lock.unlock();
        }
        // Advance to the next entry
        info = info->next;
    }
    return const_cast<store_timer_data_info*>( info );
}


/***********************************************************************
 * Gather all timers on rank 0                                          *
 ***********************************************************************/
void ProfilerApp::gatherTimers( std::vector<TimerResults>& timers )
{
    comm_barrier();
    int rank    = comm_rank();
    int N_procs = comm_size();
    if ( rank == 0 ) {
        for ( int r = 1; r < N_procs; r++ ) {
            auto* buffer    = comm_recv1<char>( r, 0 );
            size_t pos      = 0;
            size_t N_timers = 0;
            memcpy( &N_timers, &buffer[pos], sizeof( N_timers ) );
            pos += sizeof( N_timers );
            ASSERT( N_timers < 0x100000 );
            std::vector<TimerResults> add( N_timers );
            for ( auto& i : add ) {
                i.unpack( &buffer[pos] );
                pos += i.size();
            }
            addTimers( timers, add );
            delete[] buffer;
        }
    } else {
        size_t N_bytes = sizeof( size_t );
        for ( auto& timer : timers )
            N_bytes += timer.size();
        auto* buffer    = new char[N_bytes];
        size_t pos      = 0;
        size_t N_timers = timers.size();
        memcpy( &buffer[pos], &N_timers, sizeof( N_timers ) );
        pos += sizeof( N_timers );
        for ( auto& timer : timers ) {
            timer.pack( &buffer[pos] );
            pos += timer.size();
        }
        ASSERT( pos == N_bytes );
        comm_send1<char>( buffer, N_bytes, 0, 0 );
        delete[] buffer;
        timers.clear();
    }
    comm_barrier();
}
void ProfilerApp::gatherMemory( std::vector<MemoryResults>& memory )
{
    comm_barrier();
    int rank    = comm_rank();
    int N_procs = comm_size();
    ASSERT( memory.size() == 1 );
    if ( rank == 0 ) {
        memory.resize( N_procs );
        for ( int r = 1; r < N_procs; r++ ) {
            memory[r].rank  = r;
            memory[r].time  = comm_recv2<uint64_t>( r, 1 );
            memory[r].bytes = comm_recv2<uint64_t>( r, 2 );
        }
    } else {
        ASSERT( memory[0].time.size() == memory[0].bytes.size() );
        comm_send2<uint64_t>( memory[0].time, 0, 1 );
        comm_send2<uint64_t>( memory[0].bytes, 0, 2 );
        memory.clear();
    }
    comm_barrier();
}
void ProfilerApp::addTimers(
    std::vector<TimerResults>& timers, const std::vector<TimerResults>& add )
{
    std::map<id_struct, size_t> id_map;
    for ( size_t i = 0; i < timers.size(); i++ )
        id_map.insert( std::pair<id_struct, size_t>( timers[i].id, i ) );
    for ( const auto& i : add ) {
        auto it = id_map.find( i.id );
        if ( it == id_map.end() ) {
            size_t j = timers.size();
            timers.push_back( i );
            id_map.insert( std::pair<id_struct, size_t>( timers[j].id, j ) );
        } else {
            size_t j = it->second;
            for ( const auto& k : i.trace )
                timers[j].trace.push_back( k );
        }
    }
}


/***********************************************************************
 * Subroutine to perform a quicksort                                    *
 ***********************************************************************/
template<class type_a, class type_b>
static inline void quicksort2( int n, type_a* arr, type_b* brr )
{
    bool test;
    int i, ir, j, jstack, k, l, istack[100];
    type_a a, tmp_a;
    type_b b, tmp_b;
    jstack = 0;
    l      = 0;
    ir     = n - 1;
    while ( true ) {
        if ( ir - l < 7 ) { // Insertion sort when subarray small enough.
            for ( j = l + 1; j <= ir; j++ ) {
                a    = arr[j];
                b    = brr[j];
                test = true;
                for ( i = j - 1; i >= 0; i-- ) {
                    if ( arr[i] < a ) {
                        arr[i + 1] = a;
                        brr[i + 1] = b;
                        test       = false;
                        break;
                    }
                    arr[i + 1] = arr[i];
                    brr[i + 1] = brr[i];
                }
                if ( test ) {
                    i          = l - 1;
                    arr[i + 1] = a;
                    brr[i + 1] = b;
                }
            }
            if ( jstack == 0 )
                return;
            ir = istack[jstack]; // Pop stack and begin a new round of partitioning.
            l  = istack[jstack - 1];
            jstack -= 2;
        } else {
            k = ( l + ir ) / 2; // Choose median of left, center and right elements as partitioning
                                // element a. Also rearrange so that a(l) ? a(l+1) ? a(ir).
            tmp_a      = arr[k];
            arr[k]     = arr[l + 1];
            arr[l + 1] = tmp_a;
            tmp_b      = brr[k];
            brr[k]     = brr[l + 1];
            brr[l + 1] = tmp_b;
            if ( arr[l] > arr[ir] ) {
                tmp_a   = arr[l];
                arr[l]  = arr[ir];
                arr[ir] = tmp_a;
                tmp_b   = brr[l];
                brr[l]  = brr[ir];
                brr[ir] = tmp_b;
            }
            if ( arr[l + 1] > arr[ir] ) {
                tmp_a      = arr[l + 1];
                arr[l + 1] = arr[ir];
                arr[ir]    = tmp_a;
                tmp_b      = brr[l + 1];
                brr[l + 1] = brr[ir];
                brr[ir]    = tmp_b;
            }
            if ( arr[l] > arr[l + 1] ) {
                tmp_a      = arr[l];
                arr[l]     = arr[l + 1];
                arr[l + 1] = tmp_a;
                tmp_b      = brr[l];
                brr[l]     = brr[l + 1];
                brr[l + 1] = tmp_b;
            }
            // Scan up to find element > a
            j = ir;
            a = arr[l + 1]; // Partitioning element.
            b = brr[l + 1];
            for ( i = l + 2; i <= ir; i++ ) {
                if ( arr[i] < a )
                    continue;
                while ( arr[j] > a ) // Scan down to find element < a.
                    j--;
                if ( j < i )
                    break;       // Pointers crossed. Exit with partitioning complete.
                tmp_a  = arr[i]; // Exchange elements of both arrays.
                arr[i] = arr[j];
                arr[j] = tmp_a;
                tmp_b  = brr[i];
                brr[i] = brr[j];
                brr[j] = tmp_b;
            }
            arr[l + 1] = arr[j]; // Insert partitioning element in both arrays.
            arr[j]     = a;
            brr[l + 1] = brr[j];
            brr[j]     = b;
            jstack += 2;
            // Push pointers to larger subarray on stack, process smaller subarray immediately.
            if ( ir - i + 1 >= j - l ) {
                istack[jstack]     = ir;
                istack[jstack - 1] = i;
                ir                 = j - 1;
            } else {
                istack[jstack]     = j - 1;
                istack[jstack - 1] = l;
                l                  = i;
            }
        }
    }
}


/***********************************************************************
 * C interfaces                                                         *
 ***********************************************************************/
extern "C" {
void global_profiler_enable( int level ) { global_profiler.enable( level ); }
void global_profiler_disable() { global_profiler.disable(); }
int global_profiler_get_level() { return global_profiler.getLevel(); }
void global_profiler_synchronize() { global_profiler.synchronize(); }
void global_profiler_set_store_trace( int flag ) { global_profiler.setStoreTrace( flag != 0 ); }
void global_profiler_set_store_memory( int flag )
{
    auto memory = flag != 0 ? ProfilerApp::MemoryLevel::Full : ProfilerApp::MemoryLevel::None;
    global_profiler.setStoreMemory( memory );
}
void global_profiler_start( const char* name, const char* file, int line, int level )
{
    global_profiler.start( name, file, line, level );
}
void global_profiler_stop( const char* name, const char* file, int line, int level )
{
    global_profiler.stop( name, file, line, level );
}
void global_profiler_save( const char* name, int global )
{
    global_profiler.save( name, global != 0 );
}
}
