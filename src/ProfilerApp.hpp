#ifndef included_ProfilerApp_hpp
#define included_ProfilerApp_hpp

#include "ProfilerApp.h"

#include <sstream>
#include <stdexcept>


// Use the hashing function 2^32*0.5*(sqrt(5)-1)
#define GET_TIMER_HASH( id ) ( ( ( id * 0x9E3779B97F4A7C15 ) >> 48 ) % TIMER_HASH_SIZE )


/***********************************************************************
 * Function to get the timmer for a particular block of code            *
 * Note: This function performs some blocking as necessary.             *
 ***********************************************************************/
inline ProfilerApp::store_timer* ProfilerApp::getBlock( thread_info* thread_data, uint64_t id,
    bool create, const char* message, const char* filename, const int start, const int stop )
{
    using std::to_string;
    // Search for the thread-specific timer and create it if necessary (does not need blocking)
    size_t key = GET_TIMER_HASH( id ); // Get the hash index
    if ( thread_data->head[key] == nullptr ) {
        if ( !create )
            return nullptr;
        // The timer does not exist, create it
        auto new_timer = new store_timer;
        auto size      = sizeof( store_timer );
        TimerUtility::atomic::atomic_add( &d_bytes, size );
        new_timer->id          = id;
        new_timer->is_active   = false;
        new_timer->trace_index = thread_data->N_timers;
        thread_data->N_timers++;
        thread_data->head[key] = new_timer;
    }
    store_timer* timer = thread_data->head[key];
    while ( timer->id != id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( timer->next == nullptr ) {
            if ( !create )
                return nullptr;
            auto new_timer = new store_timer;
            auto size      = sizeof( store_timer );
            TimerUtility::atomic::atomic_add( &d_bytes, size );
            new_timer->id          = id;
            new_timer->is_active   = false;
            new_timer->trace_index = thread_data->N_timers;
            thread_data->N_timers++;
            timer->next = new_timer;
        }
        // Advance to the next entry
        timer = timer->next;
    }
    // Get the global timer info and create if necessary
    auto global_info = timer->timer_data;
    if ( global_info == nullptr ) {
        global_info       = getTimerData( id, message, filename, start, stop );
        timer->timer_data = global_info;
    }
    // Check the status of the timer
    int global_start = global_info->start_line;
    int global_stop  = global_info->stop_line;
    bool check = ( start != -1 && start != global_start ) || ( stop != -1 && stop != global_stop );
    if ( check ) {
        // Check if the timer is incomplete and modify accordingly
        // Note:  Technically this should be a blocking call, however it
        //        is possible to update the start/stop lines directly.
        if ( start != -1 && global_start == -1 )
            global_info->start_line = start;
        if ( stop != -1 && global_stop == -1 )
            global_info->stop_line = stop;
        global_start = global_info->start_line;
        global_stop  = global_info->stop_line;
        // Check for multiple start lines
        if ( start != -1 && global_start != start ) {
            std::string msg;
            msg = "Multiple start calls with the same message are not allowed (" +
                  global_info->message + " in " + global_info->filename + " at lines " +
                  to_string( start ) + ", " + to_string( global_info->start_line ) + ")\n";
            throw std::logic_error( msg );
        }
        // Check for multiple stop lines
        if ( stop != -1 && global_stop != stop ) {
            std::string msg;
            msg = "Multiple start calls with the same message are not allowed (" +
                  global_info->message + " in " + global_info->filename + " at lines " +
                  to_string( stop ) + ", " + to_string( global_info->stop_line ) + ")\n";
            throw std::logic_error( msg );
        }
    }
    return timer;
}


/***********************************************************************
 * Function to return a unique id based on the message and filename.    *
 * Note:  We want to return a unique (but deterministic) id for each    *
 * filename/message pair.  We want each process or thread to return the *
 * same id independent of the other calls.                              *
 ***********************************************************************/
constexpr inline const char* ProfilerApp::stripPath( const char* filename )
{
    const char* s = filename;
    while ( *s ) {
        if ( *s == 47 || *s == 92 )
            filename = s + 1;
        ++s;
    }
    return filename;
}
constexpr inline uint32_t ProfilerApp::hashString( const char* s )
{
    uint32_t c    = 0;
    uint32_t hash = 5381;
    while ( ( c = *s++ ) ) {
        // hash = hash * 33 ^ c
        hash = ( ( hash << 5 ) + hash ) ^ c;
    }
    return hash;
}
constexpr inline uint64_t ProfilerApp::getTimerId( const char* message, const char* filename )
{
    uint32_t v1  = hashString( stripPath( filename ) );
    uint32_t v2  = hashString( message );
    uint64_t key = ( static_cast<uint64_t>( v2 ) << 32 ) + static_cast<uint64_t>( v1^v2 );
    return key;
}


#endif
