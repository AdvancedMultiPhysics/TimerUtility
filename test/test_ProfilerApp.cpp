#include "MemoryApp.h"
#include "ProfilerApp.h"
#include "test_Helpers.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef USE_MPI
PROFILE_DISABLE_WARNINGS
#include <mpi.h>
PROFILE_ENABLE_WARNINGS
inline int getRank()
{
    int rank = 0;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    return rank;
}
inline int getSize()
{
    int size = 0;
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    return size;
}
inline int sumReduce( int x )
{
    int y = 0;
    MPI_Allreduce( &x, &y, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD );
    return y;
}
inline void barrier() { MPI_Barrier( MPI_COMM_WORLD ); }
#else
inline int getRank() { return 0; }
inline int getSize() { return 1; }
inline int sumReduce( int x ) { return x; }
inline void barrier() {}
inline void MPI_Init( int *, char **[] ) {}
inline void MPI_Finalize() {}
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


inline void printOverhead( const std::string &timer, int N_calls )
{
    auto results = global_profiler.getTimerResults( timer, __FILE__ );
    if ( results.trace.size() == 1 ) {
        uint64_t time = results.trace[0].tot;
        printf( "   %s: %i ns\n", timer.c_str(), int( time / N_calls ) );
    } else {
        printf( "   %s: N/A\n", timer.c_str() );
    }
}


std::string random_string( int N )
{
    std::string str;
    static char chars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::mt19937 gen{ std::random_device{}() };
    static std::uniform_int_distribution<int> dist( 0, sizeof( chars ) - 2 );
    str.reserve( N );
    for ( int i = 0; i < N; i++ )
        str += chars[dist( gen )];
    return str;
}


void recursion( int N )
{
    PROFILE( "recursion" );
    if ( N > 1 )
        recursion( N - 1 );
}
void call_recursion( int );
void recursion1( int N )
{
    PROFILE( "recursion1" );
    call_recursion( N - 1 );
}
void recursion2( int N )
{
    PROFILE( "recursion2" );
    call_recursion( N - 1 );
}
void recursion3( int N )
{
    PROFILE( "recursion3" );
    call_recursion( N - 1 );
}
void recursion4( int N )
{
    PROFILE( "recursion4" );
    call_recursion( N - 1 );
}
void recursion5( int N )
{
    PROFILE( "recursion5" );
    call_recursion( N - 1 );
}
void call_recursion( int N )
{
    if ( N == 0 )
        return;
    int i = rand() % 5;
    if ( i == 0 )
        recursion1( N );
    else if ( i == 1 )
        recursion2( N );
    else if ( i == 2 )
        recursion3( N );
    else if ( i == 3 )
        recursion4( N );
    else
        recursion5( N );
}
void test_recursion( int N )
{
    PROFILE( "test_recursion" );
    recursion( N );
    call_recursion( N );
}


size_t getStackSize1()
{
    MemoryApp::MemoryStats stats = MemoryApp::getMemoryStats();
    return stats.stack_used;
}
size_t getStackSize2()
{
    int tmp[50000];
    memset( tmp, 0, 1000 * sizeof( double ) );
    MemoryApp::MemoryStats stats = MemoryApp::getMemoryStats();
    if ( tmp[999] != 0 )
        std::cout << std::endl;
    return stats.stack_used;
}


// Perform a sanity check of the memory results
bool check( const MemoryResults &memory )
{
    bool pass = true;
    for ( size_t i = 1; i < memory.time.size(); i++ )
        pass = pass && memory.time[i] >= memory.time[i - 1];
    for ( size_t i = 0; i < memory.time.size(); i++ )
        pass = pass && static_cast<double>( memory.bytes[i] ) < 10e9;
    return pass;
}


