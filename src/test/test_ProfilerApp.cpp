#include "MemoryApp.h"
#include "ProfilerApp.h"
#include "test_Helpers.h"
#include <cmath>
#include <string>
#include <vector>

#ifdef USE_MPI
#include <mpi.h>
#endif


inline int getRank()
{
    int rank = 0;
#ifdef USE_MPI
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
#endif
    return rank;
}
inline int getSize()
{
    int size = 0;
#ifdef USE_MPI
    MPI_Comm_size( MPI_COMM_WORLD, &size );
#endif
    return size;
}


inline void printOverhead( const std::string &timer, int N_calls )
{
    if ( getRank() == 0 ) {
        size_t id    = global_profiler.get_timer_id( timer.c_str(), __FILE__ );
        auto results = global_profiler.getTimerResults( id );
        if ( results.trace.size() == 1 ) {
            float time = results.trace[0].tot;
            printf( "   %s: %i ns\n", timer.c_str(), int( time * 1e9 / N_calls ) );
        } else {
            printf( "   %s: N/A\n", timer.c_str() );
        }
    }
}


bool call_recursive_scope( int N, int i = 0 )
{
    bool pass = true;
    if ( i < 10 ) {
        char name[20];
        sprintf( name, "scoped-%i", i + 1 );
        pass = !global_profiler.active( name, __FILE__ );
        PROFILE_SCOPED( timer, "scoped" );
        pass = pass && global_profiler.active( name, __FILE__ );
        if ( N > 0 )
            pass = pass && call_recursive_scope( --N, ++i );
        sprintf( name, "scoped-%i", i + 2 );
        pass = pass && !global_profiler.active( name, __FILE__ );
    } else {
        PROFILE_SCOPED( timer, "scoped" );
        if ( N > 0 )
            pass = pass && call_recursive_scope( --N, ++i );
    }
    return pass;
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


int run_tests( bool enable_trace, std::string save_name )
{
    PROFILE_ENABLE();
    PROFILE_SYNCHRONIZE();
    if ( enable_trace ) {
        PROFILE_ENABLE_TRACE();
        PROFILE_ENABLE_MEMORY();
    }
    PROFILE_START( "MAIN" );

    const int N_it     = 100;
    const int N_timers = 500;
    int N_errors       = 0;
    const int rank     = getRank();
    const int N_proc   = getSize();

    // Check that "MAIN" is active and "NULL" is not
    bool test1 = global_profiler.active( "MAIN", __FILE__ );
    bool test2 = global_profiler.active( "NULL", __FILE__ );
    if ( !test1 || test2 ) {
        std::cout << "Correct timers are not active\n";
        N_errors++;
    }

    // Sleep for 1 second
    PROFILE_START( "sleep" );
    sleep( 1 );
    PROFILE_STOP( "sleep" );

    // Test the scoped timer
    bool pass = call_recursive_scope( 50 );
    if ( !pass ) {
        std::cout << "Scoped timer fails\n";
        N_errors++;
    }

    // Get a list of timer names
    std::vector<std::string> names( N_timers );
    std::vector<std::string> names2( N_timers );
    std::vector<size_t> ids( N_timers );
    for ( int i = 0; i < N_timers; i++ ) {
        char tmp[16];
        sprintf( tmp, "%04i", i );
        names[i]  = std::string( tmp );
        names2[i] = std::string( tmp ) + "_";
        ids[i]    = ProfilerApp::get_timer_id( names[i].c_str(), __FILE__ );
    }

    // Check that the start/stop command fail when they should
    try { // Check basic start/stop
        PROFILE_START( "dummy1" );
        PROFILE_STOP( "dummy1" );
    } catch ( ... ) {
        N_errors++;
    }
    try { // Check stop call before start
        PROFILE_STOP( "dummy2" );
        N_errors++;
    } catch ( ... ) {
    }
    try { // Check multiple calls to start
        PROFILE_START( "dummy3" );
        PROFILE_START2( "dummy3" );
        N_errors++;
    } catch ( ... ) {
        PROFILE_STOP( "dummy3" );
    }
    try { // Check multiple calls to start with different line numbers
        PROFILE_START( "dummy1" );
        N_errors++;
    } catch ( ... ) {
    }

    // Test a timer with many special characters
    const char special_char[] = "<>[]{}();:'\",./?\\-_+=`~!@#$%^&*";
    PROFILE_START( special_char );
    PROFILE_STOP( special_char );

    // Check the performance
    for ( int i = 0; i < N_it; i++ ) {
        // Test how long it takes to get the time
        PROFILE_START( "gettime" );
        ProfilerApp::time_point time1;
        for ( int j = 0; j < N_timers; j++ )
            time1 = ProfilerApp::now();
        PROFILE_STOP( "gettime" );
        // Test how long it takes to start/stop the timers
        PROFILE_START( "static" );
        for ( int j = 0; j < N_timers; j++ ) {
            PROFILE_START( "static_name" );
            PROFILE_STOP( "static_name" );
        }
        PROFILE_STOP( "static" );
        PROFILE_START( "dynamic" );
        for ( int j = 0; j < N_timers; j++ ) {
            PROFILE_START( names2[j] );
            PROFILE_STOP( names2[j] );
        }
        PROFILE_STOP( "dynamic" );
        PROFILE_START( "scoped-static" );
        for ( int j = 0; j < N_timers; j++ )
            PROFILE_SCOPED( timer, "static_name2" );
        PROFILE_STOP( "scoped-static" );
        PROFILE_START( "scoped-dynamic" );
        for ( int j = 0; j < N_timers; j++ )
            PROFILE_SCOPED( timer, names2[j] );
        PROFILE_STOP( "scoped-dynamic" );
        PROFILE_START( "scoped-manual" );
        for ( int j = 0; j < N_timers; j++ ) {
            ScopedTimer timer( "static_name3", __FILE__, __LINE__, 0, false, global_profiler );
            NULL_USE( timer );
        }
        PROFILE_STOP( "scoped-manual" );
        PROFILE_START( "level 0" );
        for ( int j = 0; j < N_timers; j++ ) {
            global_profiler.start( names[j], __FILE__, __LINE__, 0, ids[j] );
            global_profiler.stop( names[j], __FILE__, __LINE__, 0, ids[j] );
        }
        PROFILE_STOP( "level 0" );
        PROFILE_START( "level 1" );
        for ( int j = 0; j < N_timers; j++ ) {
            global_profiler.start( names[j], __FILE__, __LINE__, 1, ids[j] );
            global_profiler.stop( names[j], __FILE__, __LINE__, 1, ids[j] );
        }
        PROFILE_STOP( "level 1" );
        PROFILE_START( "level 2" );
        for ( int j = 0; j < N_timers; j++ ) {
            PROFILE_START( names[j], 2 );
            PROFILE_STOP( names[j], 2 );
        }
        PROFILE_STOP( "level 2" );
        // Test the two forms of active
        PROFILE_START( "active 1" );
        for ( int j = 0; j < N_timers; j++ )
            global_profiler.active( names[j], __FILE__ );
        PROFILE_STOP( "active 1" );
        PROFILE_START( "active 2" );
        for ( int j = 0; j < N_timers; j++ )
            global_profiler.active( ids[j] );
        PROFILE_STOP( "active 2" );
        // Test the memory around allocations
        PROFILE_START( "allocate1" );
        PROFILE_START( "allocate2" );
        auto *tmp = new double[5000000];
        NULL_USE( tmp );
        PROFILE_STOP( "allocate2" );
        delete[] tmp;
        PROFILE_START( "allocate3" );
        tmp = new double[100000];
        NULL_USE( tmp );
        PROFILE_STOP( "allocate3" );
        delete[] tmp;
        PROFILE_STOP( "allocate1" );
    }
    printf( "\nProfiler overhead:\n" );
    printOverhead( "static", N_it * N_timers );
    printOverhead( "dynamic", N_it * N_timers );
    printOverhead( "scoped-static", N_it * N_timers );
    printOverhead( "scoped-dynamic", N_it * N_timers );
    printOverhead( "scoped-manual", N_it * N_timers );
    printOverhead( "level 0", N_it * N_timers );
    printOverhead( "level 1", N_it * N_timers );
    printOverhead( "level 2", N_it * N_timers );
    printOverhead( "active 1", N_it * N_timers );
    printOverhead( "active 2", N_it * N_timers );
    printf( "\n" );

    // Profile the save
    PROFILE_START( "SAVE" );
    PROFILE_SAVE( save_name );
    PROFILE_STOP( "SAVE" );

    // Re-save the results
    PROFILE_SAVE( save_name );

    // Get the timers (sorting based on the timer ids)
    std::vector<TimerResults> data1 = global_profiler.getTimerResults();
    MemoryResults memory1           = global_profiler.getMemoryResults();
    if ( !check( memory1 ) ) {
        std::cout << "Memory results do not make sense\n";
        N_errors++;
    }
    size_t bytes1[2] = { 0, 0 };
    std::vector<id_struct> id1( data1.size() );
    for ( size_t i = 0; i < data1.size(); i++ ) {
        bytes1[0] += data1[i].size( false );
        bytes1[1] += data1[i].size( true );
        id1[i] = data1[i].id;
    }
    quicksort( id1.size(), &id1[0], &data1[0] );

    // Load the data from the file (sorting based on the timer ids)
    PROFILE_START( "LOAD" );
    TimerMemoryResults load_results  = ProfilerApp::load( save_name, rank );
    std::vector<TimerResults> &data2 = load_results.timers;
    MemoryResults memory2;
    if ( !load_results.memory.empty() )
        memory2 = load_results.memory[0];
    PROFILE_STOP( "LOAD" );
    size_t bytes2[2] = { 0, 0 };
    std::vector<id_struct> id2( data1.size() );
    for ( size_t i = 0; i < data2.size(); i++ ) {
        bytes2[0] += data2[i].size( false );
        bytes2[1] += data2[i].size( true );
        id2[i] = data2[i].id;
    }
    quicksort( id2.size(), &id2[0], &data2[0] );

    // Find and check MAIN
    const TraceResults *trace = nullptr;
    for ( auto &i : data1 ) {
        if ( i.message == "MAIN" )
            trace = &i.trace[0];
    }
    if ( trace != nullptr ) {
        if ( trace->tot == 0 ) {
            std::cout << "Error with trace results\n";
            N_errors++;
        }
    } else {
        std::cout << "MAIN was not found in trace results\n";
        N_errors++;
    }
    if ( enable_trace && trace != nullptr ) {
        int N               = trace->N_trace;
        const double *start = trace->start();
        const double *stop  = trace->stop();
        bool pass           = start != nullptr && stop != nullptr && N == 1;
        if ( *start < -0.01 || *start > 100.0 || *start > *stop )
            pass = false;
        if ( !pass ) {
            std::cout << "Error with trace results of MAIN\n";
            N_errors++;
        }
    }

    // Find and check sleep
    trace = nullptr;
    for ( auto &i : data1 ) {
        if ( i.message == "sleep" ) {
            trace = &i.trace[0];
            if ( fabs( i.trace[0].tot - 1.0 ) > 0.1 ) {
                std::cout << "Error profiling sleep: " << i.trace[0].tot << std::endl;
                N_errors++;
            }
        }
    }
    if ( trace == nullptr ) {
        std::cout << "sleep was not found in trace results\n";
        N_errors++;
    }

    // Compare the sets of timers
    if ( data1.size() != data2.size() || bytes1[0] == 0 || bytes1[0] != bytes2[0] ||
         bytes1[1] != bytes2[1] ) {
        std::cout << "Timers do not match " << data1.size() << " " << data2.size() << " "
                  << bytes1[0] << " " << bytes2[0] << " " << bytes1[1] << " " << bytes2[1] << " "
                  << std::endl;
        N_errors++;
    } else {
        bool error = false;
        for ( size_t i = 0; i < data1[i].trace.size(); i++ )
            error = error && data1[i].trace == data2[i].trace;
        if ( error ) {
            std::cout << "Timers do not match (1)" << std::endl;
            N_errors++;
        }
    }

    // Compare the memory results
    if ( memory1.time.size() != memory2.time.size() ) {
        std::cout << "Memory trace does not match\n";
        N_errors++;
    } else {
        bool error = false;
        for ( size_t i = 0; i < memory1.time.size(); i++ ) {
            if ( memory1.time[i] != memory2.time[i] || memory1.bytes[i] != memory2.bytes[i] )
                error = true;
        }
        if ( error ) {
            std::cout << "Memory trace does not match\n";
            N_errors++;
        }
    }

    // Test packing/unpacking TimerMemoryResults
    {
        TimerMemoryResults x, y;
        x.N_procs    = N_proc;
        x.timers     = data1;
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

    PROFILE_SAVE( save_name );
    PROFILE_SAVE( save_name, true );
    MemoryApp::print( std::cout );
    PROFILE_STOP( "MAIN" );
    return N_errors;
}


int main( int argc, char *argv[] )
{
// Initialize MPI
#ifdef USE_MPI
    MPI_Init( &argc, &argv );
#else
    NULL_USE( argc );
    NULL_USE( argv );
#endif

    int N_errors = 0;
    { // Limit scope

        // Run the tests
        std::vector<std::pair<bool, std::string>> tests;
        tests.emplace_back( false, "test_ProfilerApp" );
        // tests.push_back( std::pair<bool,std::string>(true,"test_ProfilerApp-trace"));
        for ( auto &test : tests ) {
            int m1 = getMemoryUsage();
            int m2 = global_profiler.getMemoryUsed();
            printf( "%i %i %i\n", m1 - m2, m1, m2 );
            N_errors += run_tests( test.first, test.second );
            m1 = getMemoryUsage();
            m2 = global_profiler.getMemoryUsed();
            printf( "%i %i %i\n", m1 - m2, m1, m2 );
            PROFILE_DISABLE();
        }
        int m1 = getMemoryUsage();
        int m2 = global_profiler.getMemoryUsed();
        printf( "%i %i %i\n\n", m1 - m2, m1, m2 );
    }

    // Print the memory stats
    MemoryApp::print( std::cout );
    std::cout << "Stack size 1: " << getStackSize1() << std::endl;
    std::cout << "Stack size 2: " << getStackSize2() << std::endl;
    std::cout << std::endl;

    // Finalize MPI and SAMRAI
    if ( N_errors == 0 )
        std::cout << "All tests passed" << std::endl;
    else
        std::cout << "Some tests failed" << std::endl;
#ifdef USE_MPI
    MPI_Barrier( MPI_COMM_WORLD );
    MPI_Finalize();
#endif
    return ( N_errors );
}
