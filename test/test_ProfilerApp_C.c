#include "ProfilerAppMacros_C.h"
#include <stdio.h>
#include <stdlib.h>


// Define NULL_USE
#define NULL_USE( var ) global_profiler_nullUse( (void *) &var )


// Define bool
typedef int bool;
#define true 1
#define false 0


int run_tests( bool enable_trace, const char *save_name )
{
    const int N_it     = 100;
    const int N_timers = 1000;
    char names[1000][16];
    int N_errors = 0;
    int i, j;
    double *tmp;
    printf( "Sizeof size_t* = %i\n", (int) sizeof( size_t * ) );

    PROFILE_ENABLE( 0 );
    PROFILE_SYNCHRONIZE();
    if ( enable_trace ) {
        PROFILE_ENABLE_TRACE();
        PROFILE_ENABLE_MEMORY();
    }
    PROFILE_START( "MAIN" );

    // Get a list of timer names
    for ( i = 0; i < N_timers; i++ )
        sprintf( names[i], "%04i", i );

    // Check basic start/stop
    PROFILE_START( "dummy1" );
    PROFILE_STOP( "dummy1" );

    // Check the performance
    for ( i = 0; i < N_it; i++ ) {
        // Test how long it takes to start/stop the timers
        PROFILE_START( "level 0" );
        for ( j = 0; j < N_timers; j++ ) {
            PROFILE_START( names[j], 0 );
            PROFILE_STOP( names[j], 0 );
        }
        PROFILE_STOP( "level 0" );
        PROFILE_START( "level 1" );
        for ( j = 0; j < N_timers; j++ ) {
            PROFILE_START( names[j], 1 );
            PROFILE_STOP( names[j], 1 );
        }
        PROFILE_STOP( "level 1" );
        // Test the memory around allocations
        PROFILE_START( "allocate1" );
        PROFILE_START( "allocate2" );
        tmp = (double *) malloc( 5000000 * sizeof( double ) );
        NULL_USE( tmp );
        PROFILE_STOP( "allocate2" );
        free( tmp );
        PROFILE_START( "allocate3" );
        tmp = (double *) malloc( 100000 * sizeof( double ) );
        NULL_USE( tmp );
        PROFILE_STOP( "allocate3" );
        free( tmp );
        PROFILE_STOP( "allocate1" );
    }

    // Profile the save
    PROFILE_START( "SAVE" );
    PROFILE_SAVE( save_name );
    PROFILE_STOP( "SAVE" );

    // Stop main
    PROFILE_STOP( "MAIN" );

    // Re-save the results and finish
    PROFILE_SAVE( save_name );
    PROFILE_SAVE( save_name, true );
    return N_errors;
}


int main( int argc, char *argv[] )
{
    NULL_USE( argc );
    NULL_USE( argv );

    // Run the tests
    int N_errors = 0;
    N_errors += run_tests( false, "test_ProfilerApp_C" );
    N_errors += run_tests( true, "test_ProfilerApp_C-trace" );

    // Finished
    if ( N_errors == 0 )
        printf( "All tests passed\n" );
    else
        printf( "Some tests failed\n" );
    return ( N_errors );
}
