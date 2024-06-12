#include "ProfilerApp.h"
#include "MemoryApp.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
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

#define NULL_USE( variable )                   \
    do {                                       \
        if ( 0 ) {                             \
            auto static t = (char*) &variable; \
            t++;                               \
        }                                      \
    } while ( 0 )


// Check the hash size
constexpr int log2int( uint64_t x )
{
    int r = 0;
    while ( x > 1 ) {
        r++;
        x = x >> 1;
    }
    return r;
}


/******************************************************************
 * Define global variables                                         *
 ******************************************************************/
ProfilerApp global_profiler;
constexpr size_t ProfilerApp::StoreTimes::MAX_TRACE;
constexpr size_t ProfilerApp::StoreMemory::MAX_ENTRIES;
constexpr size_t ProfilerApp::HASH_SIZE;
static_assert( ProfilerApp::HASH_SIZE == ( (uint64_t) 0x1 << log2int( ProfilerApp::HASH_SIZE ) ) );


// Declare quicksort
template<class A>
static inline void quicksort( std::vector<A>& a );
template<class A, class B>
static inline void quicksort( std::vector<A>& a, std::vector<B>& b );
template<class T>
void unique( std::vector<T>& x );
template<class T>
static inline int binarySearch( const std::vector<T>& x, T v );


// Inline function to get the current time/date string (without the newline character)
static inline std::string getDateString()
{
    time_t rawtime;
    time( &rawtime );
    std::string tmp( ctime( &rawtime ) );
    return tmp.substr( 0, tmp.length() - 1 );
}


// check if two values are approximately equal
static bool approx_equal( double x, double y, double tol = 1e-12 )
{
    return fabs( x - y ) <= tol * 0.5 * fabs( x + y );
}


/******************************************************************
 * Helper functions to pack/unpack data                            *
 ******************************************************************/
