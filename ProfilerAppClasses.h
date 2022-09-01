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
     *    appends " (x)" to indicate the number of recursive calls of the given timer.
     * @param[in] msg       Name of the timer
     * @param[in] file      Name of the file containing the code (__FILE__)
     * @param[in] line      Line number containing the start command (__LINE__)
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     * @param[in] app       Profiler application to use (default will use the global profiler)
     * @param[in] trace     Store trace-level data for the timer:
     *                      -1: Default, use the global trace flag
     *                       0: Disable trace data for this timer
     *                       1: Enable trace data for this timer
     */
    ScopedTimer( const std::string& msg, const char* file, int line, int level = -1,
        ProfilerApp& app = global_profiler, int trace = -1 )
        : d_trace( trace ), d_app( app ), d_timer( nullptr )
    {
        if ( level == -1 )
            level = 0;
        if ( level >= 0 && level <= app.getLevel() ) {
            uint32_t v1 = ProfilerApp::hashString( ProfilerApp::stripPath( file ) );
            uint32_t v2 = ProfilerApp::hashString( msg.c_str() );
            initialize( v1, v2, msg.c_str(), file, line );
        }
    }

    // Advanced interface
    ScopedTimer( uint32_t v1, uint32_t v2, const char* msg, const char* file, int line, int level,
        ProfilerApp& app, int trace )
        : d_trace( trace ), d_app( app ), d_timer( nullptr )
    {
        if ( level == -1 )
            level = 0;
        if ( level >= 0 && level <= app.getLevel() )
            initialize( v1, v2, msg, file, line );
    }

    //! Destructor
    ~ScopedTimer()
    {
        if ( d_timer )
            d_app.stop( d_timer, std::chrono::steady_clock::now(), d_trace );
    }

    ScopedTimer( const ScopedTimer& ) = delete;

    ScopedTimer& operator=( const ScopedTimer& ) = delete;


private:
    int d_trace;
    ProfilerApp& d_app;
    ProfilerApp::store_timer* d_timer;


private:
    inline void initialize(
        uint32_t v1, uint32_t v2, const char* msg, const char* file, const int line )
    {
        // Get the appropriate timer
        uint64_t id = ( static_cast<uint64_t>( v2 ) << 32 ) + static_cast<uint64_t>( v1 ^ v2 );
        d_timer     = d_app.getBlock( id, true, msg, file, line, -1 );
        if ( d_timer->is_active ) {
            char msg2[512];
            size_t N = strlen( msg );
            memcpy( msg2, msg, N );
            msg2[N] = '(';
            for ( uint32_t r = 2; d_timer->is_active; r++ ) {
                // Append the index
                char* s    = &msg2[N + 1];
                uint32_t x = r, i = 0, y[10] = { 0 };
                while ( x > 0 ) {
                    y[i] = x % 10;
                    x /= 10;
                    i++;
                }
                for ( uint8_t j = 0; j < i; j++ )
                    s[j] = 48 + y[i - j - 1];
                s[i]     = ')';
                s[i + 1] = 0;
                // Create a new hash for each iteration
                s           = &msg2[N];
                uint32_t c  = 0;
                uint32_t v3 = v2;
                while ( ( c = *s++ ) )
                    v3 = ( ( v3 << 5 ) + v3 ) ^ c;
                id = ( static_cast<uint64_t>( v3 ) << 32 ) + static_cast<uint64_t>( v1 ^ v3 );
                // Check if the timer is active
                d_timer = d_app.getBlock( id, true, msg2, file, line, -1 );
            }
        }
        // Start the timer
        d_app.start( d_timer );
    }
};


#endif
