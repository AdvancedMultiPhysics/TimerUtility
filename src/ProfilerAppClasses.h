#ifndef included_ProfilerAppClasses
#define included_ProfilerAppClasses


#include "ProfilerApp.h"


/** \class ScopedTimer
 *
 * This class provides a scoped timer that automatically stops when it
 * leaves scope and is thread safe.
 * Example usage:
 *    void my_function(void *arg) {
 *       ScopedTimer timer = PROFILE_SCOPED(timer,"my function");
 *       ...
 *    }
 *    void my_function(void *arg) {
 *       PROFILE_SCOPED(timer,"my function");
 *       ...
 *    }
 */
class ScopedTimer
{
public:
    /**
     * @brief Create and start a scoped profiler
     * @details This is constructor to create and start a timer that starts
     *    at the given line, and is automatically deleted.
     *    The scoped timer is also recursive safe, in that it automatically
     *    appends "-x" to indicate the number of recursive calls of the given timer.
     *    Note: We can only have one scoped timer in a given scope
     *    Note: the scoped timer is generally lower performance that PROFILE_START and PROFILE_STOP.
     * @param[in] msg       Name of the timer
     * @param[in] file      Name of the file containing the code (__FILE__)
     * @param[in] line      Line number containing the start command (__LINE__)
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     * @param[in] recursive Should the time support recursive calls (default is true)
     * @param[in] app       Profiler application to use (default will use the global profiler)
     */
    ScopedTimer( const std::string& msg, const char* file, const int line, const int level = 0,
        bool recursive = true, ProfilerApp& app = global_profiler )
        : d_app( app ), d_thread( nullptr ), d_timer( nullptr ), d_count( nullptr )
    {
        if ( level < 0 || level >= 128 )
            throw std::logic_error( "timer level must be in the range 0-127 (ScopedTimer)" );
        if ( level <= app.get_level() ) {
            // Get the thread data
            d_thread = app.get_thread_data();
            // Get the appropriate timer
            if ( recursive ) {
                char message[256];
#if defined( TIMER_ENABLE_THREAD_LOCAL ) && !defined( DISABLE_TIMER_THREAD_LOCAL_MAP )
                d_count = app.d_level_map.get( msg );
                ++( *d_count );
                sprintf( message, "%s-%i", msg.c_str(), *d_count );
                size_t id = ProfilerApp::get_timer_id( message, file );
#else
                int recursive_level = 0;
                size_t id           = 0;
                while ( id == 0 ) {
                    recursive_level++;
                    sprintf( message, "%s-%i", msg.c_str(), recursive_level );
                    size_t id2 = ProfilerApp::get_timer_id( message, file );
                    bool test  = d_app.active( id2 );
                    id         = test ? 0 : id2;
                }

#endif
                d_timer = app.get_block( d_thread, id, true, message, file, line, -1 );
            } else {
                size_t id = ProfilerApp::get_timer_id( msg.c_str(), file );
                d_timer   = app.get_block( d_thread, id, true, msg.c_str(), file, line, -1 );
            }
            // Start the timer
            d_app.start( d_thread, d_timer );
        }
    }
    ~ScopedTimer()
    {
        if ( d_timer ) {
            d_app.stop( d_thread, d_timer );
            if ( d_count )
                --( *d_count );
        }
    }

protected:
    ScopedTimer( const ScopedTimer& );            // Private copy constructor
    ScopedTimer& operator=( const ScopedTimer& ); // Private assignment operator
private:
    ProfilerApp& d_app;
    ProfilerApp::thread_info* d_thread;
    ProfilerApp::store_timer* d_timer;
    int* d_count;
};


#endif
