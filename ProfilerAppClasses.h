#ifndef included_ProfilerAppClasses
#define included_ProfilerAppClasses


#include "ProfilerApp.h"


/** \class ProfilerAppTimer
 *
 * This class provides a timer that automatically stops when it
 * leaves scope and is thread safe.
 * Example usage:
 *    void my_function(void *arg) {
 *       PROFILE(timer,"my function");
 *       ...
 *    }
 */
template<std::size_t id, bool fixedMessage = true>
class ProfilerAppTimer final
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
     * @param[in] trace     Store trace-level data for the timer:
     *                      -1: Default, use the global trace flag
     *                       0: Disable trace data for this timer
     *                       1: Enable trace data for this timer
     */
    explicit ProfilerAppTimer( const char* msg, const char* file, int line, int level, int trace )
        : d_level( level ), d_traceFlag( trace ), d_trace( nullptr )
    {
        if ( level >= 0 && level <= global_profiler.getLevel() ) {
            if constexpr ( fixedMessage ) {
                auto timer = global_profiler.getBlock( id, msg, file, line );
                d_trace    = global_profiler.start( timer );
            } else {
                auto id2   = id ^ static_cast<uint64_t>( ProfilerApp::hashString( msg ) );
                auto timer = global_profiler.getBlock( id2, msg, file, line );
                d_trace    = global_profiler.start( timer );
            }
        }
    }
    explicit ProfilerAppTimer(
        const std::string& msg, const char* file, int line, int level, int trace )
        : d_level( level ), d_traceFlag( trace ), d_trace( nullptr )
    {
        static_assert( !fixedMessage, "string interface intended for dynamic names only" );
        if ( level >= 0 && level <= global_profiler.getLevel() ) {
            auto id2   = id ^ static_cast<uint64_t>( ProfilerApp::hashString( msg ) );
            auto timer = global_profiler.getBlock( id2, msg.data(), file, line );
            d_trace    = global_profiler.start( timer );
        }
    }

    //! Destructor
    ~ProfilerAppTimer()
    {
        if ( d_trace && global_profiler.getLevel() >= 0 )
            global_profiler.stop( d_trace, std::chrono::steady_clock::now(), d_traceFlag );
    }

    ProfilerAppTimer( const ProfilerAppTimer& )            = delete;
    ProfilerAppTimer& operator=( const ProfilerAppTimer& ) = delete;


private:
    int d_level;
    int d_traceFlag;
    ProfilerApp::store_trace* d_trace;
};


#endif
