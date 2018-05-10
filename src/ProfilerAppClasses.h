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
        if ( level >= 0 && level <= app.getLevel() ) {
            auto id = ProfilerApp::getTimerId( msg.c_str(), file );
            initialize( id, msg.c_str(), file, line, recursive );
        }
    }


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
    ScopedTimer( uint64_t id, const char* msg, const char* file, const int line, const int level,
        bool recursive, ProfilerApp& app )
        : d_app( app ), d_thread( nullptr ), d_timer( nullptr ), d_count( nullptr )
    {
        if ( level >= 0 && level <= app.getLevel() )
            initialize( id, msg, file, line, recursive );
    }
    ~ScopedTimer()
    {
        if ( d_timer ) {
            d_app.stop( d_thread, d_timer );
            if ( d_count )
                --( *d_count );
        }
    }

    ScopedTimer( const ScopedTimer& ) = delete;

    ScopedTimer& operator=( const ScopedTimer& ) = delete;


private:
    ProfilerApp& d_app;
    ProfilerApp::thread_info* d_thread;
    ProfilerApp::store_timer* d_timer;
    int* d_count;


private:
    inline void initialize( uint64_t id, const char* msg, const char* file, const int line, bool recursive )
    {
        // Get the thread data
        d_thread = d_app.getThreadData();
        // Get the appropriate timer
        d_timer = d_app.getBlock( d_thread, id, true, msg, file, line, -1 );
        if ( !recursive && d_timer->is_active )
            d_app.activeErrStart( d_thread, d_timer );
        if ( d_timer->is_active ) {
            char msg2[512];
            size_t N = strlen(msg);
            memcpy( msg2, msg, N );
            char *s = &msg2[N];
            *s = '(';
            for ( uint32_t r = 2; d_timer->is_active; r++ ) {
                append_name( r, &s[1] );
                // Create a new hash for each iteration
                auto id2 = getID( id, s );
                // Check if the timer is active
                d_timer = d_app.getBlock( d_thread, id2, true, msg2, file, line, -1 );
            }
        }
        // Start the timer
        d_app.start( d_thread, d_timer );
    }
    static inline uint64_t getID( uint64_t id, char *s ) {
        uint32_t v2 = id >> 32;
        uint32_t v1 = static_cast<uint32_t>( id & 0xFFFFFFFF ) ^ v2;
        uint32_t c  = 0;
        while ( ( c = *s++ ) )
            v2 = ( ( v2 << 5 ) + v2 ) ^ c;
        return ( static_cast<uint64_t>( v2 ) << 32 ) + static_cast<uint64_t>( v1^v2 );
    }
    static inline void append_name( uint32_t x, char *s )
    {
        if ( x == 0 ) {
            s[0] = 48;
            return;
        }
        uint8_t i=0, y[10] = { 0 };
        while ( x > 0 ) {
            y[i] = x % 10;
            x /= 10;
            i++;
        }
        for ( int j=0; j<i; j++)
            s[j] = 48 + y[i-j-1];
        s[i] = ')';
        s[i+1] = 0;
    }
};


#endif
