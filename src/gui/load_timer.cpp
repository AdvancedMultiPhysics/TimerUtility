#include <QApplication>
#include <QAction>
#include <QObject>
#include <QtCore>
#include "timerwindow.h"
#include "ProfilerApp.h"
#include <thread>


int N_errors_global = 0;

// Run the unit tests for the app and then close the app
void runUnitTests( int N, char *files[], TimerWindow& app )
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int i=0; i<N; i++) {
        printf("Testing %s:\n",files[i]);
        int N_errors = app.runUnitTests(files[i]);
        N_errors_global += N_errors;
        if ( N_errors > 0 ) {
            printf("   Failed some tests\n");
        } else {
            printf("   Passed all tests\n");
        }
    }
    app.exit();
}


int main(int argc, char *argv[])
{
    // Load the application
    PROFILE_ENABLE();
    //Q_INIT_RESOURCE(application);
    QApplication app(argc, argv);
    app.setOrganizationName("");
    app.setApplicationName("load_timer");
    TimerWindow mainWin;
    #if defined(Q_OS_SYMBIAN)
        mainWin.showMaximized();
    #else
        mainWin.show();
    #endif
    // Run the unit tests (if provided extra arguments)
    std::thread thread;
    if ( argc > 1 )
        thread = std::thread(runUnitTests,argc-1,&argv[1],std::ref(mainWin));
    // Wait for application to finish
    int rtn = app.exec();
    // Join the threads
    if ( argc > 1 ) {
        thread.join();
        PROFILE_SAVE("load_timer");
    }
    if ( N_errors_global > 0 )
        rtn = N_errors_global;
    return rtn;
}
