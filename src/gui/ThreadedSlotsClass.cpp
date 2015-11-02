#include "ThreadedSlotsClass.h"

#include <thread>
#include <mutex>

#include <QApplication>


ThreadedSlotsClass::ThreadedSlotsClass( ):
    d_callAction(NULL), d_running(false)
{
    d_id = std::this_thread::get_id();
    d_callAction = new QAction(NULL);
    connect(d_callAction, SIGNAL(triggered()), this, SLOT(callFunMain()));
}
ThreadedSlotsClass:: ~ThreadedSlotsClass()
{
    delete d_callAction;
}
void ThreadedSlotsClass::callFun( std::function<void(void)> fun )
{
    // If this is the parent thread, process immediately
    if ( std::this_thread::get_id() == d_id ) {
        d_fun = fun;
        callFunMain();
        return;
    }
    // Serialize multiple threads
    d_mutex.lock();
    while ( d_running )
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    d_running = true;
    d_fun = fun;
    d_callAction->activate(QAction::Trigger);
    while ( d_running )
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    d_mutex.unlock();
}
void ThreadedSlotsClass::callFunMain( )
{
    qApp->processEvents();
    d_fun();
    qApp->processEvents();
    d_running = false;
}
static void nullFunction( ) { }
void ThreadedSlotsClass::update( )
{
    if ( std::this_thread::get_id() == d_id ) {
        qApp->processEvents();
    } else {
        ThreadedSlotsClass::callFun( nullFunction );
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}