// Test performance of clock
template<typename TYPE>
static inline int test_clock()
{
    auto start  = TYPE::now();
    const int N = 100000;
    for ( int j = 0; j < N; j++ ) {
        [[maybe_unused]] auto t1 = TYPE::now();
        [[maybe_unused]] auto t2 = TYPE::now();
    }
    auto stop = TYPE::now();
    auto ns   = diff_ns( stop, start );
    return ns / N;
}
template<typename TYPE>
static inline int get_clock_resolution()
{
    int resolution = std::numeric_limits<int>::max();
    for ( int i = 0; i < 10; i++ ) {
        int ns  = 0;
        auto t0 = TYPE::now();
        while ( ns <= 0 )
            ns = diff_ns( TYPE::now(), t0 );
        resolution = std::min( resolution, ns );
    }
    return resolution;
}
static inline int test_diff_ns()
{
    int N       = 1000;
    auto start  = std::chrono::steady_clock::now();
    uint64_t ns = 0;
    for ( int i = 0; i < N; i++ )
        ns += diff_ns( std::chrono::steady_clock::now(), start );
    auto stop = std::chrono::steady_clock::now();
    ASSERT( ns > 0 );
    int time_duration = diff_ns( stop, start ) - N * test_clock<std::chrono::steady_clock>();
    return std::max( time_duration, 0 ) / N;
}
static inline int test_getMemoryUsage()
{
    auto t1 = std::chrono::steady_clock::now();
    int N   = 100000;
    auto x  = new size_t[N];
    for ( int j = 0; j < N; j++ )
        x[j] = MemoryApp::getMemoryUsage();
    auto t2      = std::chrono::steady_clock::now();
    auto ns      = diff_ns( t2, t1 );
    size_t bytes = 0;
    for ( int j = 0; j < N; j++ )
        bytes = std::max( bytes, x[j] );
    if ( bytes == 0 )
        printf( "getMemoryUsage returns 0\n" );
    delete[] x;
    return ns / N;
}
static inline int test_getTotalMemoryUsage()
{
    auto t1 = std::chrono::steady_clock::now();
    int N   = 1000000;
    auto x  = new size_t[N];
    for ( int j = 0; j < N; j++ )
        x[j] = MemoryApp::getTotalMemoryUsage();
    auto t2      = std::chrono::steady_clock::now();
    auto ns      = diff_ns( t2, t1 );
    size_t bytes = 0;
    for ( int j = 0; j < N; j++ )
        bytes = std::max( bytes, x[j] );
    if ( bytes == 0 )
        printf( "getTotalMemoryUsage returns 0\n" );
    delete[] x;
    return ns / N;
}


// Compare two sets of timers
bool compareTimers( const std::vector<TimerResults> &x, const std::vector<TimerResults> &y )
{
    int i = find( x, "MAIN" );
    int j = find( y, "MAIN" );
    if ( i == -1 || j == -1 ) {
        printf( "Unable to find 'MAIN'\n" );
        return false;
    } else if ( x[i].id != y[j].id ) {
        printf( "Timer ids for 'MAIN' do not match\n" );
        return false;
    }
    std::set<id_struct> ids;
    for ( auto &t : x )
        ids.insert( t.id );
    for ( auto &t : y )
        ids.insert( t.id );
    bool pass = true;
    for ( auto id : ids ) {
        i = find( x, id );
        j = find( y, id );
        if ( i == -1 ) {
            printf( "Timer '%s' is missing from x but is found in y\n", y[j].message );
            pass = false;
        } else if ( j == -1 ) {
            printf( "Timer '%s' is missing from y but is found in x\n", x[i].message );
            pass = false;
        } else if ( x[i] == y[j] ) {
            continue;
        } else if ( std::string( x[i].message ) == "SAVE" ) {
            // SAVE is special
        } else if ( x[i].size( true ) != y[j].size( true ) ||
                    x[i].size( false ) != y[j].size( false ) ) {
            printf( "Timers do not match: '%s' (%i,%i) (%i,%i)\n", x[i].message,
                (int) x[i].size( false ), (int) x[i].size( true ), (int) y[j].size( false ),
                (int) y[j].size( true ) );
            pass = false;
        } else if ( std::string( x[i].message ) == "MAIN" ) {
            // MAIN is special, times change
        } else if ( pass ) {
            printf( "Timers do not match: '%s'\n", x[i].message );
            pass = false;
        }
    }
    return pass;
}


