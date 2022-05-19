#include "MemoryApp.h"
#include "ProfilerApp.h"
#include "timerwindow.h"
#include <QAction>
#include <QApplication>
#include <QObject>
#include <QtCore>
#include <thread>


int N_errors_global = 0;

// Run the unit tests for the app and then close the app
void runUnitTests( int N, char *files[], TimerWindow &app )
{
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
    for ( int i = 0; i < N; i++ ) {
        printf( "Testing %s:\n", files[i] );
        bool pass = app.runUnitTests( files[i] );
        if ( pass ) {
            printf( "   Passed all tests\n" );
        } else {
            printf( "   Failed some tests\n" );
            N_errors_global++;
        }
    }
    app.exit();
}


int main( int argc, char *argv[] )
{
    bool run_unit_tests = argc > 1;
    int rtn             = 0;
    PROFILE_ENABLE( 2 );
    PROFILE_ENABLE_TRACE();
    PROFILE_ENABLE_MEMORY();
    { // Limit scope for MemoryApp
        // Load the application
        // Q_INIT_RESOURCE(application);
        QApplication app( argc, argv );
        app.setOrganizationName( "" );
        app.setApplicationName( "load_timer" );
        TimerWindow mainWin;
#if defined( Q_OS_SYMBIAN )
        mainWin.showMaximized();
#else
        mainWin.show();
#endif
        // Run the unit tests (if provided extra arguments)
        std::thread thread;
        if ( run_unit_tests )
            thread = std::thread( runUnitTests, argc - 1, &argv[1], std::ref( mainWin ) );
        // Wait for application to finish
        rtn = app.exec();
        // Join the threads
        if ( run_unit_tests )
            thread.join();
        if ( N_errors_global > 0 )
            rtn = N_errors_global;
    } // MemoryApp scope limit
    if ( run_unit_tests ) {
        PROFILE_SAVE( "load_timer" );
        MemoryApp::print( std::cout );
    }
    return rtn;
}
