#ifndef ThreadedSlotsClass_H
#define ThreadedSlotsClass_H

#include <functional>
#include <mutex>
#include <thread>

#include <QAction>
#include <QMainWindow>


//! This is a helper class that allows a thread to call a function from the main thread
class ThreadedSlotsClass : public QMainWindow
{
    Q_OBJECT

protected:
    //! Empty constructor
    ThreadedSlotsClass();
    //! Destructor
    virtual ~ThreadedSlotsClass();
    //! Call a function
    void callFun( std::function<void( void )>&& fun );
    //! Process queue events
    void update();

private:
    std::function<void( void )> d_fun;
    QAction* d_callAction;
    volatile bool d_running;
    std::mutex d_mutex;
    std::thread::id d_id;

private slots:
    void callFunMain();
};

#endif
