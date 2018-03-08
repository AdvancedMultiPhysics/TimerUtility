#ifndef included_ProfilerApp_hpp
#define included_ProfilerApp_hpp

#include "ProfilerApp.h"

#include <sstream>
#include <stdexcept>


// Inline function to get the filename without the path
CONSTEXPR_TIMER inline const char* strip_path( const char* filename_in )
{
    const char* filename = filename_in;
    const char* s        = filename_in;
    while ( *s ) {
        if ( *s == 47 || *s == 92 )
            filename = s + 1;
        ++s;
    }
    return filename;
}


// Use the hashing function 2^32*0.5*(sqrt(5)-1)
#define GET_TIMER_HASH( id ) ( ( ( id * 0x9E3779B97F4A7C15 ) >> 48 ) % TIMER_HASH_SIZE ) 


/***********************************************************************
 * Function to get the timmer for a particular block of code            *
 * Note: This function performs some blocking as necessary.             *
 ***********************************************************************/
inline ProfilerApp::store_timer* ProfilerApp::get_block( thread_info* thread_data, uint64_t id,
    bool create, const char* message, const char* filename, const int start, const int stop )
{
    // Search for the thread-specific timer and create it if necessary (does not need blocking)
    size_t key = GET_TIMER_HASH( id ); // Get the hash index
    if ( thread_data->head[key] == NULL ) {
        if ( !create )
            return nullptr;
        // The timer does not exist, create it
        store_timer* new_timer                  = new store_timer;
        TimerUtility::atomic::int64_atomic size = sizeof( store_timer );
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
        if ( timer->next == NULL ) {
            if ( !create )
                return nullptr;
            store_timer* new_timer                  = new store_timer;
            TimerUtility::atomic::int64_atomic size = sizeof( store_timer );
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
    store_timer_data_info* global_info = timer->timer_data;
    if ( global_info == NULL ) {
        global_info       = get_timer_data( id, message, filename, start, stop );
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
            std::stringstream msg;
            msg << "Multiple start calls with the same message are not allowed ("
                << global_info->message << " in " << global_info->filename << " at lines " << start
                << ", " << global_info->start_line << ")\n";
            throw std::logic_error( msg.str() );
        }
        // Check for multiple stop lines
        if ( stop != -1 && global_stop != stop ) {
            std::stringstream msg;
            msg << "Multiple start calls with the same message are not allowed ("
                << global_info->message << " in " << global_info->filename << " at lines " << stop
                << ", " << global_info->stop_line << ")\n";
            throw std::logic_error( msg.str() );
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
CONSTEXPR_TIMER inline uint64_t ProfilerApp::get_timer_id(
    const char* message, const char* filename )
{
    uint32_t c;
    // Hash the filename using DJB2
    const char* s  = strip_path( filename );
    uint32_t hash1 = 5381;
    while ( ( c = *s++ ) ) {
        // hash = hash * 33 ^ c
        hash1 = ( ( hash1 << 5 ) + hash1 ) ^ c;
    }
    // Hash the message using DJB2
    s              = message;
    uint32_t hash2 = 5381;
    while ( ( c = *s++ ) ) {
        // hash = hash * 33 ^ c
        hash2 = ( ( hash2 << 5 ) + hash2 ) ^ c;
    }
    // Combine the two hashes
    uint64_t key = ( static_cast<uint64_t>( hash1 ) << 16 ) + static_cast<uint64_t>( hash2 );
    return key;
}


#endif