// Run all tests
int run_tests( bool enable_trace, bool enable_memory, std::string save_name )
{
    PROFILE_ENABLE();
    PROFILE_SYNCHRONIZE();
    if ( enable_trace )
        PROFILE_ENABLE_TRACE();
    if ( enable_memory )
        PROFILE_ENABLE_MEMORY();
    PROFILE( "MAIN" );

    const int N_it     = 500;
    const int N_timers = 500;
    int N_errors       = 0;
    const int rank     = getRank();
    const int N_proc   = getSize();
    printf( "Rank %i of %i\n", rank, N_proc );

    // Sleep for 1 second
    {
        PROFILE( "sleep" );
        std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
    }

    // Test recursion
    test_recursion( 1000 );

    // Get a list of timer names
    std::vector<std::string> names( N_timers );
    std::vector<std::string> names2( N_timers );
    std::vector<std::string> names3( N_timers );
    std::vector<size_t> ids( N_timers );
    for ( int i = 0; i < N_timers; i++ ) {
        char tmp[16];
        sprintf( tmp, "%04i", i );
        names[i]  = std::string( tmp );
        names2[i] = std::string( tmp ) + "_";
        names3[i] = std::string( tmp ) + "_s";
        ids[i]    = ProfilerApp::getTimerId( names[i].c_str(), __FILE__, 0 );
    }

    // Test a timer with many special characters
    constexpr char special_char[] = "<>[]{}();:'\",./?\\-_+=`~!@#$%^&*";
    {
        PROFILE( special_char );
    }

    // Check the performance
    for ( int i = 0; i < N_it; i++ ) {
        // Test how long it takes to start/stop the timers
        barrier();
        {
            PROFILE( "single" );
            for ( int j = 0; j < N_timers; j++ )
                PROFILE( "static_name" );
        }
        {
            PROFILE( "static" );
            for ( int j = 0; j < N_timers; j++ ) {
                global_profiler.start( ids[j], names[j].data(), __FILE__, __LINE__, 0 );
                global_profiler.stop( ids[j] );
            }
        }
        {
            PROFILE( "dynamic" );
            for ( int j = 0; j < N_timers; j++ )
                PROFILE2( names2[j] );
        }
        {
            PROFILE( "level 0" );
            for ( int j = 0; j < N_timers; j++ ) {
                global_profiler.start( ids[j], names[j].data(), __FILE__, -1, 0 );
                global_profiler.stop( ids[j], 0 );
            }
        }
        {
            PROFILE( "level 1" );
            for ( int j = 0; j < N_timers; j++ ) {
                global_profiler.start( ids[j], names[j].data(), __FILE__, -1, 1 );
                global_profiler.stop( ids[j], 1 );
            }
        }
        {
            PROFILE( "level 1 (single)" );
            for ( int j = 0; j < N_timers; j++ )
                PROFILE( "static_name", 1 );
        }
        {
            PROFILE( "level 1 (static)" );
            for ( int j = 0; j < N_timers; j++ ) {
                global_profiler.start( ids[j], names[j].data(), __FILE__, __LINE__, 1 );
                global_profiler.stop( ids[j], 1 );
            }
        }
        {
            PROFILE( "level 1 (dynamic)" );
            for ( int j = 0; j < N_timers; j++ )
                PROFILE2( names2[j], 1 );
        }
        // Test the memory around allocations
        {
            PROFILE( "allocate1" );
            [[maybe_unused]] double *tmp = nullptr;
            {
                PROFILE( "allocate2" );
                tmp = new double[5000000];
            }
            delete[] tmp;
            {
                PROFILE( "allocate3" );
                tmp = new double[100000];
            }
            delete[] tmp;
        }
    }
    if ( getRank() == 0 ) {
        printf( "\nProfiler overhead (%i,%i):\n", enable_trace ? 1 : 0, enable_memory ? 1 : 0 );
        printOverhead( "single", N_it * N_timers );
        printOverhead( "static", N_it * N_timers );
        printOverhead( "dynamic", N_it * N_timers );
        printOverhead( "level 0", N_it * N_timers );
        printOverhead( "level 1", N_it * N_timers );
        printOverhead( "level 1 (single)", N_it * N_timers );
        printOverhead( "level 1 (static)", N_it * N_timers );
        printOverhead( "level 1 (dynamic)", N_it * N_timers );
        printf( "\n" );
    }

    // Create a timer with long names to ensure we truncate correctly
    std::string long_msg      = "Long message - " + random_string( 128 );
    std::string long_file     = "Long filename - " + random_string( 128 );
    std::string long_path     = "Long pathname - " + random_string( 128 );
    std::string long_filename = long_path + "/" + long_file;
    auto long_id              = ProfilerApp::getTimerId( long_msg.data(), long_filename.data(), 0 );
    global_profiler.start( long_id, long_msg.data(), long_filename.data(), 0 );
    global_profiler.stop( long_id );

    // Pause memory profiling before the save
    if ( global_profiler.getStoreMemory() != ProfilerApp::MemoryLevel::None )
        global_profiler.setStoreMemory( ProfilerApp::MemoryLevel::Pause );

    // Profile the save
    {
        PROFILE( "SAVE" );
        PROFILE_SAVE( save_name );
    }

    // Re-save the results
    PROFILE_SAVE( save_name );

    // Get the timers (sorting based on the timer ids)
    auto memory1 = global_profiler.getMemoryResults();
    PROFILE_MEMORY();
    auto data1 = global_profiler.getTimerResults();
    PROFILE_MEMORY();
    if ( !check( memory1 ) ) {
        std::cout << "Memory results do not make sense\n";
        N_errors++;
    }
    sort( data1 );

    // Load the data from the file (sorting based on the timer ids)
    auto id = ProfilerApp::getTimerId( "LOAD", __FILE__, 0 );
    global_profiler.start( id, "LOAD", __FILE__, __LINE__ );
    auto load_results = ProfilerApp::load( save_name, rank );
    auto &data2       = load_results.timers;
    MemoryResults memory2;
    if ( !load_results.memory.empty() )
        memory2 = load_results.memory[0];
    global_profiler.stop( id );
    sort( data2 );

    // Find and check sleep
    TraceResults *trace = nullptr;
    for ( auto &timer : data1 ) {
        if ( strcmp( timer.message, "sleep" ) == 0 ) {
            trace      = &timer.trace[0];
            double tot = 1e-9 * trace->tot;
            if ( fabs( tot - 1.0 ) > 0.1 ) {
                std::cout << "Error profiling sleep: " << tot << std::endl;
                N_errors++;
            }
        }
    }
    if ( trace == nullptr ) {
        std::cout << "sleep was not found in trace results\n";
        N_errors++;
    }

    // Compare the sets of timers
    bool test = compareTimers( data1, data2 );
    if ( !test )
        N_errors++;

        // Compare the memory results
#ifndef TIMER_DISABLE_NEW_OVERLOAD
    if ( enable_memory ) {
        bool error = memory2.time.empty() || memory1.time.size() < memory2.time.size();
        for ( size_t i = 0; i < memory2.time.size(); i++ ) {
            if ( memory1.time[i] != memory2.time[i] || memory1.bytes[i] != memory2.bytes[i] )
                error = true;
        }
        if ( error ) {
            std::cout << "Memory trace does not match\n";
            N_errors++;
        }
    }
#endif

    // Test packing/unpacking TimerMemoryResults
    {
        TimerMemoryResults x, y;
        x.N_procs    = N_proc;
        x.timers     = std::move( data1 );
        x.memory     = std::vector<MemoryResults>( 1, memory1 );
        size_t bytes = x.size();
        auto *data   = new char[bytes];
        x.pack( data );
        y.unpack( data );
        if ( x != x ) {
            std::cout << "TimerMemoryResults do not match self\n";
            N_errors++;
        }
        if ( x != y ) {
            std::cout << "TimerMemoryResults do not match after pack/unpack\n";
            N_errors++;
        }
        delete[] data;
    }

    PROFILE_SAVE( save_name, true );
    PROFILE_SAVE( save_name, false );
    return N_errors;
}


