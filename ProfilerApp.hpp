#ifndef included_ProfilerApp_hpp
#define included_ProfilerApp_hpp

#include "ProfilerApp.h"

#include <cassert>
#include <sstream>
#include <stdexcept>


/***********************************************************************
 * Function to get the timer for a particular block of code             *
 * Note: This function performs some blocking as necessary.             *
 ***********************************************************************/
inline ProfilerApp::store_timer* ProfilerApp::getBlock( uint64_t id )
{
    constexpr uint64_t mask = HASH_SIZE - 1;
    uint64_t key            = id & mask; // Get the hash index
    auto thread             = getThreadData();
    store_timer* timer      = thread->timers[key];
    if ( !timer )
        return nullptr;
    while ( timer->id != id ) {
        if ( timer->next == nullptr )
            return nullptr;
        timer = timer->next;
    }
    return timer;
}
inline ProfilerApp::store_timer* ProfilerApp::getBlock( uint64_t id, const char* message,
    const char* filename, int line, bool static_msg, bool static_file )
{
    constexpr uint64_t mask = HASH_SIZE - 1;
    uint64_t key            = id & mask; // Get the hash index
    auto thread             = getThreadData();
    store_timer* timer      = thread->timers[key];
    if ( !timer ) {
        auto new_timer = new store_timer( id, message, filename, line, static_msg, static_file );
        d_bytes.fetch_add( sizeof( store_timer ) );
        new_timer->id       = id;
        thread->timers[key] = new_timer;
        timer               = new_timer;
    }
    while ( timer->id != id ) {
        if ( timer->next == nullptr ) {
            auto new_timer =
                new store_timer( id, message, filename, line, static_msg, static_file );
            d_bytes.fetch_add( sizeof( store_timer ) );
            new_timer->id = id;
            timer->next   = new_timer;
        }
        timer = timer->next;
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
    assert( filename );
    const char* s = filename;
    while ( *s ) {
        if ( *s == 47 || *s == 92 )
            filename = s + 1;
        ++s;
    }
    return filename;
}
constexpr inline uint64_t ProfilerApp::hashString( const char* s )
{
    assert( s );
    uint64_t c    = 0;
    uint64_t hash = 5381;
    while ( ( c = *s++ ) ) {
        // hash = hash * 33 ^ c
        hash = ( ( hash << 5 ) + hash ) ^ c;
    }
    return hash;
}
TIMER_CONSTEVAL inline uint64_t ProfilerApp::getTimerId(
    const char* message, const char* filename, int line )
{
    uint64_t v1 = static_cast<uint64_t>( hashString( stripPath( filename ) ) ) << 32;
    uint64_t v2 = hashString( message );
    uint64_t v3 = 0x9E3779B97F4A7C15 * line;
    return v1 ^ v2 ^ v3;
}
inline uint64_t ProfilerApp::getTimerId2( const char* message, const char* filename, int line )
{
    uint64_t v1 = static_cast<uint64_t>( hashString( stripPath( filename ) ) ) << 32;
    uint64_t v2 = hashString( message );
    uint64_t v3 = 0x9E3779B97F4A7C15 * line;
    return v1 ^ v2 ^ v3;
}


#endif