template<class TYPE>
static inline void pack_buffer( TYPE x, size_t& N_bytes, char* data )
{
    memcpy( &data[N_bytes], &x, sizeof( TYPE ) );
    N_bytes += sizeof( TYPE );
}
template<class TYPE>
static inline void unpack_buffer( TYPE& x, size_t& N_bytes, const char* data )
{
    memcpy( &x, &data[N_bytes], sizeof( TYPE ) );
    N_bytes += sizeof( TYPE );
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
static constexpr MPI_Datatype getType()
{
    if constexpr ( std::is_same_v<TYPE, std::byte> ) {
        return MPI_BYTE;
    } else if constexpr ( std::is_same_v<TYPE, char> ) {
        return MPI_CHAR;
    } else if constexpr ( std::is_same_v<TYPE, uint8_t> ) {
        return MPI_UINT8_T;
    } else if constexpr ( std::is_same_v<TYPE, int8_t> ) {
        return MPI_INT8_T;
    } else if constexpr ( std::is_same_v<TYPE, uint16_t> ) {
        return MPI_UINT16_T;
    } else if constexpr ( std::is_same_v<TYPE, int16_t> ) {
        return MPI_INT16_T;
    } else if constexpr ( std::is_same_v<TYPE, uint32_t> ) {
        return MPI_UINT32_T;
    } else if constexpr ( std::is_same_v<TYPE, int32_t> ) {
        return MPI_INT32_T;
    } else if constexpr ( std::is_same_v<TYPE, uint64_t> ) {
        return MPI_UINT64_T;
    } else if constexpr ( std::is_same_v<TYPE, int64_t> ) {
        return MPI_INT64_T;
    } else if constexpr ( std::is_same_v<TYPE, float> ) {
        return MPI_FLOAT;
    } else if constexpr ( std::is_same_v<TYPE, double> ) {
        return MPI_DOUBLE;
    } else {
        static_assert( !std::is_same_v<TYPE, TYPE> );
    }
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
#else // Helper functions to pack a value to a char array and increment N_bytes
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
}
#endif


/***********************************************************************
 * StoreTimes                                                           *
 ***********************************************************************/
ProfilerApp::StoreTimes::StoreTimes()
    : d_capacity( 0 ), d_size( 0 ), d_offset( 0 ), d_data( nullptr )
{
}
ProfilerApp::StoreTimes::StoreTimes( const StoreTimes& rhs, uint64_t shift )
    : d_capacity( 0 ), d_size( 0 ), d_offset( 0 ), d_data( nullptr )
{
    constexpr uint64_t max_diff = std::numeric_limits<uint16f>::max();
    uint64_t start2             = rhs.d_data[0] + shift;
    if ( start2 < max_diff && start2 < 20000 * ( rhs.d_data[1] - rhs.d_data[0] + 1 ) ) {
        reserve( rhs.d_size );
        memcpy( d_data, rhs.d_data, 2 * rhs.d_size * sizeof( uint16f ) );
        d_data[0] = uint16f( start2 );
        d_size    = rhs.d_size;
        d_offset  = rhs.d_offset + shift;
    } else {
        add( shift, shift );
        reserve( d_size + rhs.d_size );
        memcpy( &d_data[2 * d_size], rhs.d_data, 2 * rhs.d_size * sizeof( uint16f ) );
        d_size   = d_size + rhs.d_size;
        d_offset = rhs.d_offset + shift;
    }
}
ProfilerApp::StoreTimes::~StoreTimes() { free( d_data ); }
inline void ProfilerApp::StoreTimes::reserve( size_t N )
{
    d_capacity = N;
    d_data     = (uint16f*) realloc( d_data, 2 * d_capacity * sizeof( uint16f ) );
}
inline void ProfilerApp::StoreTimes::add( uint64_t start, uint64_t stop )
{
    // Allocate more memory if needed
    if ( d_size >= d_capacity ) {
        if ( d_size >= MAX_TRACE )
            return;
        size_t N = std::max<size_t>( 2 * d_capacity, 1024 );
        N        = std::min<size_t>( N, MAX_TRACE );
        reserve( N );
    }
    constexpr uint64_t max_diff = std::numeric_limits<uint16f>::max();
    uint64_t diff               = start - d_offset;
    uint16f tmp( diff );
    if ( stop - start > max_diff ) {
        // We are trying to store an interval that is too large, break it up
        add( start, start + max_diff );
        add( start + max_diff, stop );
    } else if ( diff > max_diff ) {
        // The interval to start is too large, break it up
        add( max_diff, max_diff );
        add( start, stop );
    } else if ( ( diff - tmp ) > ( stop - start + 1 ) ) {
        // We are loosing resolution
        d_data[2 * d_size + 0] = tmp;
        d_data[2 * d_size + 1] = uint16f( 0 );
        d_offset += d_data[2 * d_size + 0];
        d_size++;
        add( start, stop );
    } else {
        // Store the data
        d_data[2 * d_size + 0] = tmp;
        d_offset += d_data[2 * d_size + 0];
        d_data[2 * d_size + 1] = uint16f( stop - d_offset );
        d_offset += d_data[2 * d_size + 1];
        d_size++;
    }
}
uint16f* ProfilerApp::StoreTimes::take()
{
    auto ptr   = d_data;
    d_data     = nullptr;
    d_size     = 0;
    d_capacity = 0;
    d_offset   = 0;
    return ptr;
}


/***********************************************************************
 * StoreMemory                                                          *
 ***********************************************************************/
ProfilerApp::StoreMemory::StoreMemory()
    : d_capacity( 0 ), d_size( 0 ), d_time( nullptr ), d_bytes( nullptr )
{
}
ProfilerApp::StoreMemory::~StoreMemory()
{
    delete[] d_time;
    delete[] d_bytes;
}
inline void ProfilerApp::StoreMemory::add(
    uint64_t time, ProfilerApp::MemoryLevel level, volatile std::atomic_int64_t& bytes_profiler )
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
        bytes = MemoryApp::getTotalMemoryUsage();
#else
    NULL_USE( level );
    bytes = MemoryApp::getTotalMemoryUsage();
#endif
    bytes_profiler.fetch_sub( bytes );
    // Check if we need to allocate more memory
    if ( d_size == d_capacity ) {
        auto old_c = d_capacity;
        d_capacity *= 2;
        d_capacity = std::max<size_t>( d_capacity, 1024 );
        d_capacity = std::min<size_t>( d_capacity, MAX_ENTRIES );
        d_time     = (uint64_t*) realloc( d_time, d_capacity * sizeof( uint64_t ) );
        d_bytes    = (uint64_t*) realloc( d_bytes, d_capacity * sizeof( uint64_t ) );
        bytes_profiler.fetch_add( ( d_capacity - old_c ) * 16 );
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
void ProfilerApp::StoreMemory::reset() volatile
{
    d_size     = 0;
    d_capacity = 0;
    free( d_time );
    free( d_bytes );
    d_time  = nullptr;
    d_bytes = nullptr;
}
void ProfilerApp::StoreMemory::get(
    std::vector<uint64_t>& time, std::vector<uint64_t>& bytes ) const volatile
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
 * store_timer_data_info                                                *
 ***********************************************************************/
static inline void copyMessage( char* out, std::string_view in, size_t N )
{
    if ( in.size() + 1 < N ) {
        for ( size_t i = 0; i < in.size(); i++ )
            out[i] = in[i];
        out[in.size()] = 0;
    } else {
        for ( size_t i = 0; i < N - 4; i++ )
            out[i] = in[i];
        out[N - 4] = '.';
        out[N - 3] = '.';
        out[N - 2] = '.';
        out[N - 1] = 0;
    }
}
ProfilerApp::store_timer_data_info::store_timer_data_info()
{
    memset( this, 0, sizeof( store_timer_data_info ) );
    line = -1;
}
ProfilerApp::store_timer_data_info::store_timer_data_info(
    std::string_view msg, std::string_view filepath, uint64_t id0, int start )
{
    // Zero all data
    memset( this, 0, sizeof( store_timer_data_info ) );
    // Initialize key entries
    line = start;
    id   = id0;
    next = nullptr;
    // Copy the message, filename, path
    copyMessage( message, msg, sizeof( message ) );
    constexpr char split[] = { 47, 92, 0 };
    auto i                 = filepath.find_last_of( split );
    if ( i == std::string::npos ) {
        copyMessage( filename, filepath, sizeof( filename ) );
    } else {
        copyMessage( filename, filepath.substr( i + 1 ), sizeof( filename ) );
        copyMessage( path, filepath.substr( 0, i ), sizeof( path ) );
    }
}
ProfilerApp::store_timer_data_info::~store_timer_data_info()
{
    delete next;
    next = nullptr;
}
bool ProfilerApp::store_timer_data_info::compare( const store_timer_data_info& rhs ) const
{
    return memcmp( message, rhs.message, sizeof( message ) ) == 0 &&
           memcmp( filename, rhs.filename, sizeof( filename ) ) == 0;
}


/***********************************************************************
 * store_timer                                                          *
 ***********************************************************************/
ProfilerApp::store_timer::store_timer()
    : id( 0 ), trace_head( nullptr ), next( nullptr ), timer_data( nullptr )
{
}
ProfilerApp::store_timer::~store_timer()
{
    delete trace_head;
    delete next;
}


/***********************************************************************
 * store_timer                                                          *
 ***********************************************************************/
constexpr static uint64_t nullStart = static_cast<uint64_t>( (int64_t) -1 );
ProfilerApp::store_trace::store_trace( uint64_t s )
    : start( nullStart ),
      stack( s ),
      stack2( 0 ),
      N_calls( 0 ),
      min_time( std::numeric_limits<uint64_t>::max() ),
      max_time( 0 ),
      total_time( 0 ),
      next( nullptr )
{
}
ProfilerApp::store_trace::~store_trace() { delete next; }


/***********************************************************************
 * ThreadData                                                           *
 ***********************************************************************/
ProfilerApp::ThreadData::ThreadData() : id( 0 ), depth( 0 ), stack( 0 ), hash( 0 ), next( nullptr )
{
    static volatile std::atomic_uint32_t N_threads = 0;
    id                                             = N_threads++;
    hash = std::hash<std::thread::id>{}( std::this_thread::get_id() );
    for ( size_t i = 0; i < HASH_SIZE; i++ )
        timers[i] = nullptr;
}
ProfilerApp::ThreadData::~ThreadData()
{
    if ( next ) {
        next->~ThreadData(); // We allocated the data using malloc
        free( const_cast<ThreadData*>( next ) );
        next = nullptr;
    }
    for ( size_t i = 0; i < HASH_SIZE; i++ )
        delete timers[i];
}
void ProfilerApp::ThreadData::reset() volatile
{
    depth = 0;
    stack = 0;
    for ( size_t i = 0; i < HASH_SIZE; i++ ) {
        delete timers[i];
        timers[i] = nullptr;
    }
    memory.reset();
    if ( next )
        next->reset();
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
static constexpr uint64_t str_to_hash( std::string_view str )
{
    uint64_t key = 0;
    int N        = str.size();
    for ( int i = 0; i < N; i++ )
        key = ( key << 6 ) + to_int( str[N - i - 1] );
    return key;
}
static constexpr std::array<char, 12> hash_to_str( uint64_t key )
{
    // We will store the id use the 64 character set { 0-9 a-z A-Z & $ }
    std::array<char, 12> id = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
static_assert( sizeof( id_struct ) == 8, "Unexpected size for id_struct" );
static_assert( static_cast<uint64_t>( id_struct() ) == 0 );
static_assert( check_char_conversion(), "Internal error" );
static_assert( check_char_conversion(), "Internal error" );
static_assert( str_to_hash( hash_to_str( 0x32eb809d ).data() ) == 0x32eb809d );
static_assert( str_to_hash( hash_to_str( 0x35a5b6b25e9eb68f ).data() ) == 0x35a5b6b25e9eb68f );
static_assert(
    std::string_view( hash_to_str( str_to_hash( "O8luZAsKZG6" ) ).data() ) == "O8luZAsKZG6" );
static_assert( sizeof( id_struct ) <= sizeof( uint64_t ), "Unexpected size for id_struct" );
id_struct::id_struct( const std::string& rhs ) { data = str_to_hash( rhs ); }
id_struct::id_struct( const std::string_view rhs ) { data = str_to_hash( rhs ); }
id_struct::id_struct( const char* rhs ) { data = str_to_hash( rhs ); }
std::array<char, 12> id_struct::str() const { return hash_to_str( data ); }


/***********************************************************************
 * TraceResults                                                         *
 ***********************************************************************/
TraceResults::TraceResults()
    : thread( 0 ),
      rank( 0 ),
      N_trace( 0 ),
      min( std::numeric_limits<uint64_t>::max() ),
      max( 0 ),
      tot( 0 ),
      N( 0 ),
      stack( 0 ),
      stack2( 0 ),
      times( nullptr )
{
    ASSERT( str_to_hash( hash_to_str( 0x32eb809d ).data() ) == 0x32eb809d );
}
TraceResults::~TraceResults()
{
    free( times );
    times = nullptr;
}
TraceResults::TraceResults( TraceResults&& rhs )
    : id( rhs.id ),
      thread( rhs.thread ),
      rank( rhs.rank ),
      N_trace( rhs.N_trace ),
      min( rhs.min ),
      max( rhs.max ),
      tot( rhs.tot ),
      N( rhs.N ),
      stack( rhs.stack ),
      stack2( rhs.stack2 ),
      times( rhs.times )
{
    rhs.N_trace = 0;
    rhs.times   = nullptr;
}
TraceResults& TraceResults::operator=( TraceResults&& rhs )
{
    if ( this == &rhs )
        return *this;
    id          = rhs.id;
    thread      = rhs.thread;
    rank        = rhs.rank;
    N_trace     = rhs.N_trace;
    N           = rhs.N;
    min         = rhs.min;
    max         = rhs.max;
    tot         = rhs.tot;
    stack       = rhs.stack;
    stack2      = rhs.stack2;
    times       = rhs.times;
    rhs.N_trace = 0;
    rhs.times   = nullptr;
    return *this;
}
size_t TraceResults::size( bool store_trace ) const
{
    size_t bytes = 0;
    bytes += sizeof( id );
    bytes += sizeof( thread );
    bytes += sizeof( rank );
    bytes += sizeof( N_trace );
    bytes += sizeof( min );
    bytes += sizeof( max );
    bytes += sizeof( tot );
    bytes += sizeof( N );
    bytes += sizeof( stack );
    bytes += sizeof( stack2 );
    if ( store_trace )
        bytes += 2 * N_trace * sizeof( uint16f );
    return bytes;
}
size_t TraceResults::pack( char* data, bool store_trace ) const
{
    auto N_trace0 = N_trace;
    auto* this2   = const_cast<TraceResults*>( this );
    if ( !store_trace )
        this2->N_trace = 0;
    size_t pos = 0;
    pack_buffer( id, pos, data );
    pack_buffer( thread, pos, data );
    pack_buffer( rank, pos, data );
    pack_buffer( N_trace, pos, data );
    pack_buffer( min, pos, data );
    pack_buffer( max, pos, data );
    pack_buffer( tot, pos, data );
    pack_buffer( N, pos, data );
    pack_buffer( stack, pos, data );
    pack_buffer( stack2, pos, data );
    if ( N_trace > 0 && store_trace ) {
        memcpy( &data[pos], times, 2 * N_trace * sizeof( uint16f ) );
        pos += 2 * N_trace * sizeof( uint16f );
    }
    this2->N_trace = N_trace0;
    return pos;
}
size_t TraceResults::unpack( const char* data )
{
    delete[] times;
    size_t pos = 0;
    unpack_buffer( id, pos, data );
    unpack_buffer( thread, pos, data );
    unpack_buffer( rank, pos, data );
    unpack_buffer( N_trace, pos, data );
    unpack_buffer( min, pos, data );
    unpack_buffer( max, pos, data );
    unpack_buffer( tot, pos, data );
    unpack_buffer( N, pos, data );
    unpack_buffer( stack, pos, data );
    unpack_buffer( stack2, pos, data );
    times = nullptr;
    if ( N_trace > 0 ) {
        times = (uint16f*) malloc( 2 * N_trace * sizeof( uint16f ) );
        memcpy( times, &data[pos], 2 * N_trace * sizeof( uint16f ) );
        pos += 2 * N_trace * sizeof( uint16f );
    }
    return pos;
}
bool TraceResults::operator==( const TraceResults& rhs ) const
{
    bool equal = id == rhs.id;
    equal      = equal && thread == rhs.thread;
    equal      = equal && rank == rhs.rank;
    equal      = equal && N_trace == rhs.N_trace;
    equal      = equal && approx_equal( min, rhs.min, 0.005 );
    equal      = equal && approx_equal( max, rhs.max, 0.005 );
    equal      = equal && approx_equal( tot, rhs.tot, 0.005 );
    equal      = equal && N == rhs.N;
    equal      = equal && stack == rhs.stack;
    equal      = equal && stack2 == rhs.stack2;
    return equal;
}


/***********************************************************************
 * TimerResults                                                         *
 ***********************************************************************/
TimerResults::TimerResults() : line( 0 )
{
    memset( message, 0, sizeof( message ) );
    memset( file, 0, sizeof( file ) );
    memset( path, 0, sizeof( path ) );
}
size_t TimerResults::size( bool store_trace ) const
{
    size_t bytes = sizeof( id );
    bytes += sizeof( line );
    bytes += sizeof( int );
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
    size_t pos = sizeof( id );
    int size   = trace.size();
    memcpy( &data[pos], &line, sizeof( line ) );
    pos += sizeof( line );
    memcpy( &data[pos], &size, sizeof( size ) );
    pos += sizeof( size );
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
    int size   = 0;
    memcpy( &line, &data[pos], sizeof( line ) );
    pos += sizeof( line );
    memcpy( &size, &data[pos], sizeof( size ) );
    pos += sizeof( size );
    memcpy( message, &data[pos], sizeof( message ) );
    pos += sizeof( message );
    memcpy( file, &data[pos], sizeof( file ) );
    pos += sizeof( file );
    memcpy( path, &data[pos], sizeof( path ) );
    pos += sizeof( path );
    trace.resize( size );
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
    equal      = equal && line == rhs.line;
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
    size_t bytes = 3 * sizeof( int ) + sizeof( walltime );
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
    memcpy( data + pos, &walltime, sizeof( walltime ) );
    pos += sizeof( walltime );
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
    memcpy( &walltime, data + pos, sizeof( walltime ) );
    pos += sizeof( walltime );
    N_procs = tmp[0];
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
    equal      = equal && walltime == rhs.walltime;
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
ProfilerApp* ProfilerApp::getInstantance() { return &global_profiler; }
ProfilerApp::ProfilerApp()
    : d_timer_table{ nullptr }, d_construct_time( std::chrono::steady_clock::now() )
{
    if ( &global_profiler != this )
        throw std::logic_error( "Creating multiple instances of ProfilerApp is not supported, "
                                "please use getInstantance()" );
    for ( auto& tmp : d_threadData )
        tmp = nullptr;
    for ( auto& tmp : d_timer_table )
        tmp = nullptr;
    d_level               = -1;
    d_shift               = 0;
    d_store_trace_data    = false;
    d_store_memory_data   = MemoryLevel::None;
    d_disable_timer_error = false;
    d_bytes               = sizeof( ProfilerApp );
    // Get the current thread to set it's id to 0
    auto thread = getThreadData();
    NULL_USE( thread );
}
ProfilerApp::~ProfilerApp()
{
    disable();
    for ( auto& tmp : d_threadData ) {
        if ( tmp ) {
            tmp->~ThreadData(); // We allocated the data using malloc
            free( const_cast<ThreadData*>( tmp ) );
            tmp = nullptr;
        }
    }
    for ( auto& tmp : d_timer_table ) {
        delete tmp;
        tmp = nullptr;
    }
}
void ProfilerApp::setStoreTrace( bool profile ) { d_store_trace_data = profile; }
void ProfilerApp::setStoreMemory( MemoryLevel memory ) { d_store_memory_data = memory; }
ProfilerApp::MemoryLevel ProfilerApp::getStoreMemory() { return d_store_memory_data; }


/***********************************************************************
 * Function to synchronize the timers                                    *
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
 * Function to handle any errors                                        *
 ***********************************************************************/
void ProfilerApp::error(
    const std::string& message, const ThreadData* thread, const store_timer* timer )
{
    std::cerr << "Internal error in Profiler:\n" << message << std::endl;
    if ( thread )
        std::cerr << "   thread: " << thread->id << " " << thread->depth << " " << thread->stack
                  << std::endl;
    if ( timer )
        std::cerr << "   timer: " << timer->timer_data->message << " "
                  << timer->timer_data->filename << " " << timer->timer_data->line << std::endl;
    std::cerr << "Profiling will be disabled\n";
    disable();
}


/***********************************************************************
 * Function to create thread data                                       *
 ***********************************************************************/
ProfilerApp::ThreadData* ProfilerApp::createThreadData()
{
    constexpr uint64_t mask = HASH_SIZE - 1;
    global_profiler.d_lock.lock();
    auto id      = std::hash<std::thread::id>{}( std::this_thread::get_id() );
    uint64_t key = id & mask; // Get the hash index
    auto ptr     = &global_profiler.d_threadData[key];
    while ( *ptr != nullptr ) {
        assert( ( *ptr )->id != id );
        ptr = &( ( *ptr )->next );
    }
    auto mem = malloc( sizeof( ThreadData ) );
    *ptr     = new ( mem ) ThreadData();
    global_profiler.d_lock.unlock();
    return const_cast<ProfilerApp::ThreadData*>( *ptr );
}


/***********************************************************************
 * Function to start profiling a block of code                          *
 ***********************************************************************/
ProfilerApp::store_trace* ProfilerApp::start( store_timer* timer )
{
    auto& thread = *getThreadData();
    // Update the stack
    auto stack = thread.stack;
    thread.stack =
        ( ( thread.stack << 7 ) | ( thread.stack >> 57 ) ) ^ ( timer->id + 13 * thread.depth );
    thread.depth++;
    // Find the trace to start (creating if needed)
    store_trace* trace = nullptr;
    if ( timer->trace_head ) {
        trace     = timer->trace_head;
        auto last = trace;
        while ( trace ) {
            if ( trace->stack == stack )
                break;
            last  = trace;
            trace = trace->next;
        }
        if ( !trace ) {
            last->next    = new store_trace( stack );
            trace         = last->next;
            trace->stack2 = thread.stack;
            d_bytes.fetch_add( sizeof( store_trace ) );
        }
    } else {
        timer->trace_head = new store_trace( stack );
        trace             = timer->trace_head;
        trace->stack2     = thread.stack;
        d_bytes.fetch_add( sizeof( store_trace ) );
    }
    // Start the timer
    if ( trace->start != nullStart ) {
        error( "Trace is active", &thread, timer );
        return nullptr;
    }
    trace->start = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
    // Record the memory usage
    if ( static_cast<int>( d_store_memory_data ) >= 2 )
        thread.memory.add( trace->start, d_store_memory_data, d_bytes );
    return trace;
}


/***********************************************************************
 * Function to stop profiling a block of code                           *
 ***********************************************************************/
void ProfilerApp::stop( store_timer* timer, time_point end_time, int enableTrace )
{
    auto& thread = *getThreadData();
    // Update the stack
    auto stack2 = thread.stack;
    thread.depth--;
    uint64_t tmp = thread.stack ^ ( timer->id + 13 * thread.depth );
    thread.stack = ( tmp << 57 ) | ( tmp >> 7 );
    // Find the trace to start (creating if needed)
    auto trace = timer->trace_head;
    while ( trace ) {
        if ( trace->stack == thread.stack )
            break;
        trace = trace->next;
    }
    if ( !trace ) {
        error( "Unable to find trace, possible corrupted stack", &thread, timer );
        return;
    }
    if ( trace->stack2 != stack2 || trace->stack != thread.stack ) {
        error( "Corrupted stack", &thread, timer );
        return;
    }
    // Stop the trace
    auto start      = trace->start;
    uint64_t stop   = diff_ns( end_time, d_construct_time );
    uint64_t ns     = stop - start;
    trace->start    = nullStart;
    trace->max_time = std::max( trace->max_time, ns );
    trace->min_time = std::min( trace->min_time, ns );
    trace->total_time += ns;
    trace->N_calls++;
    // Save the starting and ending time if we are storing the detailed traces
    if ( enableTrace == -1 )
        enableTrace = d_store_trace_data ? 1 : 0;
    if ( enableTrace )
        trace->times.add( start, stop );
    // Get the memory usage
    if ( static_cast<int8_t>( d_store_memory_data ) >= 2 )
        thread.memory.add( stop, d_store_memory_data, d_bytes );
}
void ProfilerApp::stop( store_trace* trace, time_point end_time, int enableTrace )
{
    auto& thread = *getThreadData();
    // Update the stack
    if ( trace->stack2 != thread.stack ) {
        error( "Corrupted stack", &thread, nullptr );
        return;
    }
    thread.stack = trace->stack;
    thread.depth--;
    // Stop the trace
    auto start      = trace->start;
    uint64_t stop   = diff_ns( end_time, d_construct_time );
    uint64_t ns     = stop - start;
    trace->start    = nullStart;
    trace->max_time = std::max( trace->max_time, ns );
    trace->min_time = std::min( trace->min_time, ns );
    trace->total_time += ns;
    trace->N_calls++;
    // Save the starting and ending time if we are storing the detailed traces
    if ( enableTrace == -1 )
        enableTrace = d_store_trace_data ? 1 : 0;
    if ( enableTrace )
        trace->times.add( start, stop );
    // Get the memory usage
    if ( static_cast<int8_t>( d_store_memory_data ) >= 2 )
        thread.memory.add( stop, d_store_memory_data, d_bytes );
}


/***********************************************************************
 * Function to store current memory usage                               *
 ***********************************************************************/
void ProfilerApp::memory()
{
    if ( static_cast<int8_t>( d_store_memory_data ) >= 2 ) {
        int64_t ns  = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
        auto thread = getThreadData();
        thread->memory.add( ns, d_store_memory_data, d_bytes );
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
    if ( d_level < 0 )
        d_construct_time = std::chrono::steady_clock::now();
    d_level = level;
    d_lock.unlock();
}
void ProfilerApp::disable()
{
    // First, change the status flag
    d_lock.lock();
    d_level = -1;
    // Brief pause to ensure if any timers are in the process of updating they finish first
    std::this_thread::sleep_for( std::chrono::microseconds( 10 ) );
    // Reset thread specific data
    for ( auto& thread : d_threadData ) {
        if ( thread )
            thread->reset();
    }
    // Clear timer data
    for ( auto& timer : d_timer_table ) {
        delete timer;
        timer = nullptr;
    }
    d_bytes = 0;
    d_lock.unlock();
}


/***********************************************************************
 * Function to return the profiling info                                *
 ***********************************************************************/
inline void ProfilerApp::getTimerResultsID(
    uint64_t id, int rank, const time_point& end_time, TimerResults& results ) const
{
    constexpr uint64_t mask = HASH_SIZE - 1;
    uint64_t key            = id & mask; // Get the hash index
    results.id              = id_struct( id );
    // Search for the global timer info
    auto* timer_global = const_cast<store_timer_data_info*>( d_timer_table[key] );
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
    results.line = timer_global->line;
    // Loop through the thread entries
    for ( auto threadHead : d_threadData ) {
        auto thread = threadHead;
        while ( thread ) {
            auto thread_id = thread->id;
            // Search for a timer that matches the current id
            auto timer = thread->timers[key];
            while ( timer != nullptr ) {
                if ( timer->id == id )
                    break;
                timer = timer->next;
            }
            if ( timer == nullptr ) {
                // The current thread does not have a copy of this timer, move on
                thread = thread->next;
                continue;
            }
            // Loop through the trace entries
            uint64_t stop      = diff_ns( end_time, d_construct_time );
            store_trace* trace = timer->trace_head;
            while ( trace != nullptr ) {
                size_t k = results.trace.size();
                results.trace.resize( k + 1 );
                // Get the running times of the trace
                results.trace[k].id      = results.id;
                results.trace[k].thread  = thread_id;
                results.trace[k].rank    = rank;
                results.trace[k].N       = trace->N_calls;
                results.trace[k].N_trace = 0;
                results.trace[k].min     = trace->min_time;
                results.trace[k].max     = trace->max_time;
                results.trace[k].tot     = trace->total_time;
                results.trace[k].stack   = id_struct( trace->stack );
                results.trace[k].stack2  = id_struct( trace->stack2 );
                // Check if the trace is still running and update
                if ( trace->start != nullStart ) {
                    uint64_t ns = stop - trace->start;
                    results.trace[k].N++;
                    results.trace[k].min = std::min<float>( results.trace[k].min, ns );
                    results.trace[k].max = std::max<float>( results.trace[k].max, ns );
                    results.trace[k].tot += ns;
                }
                // Save the detailed trace results
                if ( trace->times.size() > 0 ) {
                    StoreTimes times( trace->times, d_shift );
                    results.trace[k].N_trace = times.size();
                    results.trace[k].times   = times.take();
                }
                if ( d_store_trace_data && trace->N_calls == 0 && trace->start != nullStart ) {
                    StoreTimes times;
                    times.add( d_shift + trace->start, d_shift + stop );
                    results.trace[k].N_trace = times.size();
                    results.trace[k].times   = times.take();
                }
                // Advance to the next trace
                trace = trace->next;
            }
            thread = thread->next;
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
    // Get a list of all timer ids
    std::vector<size_t> ids;
    for ( auto i : d_timer_table ) {
        auto* timer_global = const_cast<store_timer_data_info*>( i );
        while ( timer_global != nullptr ) {
            ids.push_back( timer_global->id );
            timer_global = const_cast<store_timer_data_info*>( timer_global->next );
        }
    }
    // Begin storing the timers
    std::vector<TimerResults> results( ids.size() );
    for ( size_t i = 0; i < ids.size(); i++ )
        getTimerResultsID( ids[i], rank, end_time, results[i] );
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
    // Get the timer
    TimerResults results;
    getTimerResultsID( id, rank, end_time, results );
    // Release the mutex
    d_lock.unlock();
    return results;
}
TimerResults ProfilerApp::getTimerResults( std::string_view message, std::string_view file ) const
{
    // Get the current time in case we need to "stop" and timers
    auto end_time = std::chrono::steady_clock::now();
    int rank      = comm_rank();
    // Get a lock
    d_lock.lock();
    // Find the timer
    uint64_t id = 0;
    store_timer_data_info tmp( message.data(), file.data(), 0, 0 );
    for ( auto timer : d_timer_table ) {
        while ( id == 0 && timer != nullptr ) {
            if ( const_cast<store_timer_data_info*>( timer )->compare( tmp ) )
                id = timer->id;
            timer = timer->next;
        }
        if ( id != 0 )
            break;
    }
    // Get the timer
    TimerResults results;
    getTimerResultsID( id, rank, end_time, results );
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
    if ( static_cast<int8_t>( d_store_memory_data ) >= 2 ) {
        auto thread = const_cast<ProfilerApp*>( this )->getThreadData();
        int64_t ns  = diff_ns( std::chrono::steady_clock::now(), d_construct_time );
        thread->memory.add( ns, d_store_memory_data, d_bytes );
    }
    // Get the memory info for each thread
    size_t N = 0;
    std::vector<std::vector<uint64_t>> time_list, bytes_list;
    for ( auto threadHead : d_threadData ) {
        auto thread = threadHead;
        while ( thread ) {
            std::vector<uint64_t> time, bytes;
            thread->memory.get( time, bytes );
            if ( !time.empty() ) {
                N += time.size();
                time_list.emplace_back( std::move( time ) );
                bytes_list.emplace_back( std::move( bytes ) );
            }
            thread = thread->next;
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
        int N2 = time_list.size();
        std::vector<size_t> index( time_list.size(), 0 );
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
    if ( N > 10 ) {
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
        time.resize( N );
        bytes.resize( N );
    }
    // Create the memory results
    MemoryResults data;
    data.rank  = 0;
    data.time  = std::move( time );
    data.bytes = std::move( bytes );
    return data;
}


/***********************************************************************
 * Function to save the profiling info                                  *
 ***********************************************************************/
static bool isRecursive( const TimerResults& timer, const TraceResults& trace,
    const std::vector<uint64_t>& stackIDs, const std::vector<std::vector<uint64_t>>& stackList )
{
    int i = binarySearch<uint64_t>( stackIDs, trace.stack );
    ASSERT( i != -1 );
    bool found = false;
    for ( auto& trace2 : timer.trace )
        found = found || binarySearch<uint64_t>( stackList[i], trace2.stack2 ) != -1;
    return found;
};
void ProfilerApp::save( const std::string& filename, bool global ) const
{
    if ( this->d_level < 0 ) {
        std::cout << "Warning: Timers are not enabled, no data will be saved\n";
        return;
    }
    const int N_procs = comm_size();
    const int rank    = comm_rank();
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
    double walltime = 1e-9 * diff_ns( std::chrono::steady_clock::now(), d_construct_time );
    auto results    = getTimerResults();
    if ( global ) {
        // Gather the timers from all files (rank 0 will do all writing)
        gatherTimers( results );
    }
    int N_threads = 0;
    for ( auto& timer : results ) {
        for ( auto& trace : timer.trace )
            N_threads = std::max( N_threads, trace.thread + 1 );
    }
    if ( !results.empty() ) {
        // Get active timer set
        auto [stackIDs, stackList] = buildStackMap( results );
        // Get the timer ids and sort the ids by the total time
        // to create a global order to save the results
        std::vector<size_t> id_order( results.size(), 0 );
        std::vector<double> total_time( results.size(), 0 );
        for ( size_t i = 0; i < results.size(); i++ ) {
            id_order[i]   = i;
            total_time[i] = 0.0;
            std::vector<double> time_thread( N_threads, 0 );
            for ( auto& trace : results[i].trace ) {
                if ( !isRecursive( results[i], trace, stackIDs, stackList ) )
                    time_thread[trace.thread] += trace.tot;
            }
            for ( int j = 0; j < N_threads; j++ )
                total_time[i] = std::max<double>( total_time[i], time_thread[j] );
        }
        quicksort( total_time, id_order );
        // Open the file(s) for writing
        FILE* timerFile = fopen( filename_timer, "wb" );
        if ( timerFile == nullptr ) {
            std::cerr << "Error opening file for writing (timer)";
            return;
        }
        FILE* traceFile = nullptr;
        for ( auto it = results.begin(); it != results.end() && !traceFile; ++it ) {
            for ( auto& trace : it->trace ) {
                if ( trace.times ) {
                    traceFile = fopen( filename_trace, "wb" );
                    if ( traceFile == nullptr ) {
                        std::cerr << "Error opening file for writing (trace)";
                        fclose( timerFile );
                        return;
                    }
                    break;
                }
            }
        }
        // Create the file header
        char header[] = "                  Message                      Filename           Line"
                        "   Thread    N_calls   Min Time  Max Time  Total Time  %% Time\n"
                        "---------------------------------------------------------------------"
                        "---------------------------------------------------------------\n";
        fprintf( timerFile, "%s", header );
        // Loop through the list of timers, storing the most expensive first
        for ( int ii = static_cast<int>( results.size() ) - 1; ii >= 0; ii-- ) {
            size_t i = id_order[ii];
            std::set<uint64_t> stack2;
            for ( auto& trace : results[i].trace )
                stack2.insert( trace.stack2 );
            std::vector<int> N_thread( N_threads, 0 );
            std::vector<double> min_thread( N_threads, 1e99 );
            std::vector<double> max_thread( N_threads, 0.0 );
            std::vector<double> tot_thread( N_threads, 0.0 );
            for ( auto& trace : results[i].trace ) {
                int k = trace.thread;
                N_thread[k] += trace.N;
                min_thread[k] = std::min( min_thread[k], 1e-9 * trace.min );
                max_thread[k] = std::max( max_thread[k], 1e-9 * trace.max );
                if ( !isRecursive( results[i], trace, stackIDs, stackList ) )
                    tot_thread[k] += 1e-9 * trace.tot;
            }
            for ( int j = 0; j < N_threads; j++ ) {
                if ( N_thread[j] == 0 )
                    continue;
                // Save the timer to the file
                // Note: we always want one space in front in case the timer starts
                //    with '<' and is long.
                fprintf( timerFile,
                    " %29s  %30s   %5i   %5i    %8i   %8.3f  %8.3f  %10.3f  %6.1f\n",
                    results[i].message, results[i].file, results[i].line, j, N_thread[j],
                    min_thread[j], max_thread[j], tot_thread[j], 100 * tot_thread[j] / walltime );
            }
        }
        // Loop through all of the entries, saving the detailed data and the trace logs
        fprintf( timerFile, "\n\n\n" );
        fprintf( timerFile, "<N_procs=%i,id=%i", N_procs, rank );
        fprintf( timerFile, ",store_trace=%i", traceFile ? 1 : 0 );
        fprintf( timerFile, ",store_memory=%i", d_store_memory_data != MemoryLevel::None ? 1 : 0 );
        fprintf( timerFile, ",walltime=%e", walltime );
        fprintf( timerFile, ",date='%s'>\n", getDateString().c_str() );
        // Loop through the list of timers, storing the most expensive first
        for ( int ii = static_cast<int>( results.size() ) - 1; ii >= 0; ii-- ) {
            size_t i = id_order[ii];
            // Store the basic timer info
            const char e = 0x0E; // Escape character for printing strings
            fprintf( timerFile, "<timer:id=%s,message=%c%s%c,file=%c%s%c,path=%c%s%c,line=%i>\n",
                results[i].id.str().data(), e, results[i].message, e, e, results[i].file, e, e,
                results[i].path, e, results[i].line );
            // Store the trace data
            for ( const auto& trace : results[i].trace ) {
                unsigned long N = trace.N;
                fprintf( timerFile,
                    "<trace:id=%s,thread=%u,rank=%u,N=%lu,min=%e,max=%e,tot=%e,stack=[%s;%s]>\n",
                    trace.id.str().data(), trace.thread, trace.rank, N, 1e-9 * trace.min,
                    1e-9 * trace.max, 1e-9 * trace.tot, hash_to_str( trace.stack ).data(),
                    hash_to_str( trace.stack2 ).data() );
                // Save the detailed trace results (this is a binary file)
                if ( trace.N_trace > 0 ) {
                    unsigned long Nt = trace.N_trace;
                    fprintf( traceFile, "<id=%s,thread=%u,rank=%u,stack=%s,N=%lu,format=uint16f>\n",
                        trace.id.str().data(), trace.thread, trace.rank,
                        hash_to_str( trace.stack ).data(), Nt );
                    fwrite( trace.times, sizeof( uint16f ), 2 * Nt, traceFile );
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
                static_cast<long int>( count ), "double", "uint32", units.c_str(), mem.rank );
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
static inline std::string readLine( FILE* fid )
{
    char memory[0x10000];
    char* rtn = fgets( memory, sizeof( memory ), fid );
    if ( rtn == nullptr )
        return std::string();
    if ( rtn[0] <= 10 ) {
        rtn[0] = '\1';
        return rtn;
    }
    rtn[sizeof( memory ) - 1] = 0;
    for ( size_t i = 0; i < sizeof( memory ); i++ ) {
        if ( rtn[i] < 32 || rtn[i] == '\n' ) {
            rtn[i] = 0;
            break;
        }
    }
    return std::string( rtn );
}
static inline std::string_view getField( std::string_view line, std::string_view name )
{
    size_t i = line.find( name );
    if ( i == std::string::npos )
        return std::string_view();
    size_t j1 = line.find( "=", i );
    size_t j2 = line.find_first_of( ",>\n\0", j1 );
    return line.substr( j1 + 1, j2 - j1 - 1 );
}
static inline bool isField( std::string_view line, std::string_view name )
{
    return line.find( name ) != std::string::npos;
}
template<class T>
static inline T convert( std::string_view field )
{
    if constexpr ( std::is_integral_v<T> ) {
        char* end = const_cast<char*>( field.data() + field.size() );
        if constexpr ( std::numeric_limits<T>::is_signed )
            return strtoll( field.data(), &end, 10 );
        else
            return strtoull( field.data(), &end, 10 );
    } else if constexpr ( std::is_floating_point_v<T> ) {
        char* end = const_cast<char*>( field.data() + field.size() );
        return strtod( field.data(), &end );
    } else {
        static_assert( !std::is_same_v<T, T> );
    }
}
static size_t getFieldArray(
    const char* line0, std::vector<std::pair<std::string_view, std::string_view>>& data )
{
    // This function parses a line of the form <field=value,field=value>
    const char e = 0x0E; // Escape character for printing strings
    // Find the line of interest
    ASSERT( line0[0] == '<' );
    int count = 0;
    size_t i0 = 0;
    while ( ( line0[i0] != '>' || count % 2 == 1 ) && line0[i0] != 0 ) {
        if ( line0[i0] == e )
            count++;
        i0++;
    }
    ASSERT( line0[i0] == '>' );
    std::string_view line( &line0[1], i0 - 1 );
    data.clear();
    while ( true ) {
        auto i = line.find( '=' );
        ASSERT( i != std::string::npos );
        auto key = line.substr( 0, i );
        std::string_view value;
        size_t j = 0;
        if ( line[i + 1] == e ) {
            j = line.find( e, i + 2 );
            ASSERT( j != std::string::npos );
            value = line.substr( i + 2, j - i - 2 );
            j     = std::min( line.find( ',', j ), line.size() );
        } else {
            j     = std::min( line.find( ',', i + 1 ), line.size() );
            value = line.substr( i + 1, j - i - 1 );
        }
        data.emplace_back( key, value );
        if ( j + 1 >= line.size() )
            break;
        line = line.substr( j + 1 );
    }
    return i0 + 1;
}
static std::vector<id_struct> getActiveIds( std::string_view str )
{
    std::vector<id_struct> ids;
    while ( str.find_first_of( " []" ) == 0 )
        str = str.substr( 1 );
    while ( str.find_last_of( " []" ) == ( str.size() - 1 ) && !str.empty() )
        str = str.substr( 0, str.size() - 1 );
    if ( str.empty() )
        return {};
    while ( !str.empty() ) {
        size_t i  = std::min( str.find( ' ' ), str.size() );
        auto str2 = str.substr( 0, i );
        ids.emplace_back( str2 );
        str = str.substr( std::min( i + 1, str.size() ) );
    }
    return ids;
}
static std::tuple<id_struct, id_struct> loadActive( std::string_view str, id_struct id )
{
    auto ids       = getActiveIds( str );
    uint64_t stack = 0;
    for ( size_t i = 0; i < ids.size(); i++ )
        stack = stack ^ static_cast<uint64_t>( ids[i] );
    id_struct stack1( stack );
    id_struct stack2( stack ^ static_cast<uint64_t>( id ) );
    return std::make_tuple( stack1, stack2 );
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
    loadTimer( timer, data.timers, N_procs, data.walltime, date, trace_data, memory_data );
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
            std::swap( data[i2], data[i1] );
            i2++;
        }
    }
    data.resize( i2 );
}
static inline void copyText( char* out, std::string_view in, size_t N )
{
    memset( out, 0, N );
    strncpy( out, in.data(), std::min( N - 1, in.size() ) );
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
    // Clear any timers that are empty (missing on the current rank)
    size_t N = 0;
    for ( size_t i = 0; i < data.timers.size(); i++ ) {
        if ( !data.timers[i].trace.empty() )
            std::swap( data.timers[N++], data.timers[i] );
    }
    data.timers.resize( N );
    return data;
}
void ProfilerApp::loadTimer( const std::string& filename, std::vector<TimerResults>& data,
    int& N_procs, double& walltime, std::string& date, bool& trace_data, bool& memory_data )
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
    walltime    = -1;
    trace_data  = false;
    memory_data = false;
    date        = std::string();
    std::vector<std::pair<std::string_view, std::string_view>> fields;
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
        line += getFieldArray( line, fields );
        if ( fields[0].first == "N_procs" ) {
            // We are loading the header
            N_procs = convert<int>( fields[0].second );
            // Load the remaining fields
            for ( size_t i = 1; i < fields.size(); i++ ) {
                if ( fields[i].first == "id" || fields[i].first == "rank" ) {
                    // Load the id/rank
                    rank = convert<int>( fields[i].second );
                } else if ( fields[i].first == "store_trace" ) {
                    // Check if we stored the trace file
                    trace_data = convert<int>( fields[i].second ) == 1;
                } else if ( fields[i].first == "store_memory" ) {
                    // Check if we stored the memory file (optional)
                    memory_data = convert<int>( fields[i].second ) == 1;
                } else if ( fields[i].first == "walltime" ) {
                    // Check if we stored the total wallclock time
                    walltime = convert<double>( fields[i].second );
                } else if ( fields[i].first == "date" ) {
                    // Load the date (optional)
                    date = std::string( fields[i].second );
                } else {
                    throw std::logic_error(
                        "Unknown field (header): " + std::string( fields[i].first ) );
                }
            }
        } else if ( fields[0].first == "timer:id" ) {
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
                    if ( fields[i].first == "message" ) {
                        // Load the message
                        copyText( timer.message, fields[i].second, sizeof( timer.message ) );
                    } else if ( fields[i].first == "file" ) {
                        // Load the filename
                        copyText( timer.file, fields[i].second, sizeof( timer.file ) );
                    } else if ( fields[i].first == "path" ) {
                        // Load the path
                        copyText( timer.path, fields[i].second, sizeof( timer.path ) );
                    } else if ( fields[i].first == "start" || fields[i].first == "line" ) {
                        // Load the start line
                        timer.line = convert<int>( fields[i].second );
                    } else if ( fields[i].first == "stop" ) {
                        // Load the stop line (obsolete)
                    } else if ( fields[i].first == "thread" || fields[i].first == "N" ||
                                fields[i].first == "min" || fields[i].first == "max" ||
                                fields[i].first == "tot" ) {
                        // Obsolete fields
                    } else {
                        throw std::logic_error(
                            "Unknown field (timer): " + std::string( fields[i].first ) );
                    }
                }
            }
        } else if ( fields[0].first, "trace:id" ) {
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
                if ( fields[i].first == "thread" ) {
                    // Load the thread id
                    trace.thread = convert<int>( fields[i].second );
                } else if ( fields[i].first == "rank" ) {
                    // Load the rank id
                    trace.rank = convert<int>( fields[i].second );
                } else if ( fields[i].first == "N" ) {
                    // Load N
                    trace.N = convert<int>( fields[i].second );
                } else if ( fields[i].first == "min" ) {
                    // Load min
                    trace.min = 1e9 * convert<double>( fields[i].second );
                } else if ( fields[i].first == "max" ) {
                    // Load max
                    trace.max = 1e9 * convert<double>( fields[i].second );
                } else if ( fields[i].first == "tot" ) {
                    // Load tot
                    trace.tot = 1e9 * convert<double>( fields[i].second );
                } else if ( fields[i].first == "stack" ) {
                    // Load stack
                    std::string_view tmp( fields[i].second );
                    auto i1 = fields[i].second.find( '[' );
                    auto i2 = fields[i].second.find( ';' );
                    auto i3 = fields[i].second.find( ']' );
                    ASSERT(
                        i1 == 0 && i2 != std::string::npos && i3 == fields[i].second.size() - 1 );
                    auto s1      = fields[i].second.substr( i1 + 1, i2 - i1 - 1 );
                    auto s2      = fields[i].second.substr( i2 + 1, i3 - i2 - 1 );
                    trace.stack  = str_to_hash( s1 );
                    trace.stack2 = str_to_hash( s2 );
                    ASSERT( s1 == std::string_view( hash_to_str( trace.stack ).data() ) );
                    ASSERT( s2 == std::string_view( hash_to_str( trace.stack2 ).data() ) );
                } else if ( fields[i].first == "active" ) {
                    // Load the active timers
                    std::tie( trace.stack, trace.stack2 ) = loadActive( fields[i].second, id );
                } else {
                    throw std::logic_error(
                        "Unknown field (trace): " + std::string( fields[i].first ) );
                }
            }
        } else {
            throw std::logic_error( "Unknown data field: " + std::string( fields[0].first ) );
        }
    }
    // Fill walltime with largest timer (if it does not exist for backward compatibility)
    if ( walltime < 0 ) {
        walltime = 0;
        for ( const auto& timer : data ) {
            for ( const auto& trace : timer.trace )
                walltime = std::max( walltime, 1e-9 * trace.tot );
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
    while ( true ) {
        // Read the header
        auto line = readLine( fid );
        if ( line.empty() ) {
            fclose( fid );
            return;
        }
        if ( line[0] == '\1' )
            continue;
        // Get the id and find the appropriate timer
        auto field = getField( line, "id=" );
        ASSERT( !field.empty() );
        id_struct id( field );
        auto it = id_map.find( id );
        if ( it == id_map.end() )
            throw std::logic_error( "Did not find matching timer" );
        TimerResults& timer = data[it->second];
        // Read the remaining trace header data
        field = getField( line, "thread=" );
        ASSERT( !field.empty() );
        auto thread = convert<unsigned int>( field );
        field       = getField( line, "rank=" );
        ASSERT( !field.empty() );
        auto rank = convert<unsigned int>( field );
        uint64_t stack;
        if ( isField( line, "stack=" ) ) {
            field = getField( line, "stack=" );
            stack = str_to_hash( field );
        } else {
            field = getField( line, "active=" );
            ASSERT( !field.empty() );
            std::tie( stack, std::ignore ) = loadActive( field, id );
        }
        field = getField( line, "N=" );
        ASSERT( !field.empty() );
        unsigned long int N = convert<uint64_t>( field );
        field               = getField( line, "format=" );
        int format          = 0;
        if ( !field.empty() ) {
            if ( field == "double" )
                format = 0;
            else if ( field == "uint16f" )
                format = 1;
            else
                throw std::logic_error( "Unknown format" );
        }
        // Find the appropriate trace
        int index = -1;
        for ( size_t i = 0; i < timer.trace.size(); i++ ) {
            if ( timer.trace[i].thread == thread && timer.trace[i].rank == rank &&
                 timer.trace[i].stack == stack )
                index = static_cast<int>( i );
        }
        ASSERT( index != -1 );
        TraceResults& trace = timer.trace[index];
        trace.N_trace       = 0;
        delete[] trace.times;
        trace.times = nullptr;
        // Read the data
        if ( format == 0 ) {
            std::vector<double> start( N ), stop( N );
            size_t N1 = fread( start.data(), sizeof( double ), N, fid );
            size_t N2 = fread( stop.data(), sizeof( double ), N, fid );
            ASSERT( N1 == N && N2 == N );
            char memory[10];
            size_t rtn2 = fread( memory, 1, 1, fid );
            ASSERT( rtn2 == 1 );
            StoreTimes times;
            times.reserve( N );
            for ( size_t i = 0; i < N; i++ ) {
                uint64_t t1 = 1e9 * start[i];
                uint64_t t2 = 1e9 * stop[i];
                times.add( t1, t2 );
            }
            trace.N_trace = times.size();
            trace.times   = times.take();
        } else if ( format == 1 ) {
            trace.N_trace = N;
            trace.times   = (uint16f*) malloc( 2 * N * sizeof( uint16f ) );
            size_t N2     = fread( trace.times, sizeof( uint16f ), 2 * N, fid );
            ASSERT( N2 == 2 * N );
            char memory[10];
            size_t rtn2 = fread( memory, 1, 1, fid );
            ASSERT( rtn2 == 1 );
        }
    }
    fclose( fid );
}
inline size_t getScale( std::string_view units )
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
        throw std::logic_error( "Unknown scale: " + std::string( units ) );
    }
    return scale;
}
void ProfilerApp::loadMemory( const std::string& filename, std::vector<MemoryResults>& data )
{
    // Open the file for reading
    FILE* fid = fopen( filename.c_str(), "rb" );
    if ( fid == nullptr )
        throw std::logic_error( "Error opening file: " + filename );
    while ( true ) {
        // Read the header
        auto line = readLine( fid );
        if ( line.empty() ) {
            fclose( fid );
            return;
        }
        if ( line[0] == '\1' )
            continue;
        data.resize( data.size() + 1 );
        MemoryResults& memory = data.back();
        // Get the header fields
        auto field = getField( line, "N=" );
        ASSERT( !field.empty() );
        auto N = convert<uint64_t>( field );
        field  = getField( line, "type1=" );
        ASSERT( !field.empty() );
        std::string type1( field );
        field = getField( line, "type2=" );
        ASSERT( !field.empty() );
        std::string type2( field );
        field = getField( line, "units=" );
        ASSERT( !field.empty() );
        size_t scale = getScale( field );
        field        = getField( line, "rank=" );
        ASSERT( !field.empty() );
        memory.rank = convert<uint64_t>( field );
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
            auto msg = "Unknown data type: " + type1 + ", " + type2;
            throw std::logic_error( msg );
        }
    }
    fclose( fid );
}


/***********************************************************************
 * Return pointer to the global timer info creating if necessary        *
 ***********************************************************************/
ProfilerApp::store_timer_data_info* ProfilerApp::getTimerData(
    uint64_t id, std::string_view message, std::string_view filename, int start )
{
    constexpr uint64_t mask = HASH_SIZE - 1;
    uint64_t key            = id & mask; // Get the hash index
    if ( d_timer_table[key] == nullptr ) {
        // The global timer does not exist, create it (requires blocking)
        // Acquire the lock (necessary for modifying the timer_table)
        d_lock.lock();
        // Check if the entry is still nullptr
        if ( d_timer_table[key] == nullptr ) {
            // Create a new entry
            auto* info_tmp =
                new store_timer_data_info( message.data(), filename.data(), id, start );
            d_bytes.fetch_add( sizeof( store_timer_data_info ) );
            d_timer_table[key] = info_tmp;
        }
        // Release the lock
        d_lock.unlock();
    }
    volatile store_timer_data_info* info = d_timer_table[key];
    while ( info->id != id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( info->next == nullptr ) {
            // Acquire the lock
            // Acquire the lock (necessary for modifying the timer_table)
            d_lock.lock();
            // Check if another thread created an entry while we were waiting for the lock
            if ( info->next == nullptr ) {
                // Create a new entry
                auto* info_tmp =
                    new store_timer_data_info( message.data(), filename.data(), id, start );
                info->next = info_tmp;
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
            for ( auto& timer : add ) {
                timer.unpack( &buffer[pos] );
                pos += timer.size();
            }
            addTimers( timers, std::move( add ) );
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
void ProfilerApp::addTimers( std::vector<TimerResults>& timers, std::vector<TimerResults>&& add )
{
    std::map<id_struct, size_t> id_map;
    for ( size_t i = 0; i < timers.size(); i++ )
        id_map.insert( std::pair<id_struct, size_t>( timers[i].id, i ) );
    for ( size_t i = 0; i < add.size(); i++ ) {
        TimerResults timer;
        std::swap( timer, add[i] );
        auto it = id_map.find( timer.id );
        if ( it == id_map.end() ) {
            size_t j = timers.size();
            timers.emplace_back( std::move( timer ) );
            id_map.insert( std::pair<id_struct, size_t>( timers[j].id, j ) );
        } else {
            size_t j = it->second;
            for ( size_t k = 0; k < timer.trace.size(); k++ ) {
                TraceResults trace;
                std::swap( trace, timer.trace[k] );
                timers[j].trace.emplace_back( std::move( trace ) );
            }
            timer.trace.clear();
        }
    }
    add.clear();
}


/***********************************************************************
 * Build a map for determining the active timers                        *
 ***********************************************************************/
template<class T>
static inline int binarySearch( const std::vector<T>& x_, T v )
{
    size_t n = x_.size();
    auto x   = x_.data();
    if ( n == 0 )
        return -1;
    if ( x[0] == v )
        return 0;
    if ( n == 1 )
        return -1;
    size_t lower = 0;
    size_t upper = n - 1;
    while ( ( upper - lower ) != 1 ) {
        size_t index = ( upper + lower ) / 2;
        if ( x[index] >= v )
            upper = index;
        else
            lower = index;
    }
    if ( x[upper] != v )
        return -1;
    return upper;
}
std::tuple<std::vector<uint64_t>, std::vector<std::vector<uint64_t>>> ProfilerApp::buildStackMap(
    const std::vector<TimerResults>& timers )
{
    // Get a map with the stack key and the final trace in the stack
    std::vector<uint64_t> stacks;
    std::vector<uint64_t> stack2;
    std::vector<uint64_t> prev;
    stacks.push_back( 0 );
    for ( const auto& timer : timers ) {
        for ( const auto& trace : timer.trace )
            stacks.push_back( static_cast<uint64_t>( trace.stack ) );
    }
    unique( stacks );
    stack2.reserve( stacks.size() );
    prev.reserve( stacks.size() );
    for ( const auto& timer : timers ) {
        for ( const auto& trace : timer.trace ) {
            if ( binarySearch( stacks, static_cast<uint64_t>( trace.stack2 ) ) != -1 ) {
                stack2.push_back( static_cast<uint64_t>( trace.stack2 ) );
                prev.push_back( static_cast<uint64_t>( trace.stack ) );
            }
        }
    }
    quicksort( stack2, prev );
    // Build the map
    std::vector<std::vector<uint64_t>> lists( stacks.size() );
    for ( size_t i = 0; i < stacks.size(); i++ ) {
        auto& list   = lists[i];
        uint64_t key = stacks[i];
        while ( key != 0 ) {
            int j = binarySearch( stack2, key );
            key   = prev[j];
            list.push_back( key );
        }
        quicksort( list );
    }
    return std::tie( stacks, lists );
}


/***********************************************************************
 * Subroutine to perform a quicksort                                    *
 ***********************************************************************/
template<class A>
static inline void quicksort( std::vector<A>& x )
{
    int64_t n = x.size();
    if ( n <= 1 )
        return;
    A* arr         = &x[0];
    int64_t jstack = 0;
    int64_t l      = 0;
    int64_t ir     = n - 1;
    int64_t istack[100];
    while ( 1 ) {
        if ( ir - l < 7 ) { // Insertion sort when subarray small enough.
            for ( int64_t j = l + 1; j <= ir; j++ ) {
                A a       = arr[j];
                bool test = true;
                int64_t i;
                for ( i = j - 1; i >= 0; i-- ) {
                    if ( arr[i] < a ) {
                        arr[i + 1] = a;
                        test       = false;
                        break;
                    }
                    arr[i + 1] = arr[i];
                }
                if ( test ) {
                    i          = l - 1;
                    arr[i + 1] = a;
                }
            }
            if ( jstack == 0 )
                return;
            ir = istack[jstack]; // Pop stack and begin a new round of partitioning.
            l  = istack[jstack - 1];
            jstack -= 2;
        } else {
            int64_t k =
                ( l + ir ) / 2; // Choose median of left, center and right elements as partitioning
                                // element a. Also rearrange so that a(l) ? a(l+1) ? a(ir).
            auto tmp_a = arr[k];
            arr[k]     = arr[l + 1];
            arr[l + 1] = tmp_a;
            if ( arr[l] > arr[ir] ) {
                tmp_a   = arr[l];
                arr[l]  = arr[ir];
                arr[ir] = tmp_a;
            }
            if ( arr[l + 1] > arr[ir] ) {
                tmp_a      = arr[l + 1];
                arr[l + 1] = arr[ir];
                arr[ir]    = tmp_a;
            }
            if ( arr[l] > arr[l + 1] ) {
                tmp_a      = arr[l];
                arr[l]     = arr[l + 1];
                arr[l + 1] = tmp_a;
            }
            // Scan up to find element > a
            int64_t j = ir;
            A a       = arr[l + 1]; // Partitioning element.
            int64_t i;
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
            }
            arr[l + 1] = arr[j]; // Insert partitioning element in both arrays.
            arr[j]     = a;
            jstack += 2;
            // Push pointers to larger subarray on stack, process smaller subarray immediately.
            if ( ir - i + 1 >= j - l ) {
                istack[jstack]     = ir;
                istack[jstack - 1] = i;
                ir                 = j - 1;
            } else {
                auto j2 = j - 1;
                while ( j2 - l > 1 && arr[j2] == arr[j] )
                    j2--;
                istack[jstack]     = j2;
                istack[jstack - 1] = l;
                l                  = i;
            }
        }
    }
}
template<class A, class B>
static inline void quicksort( std::vector<A>& x, std::vector<B>& y )
{
    int64_t n = x.size();
    if ( n <= 1 )
        return;
    A* arr         = &x[0];
    B* brr         = &y[0];
    int64_t jstack = 0;
    int64_t l      = 0;
    int64_t ir     = n - 1;
    int64_t istack[100];
    while ( 1 ) {
        if ( ir - l < 7 ) { // Insertion sort when subarray small enough.
            for ( int64_t j = l + 1; j <= ir; j++ ) {
                A a       = arr[j];
                B b       = brr[j];
                bool test = true;
                int64_t i;
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
            int64_t k =
                ( l + ir ) / 2; // Choose median of left, center and right elements as partitioning
                                // element a. Also rearrange so that a(l) ? a(l+1) ? a(ir).
            auto tmp_a = arr[k];
            arr[k]     = arr[l + 1];
            arr[l + 1] = tmp_a;
            auto tmp_b = brr[k];
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
            int64_t j = ir;
            A a       = arr[l + 1]; // Partitioning element.
            B b       = brr[l + 1];
            int64_t i;
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
                auto j2 = j - 1;
                while ( j2 - l > 1 && arr[j2] == arr[j] )
                    j2--;
                istack[jstack]     = j2;
                istack[jstack - 1] = l;
                l                  = i;
            }
        }
    }
}
template<class T>
void unique( std::vector<T>& x )
{
    if ( x.size() <= 1 )
        return;
    // First perform a quicksort
    quicksort( x );
    // Next remove duplicate entries
    size_t pos = 1;
    for ( size_t i = 1; i < x.size(); i++ ) {
        if ( x[i] != x[pos - 1] ) {
            x[pos] = x[i];
            pos++;
        }
    }
    if ( pos < x.size() )
        x.resize( pos );
}


/***********************************************************************
 * Test uint16f                                                        *
 ***********************************************************************/
static_assert( sizeof( uint16f ) == 2, "uint16f is not 2 bytes" );
static_assert( static_cast<uint64_t>( uint16f( 0u ) ) == 0u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 1u ) ) == 1u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 1023u ) ) == 1023u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 2047u ) ) == 2047u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 2048u ) ) == 2048u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 2049u ) ) == 2049u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 4095u ) ) == 4095u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 4096u ) ) == 4096u, "Error in uint16f" );
static_assert( static_cast<uint64_t>( uint16f( 4098u ) ) == 4098u, "Error in uint16f" );
static_assert(
    static_cast<uint64_t>( std::numeric_limits<uint16f>::min() ) == 0u, "Error in uint16f" );
static_assert(
    static_cast<uint64_t>( std::numeric_limits<uint16f>::max() ) > 4e12, "Error in uint16f" );


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
    auto id = ProfilerApp::getTimerId( name, file, 0 );
    global_profiler.start( id, name, file, line, level );
}
void global_profiler_stop( const char* name, const char* file, int level )
{
    auto id = ProfilerApp::getTimerId( name, file, 0 );
    global_profiler.stop( id, level );
}
void global_profiler_save( const char* name, int global )
{
    global_profiler.save( name, global != 0 );
}
}