int main( int argc, char *argv[] )
{
    // Initialize MPI
    MPI_Init( &argc, &argv );
    int rank = getRank();

    // Print basic info
    int N_errors = 0;
    if ( rank == 0 ) {
        // Print clock resolution
        printf( "\nClock resolution:\n" );
        printf( "  system_clock: %i\n", get_clock_resolution<std::chrono::system_clock>() );
        printf( "  steady_clock: %i\n", get_clock_resolution<std::chrono::steady_clock>() );
        printf( "  high_resolution_clock: %i\n",
            get_clock_resolution<std::chrono::high_resolution_clock>() );
        printf( "\n" );
        // Test how long it takes to get the time
        printf( "Clock performance:\n" );
        printf( "  system_clock: %i\n", test_clock<std::chrono::system_clock>() );
        printf( "  steady_clock: %i\n", test_clock<std::chrono::steady_clock>() );
        printf( "  high_resolution_clock: %i\n", test_clock<std::chrono::high_resolution_clock>() );
        printf( "  diff_ns: %i\n", test_diff_ns() );
        printf( "\n" );
        // Test how long it takes to get memory usage
        printf( "getMemoryUsage: %i\n", test_getMemoryUsage() );
        printf( "getTotalMemoryUsage: %i\n", test_getTotalMemoryUsage() );
        printf( "\n" );
        // Print the size of some structures
        printf( "sizeof(TimerResults): %i\n", (int) sizeof( TimerResults ) );
        printf( "sizeof(TraceResults): %i\n", (int) sizeof( TraceResults ) );
        printf( "\n" );
    }

    // Run the profiler tests
    {
        std::vector<std::tuple<bool, bool, std::string>> tests;
        tests.emplace_back<bool, bool, std::string>( false, false, "test_ProfilerApp" );
        tests.emplace_back<bool, bool, std::string>( true, false, "test_ProfilerApp-trace" );
        tests.emplace_back<bool, bool, std::string>( true, true, "test_ProfilerApp-memory" );
        for ( auto &test : tests ) {
            N_errors += run_tests( std::get<0>( test ), std::get<1>( test ), std::get<2>( test ) );
            PROFILE_DISABLE();
        }
    }

    // Print the memory stats
    if ( rank == 0 ) {
        std::cout << std::endl;
        MemoryApp::print( std::cout );
    }

    // Finalize MPI
    if ( rank == 0 ) {
        if ( N_errors == 0 )
            std::cout << "All tests passed" << std::endl;
        else
            std::cout << "Some tests failed" << std::endl;
    }
    N_errors = sumReduce( N_errors );
    MPI_Finalize();
    return N_errors;
}
