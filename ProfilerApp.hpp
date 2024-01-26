#ifndef included_ProfilerApp_hpp
#define included_ProfilerApp_hpp

#include "ProfilerApp.h"

#include <sstream>
#include <stdexcept>


// Use the hashing function 2^64*0.5*(sqrt(5)-1)
#define GET_TIMER_HASH( id ) ( ( ( id * 0x9E3779B97F4A7C15 ) >> 48 ) % TIMER_HASH_SIZE )


/***********************************************************************
 * uint16f                                                              *
 ***********************************************************************/
constexpr uint16_t uint16f::getData( uint64_t x )
{
    uint16_t y = x;
    if ( x > 2048 ) {
        uint64_t tmp = x >> 11;
        uint16_t e   = 0;
        while ( tmp != 0 ) {
            e++;
            tmp >>= 1;
        }
        uint16_t b = x >> ( e - 1 ) & 0x7FF;
        y          = ( e << 11 ) | b;
        if ( e >= 32 )
            y = 0xFFFF;
    }
    return y;
}
constexpr uint16f::operator uint64_t() const
{
    uint16_t e = data >> 11;
    uint16_t b = data & 0x7FF;
    uint64_t x = b;
    if ( e > 0 ) {
        x = x | 0x800;
        x <<= ( e - 1 );
    }
    return x;
}
namespace std {
template<>
class numeric_limits<uint16f>
{
public:
    static constexpr bool is_specialized() { return true; }
    static constexpr bool is_signed() { return false; }
    static constexpr bool is_integer() { return true; }
    static constexpr bool is_exact() { return false; }
    static constexpr bool has_infinity() { return false; }
    static constexpr bool has_quiet_NaN() { return false; }
    static constexpr bool has_signaling_NaN() { return false; }
    static constexpr bool has_denorm() { return false; }
    static constexpr bool has_denorm_loss() { return false; }
    static constexpr std::float_round_style round_style() { return round_toward_zero; }
    static constexpr bool is_iec559() { return false; }
    static constexpr bool is_bounded() { return true; }
    static constexpr bool is_modulo() { return false; }
    static constexpr int digits() { return 11; }
    static constexpr int digits10() { return 3; }
    static constexpr int max_digits10() { return 4; }
    static constexpr int radix() { return 2; }
    static constexpr int min_exponent() { return 0; }
    static constexpr int min_exponent10() { return 0; }
    static constexpr int max_exponent() { return 42; }
    static constexpr int max_exponent10() { return 13; }
    static constexpr bool traps() { return false; }
    static constexpr bool tinyness_before() { return false; }
    static constexpr uint16f min() { return uint16f( 0 ); }
    static constexpr uint16f lowest() { return uint16f( 0 ); }
    static constexpr uint16f max() { return uint16f( ~( (uint64_t) 0 ) ); }
    static constexpr uint16f epsilon() { return uint16f( 1 ); }
    static constexpr uint16f round_error() { return uint16f( 0 ); }
    static constexpr uint16f infinity() { return uint16f( 0 ); }
    static constexpr uint16f quiet_NaN() { return uint16f( 0 ); }
    static constexpr uint16f signaling_NaN() { return uint16f( 0 ); }
    static constexpr uint16f denorm_min() { return uint16f( 0 ); }
};
} // namespace std


/***********************************************************************
 * Function to get the timmer for a particular block of code            *
 * Note: This function performs some blocking as necessary.             *
 ***********************************************************************/
inline ProfilerApp::store_timer* ProfilerApp::getBlock(
    uint64_t id, bool create, std::string_view message, std::string_view filename, int line )
{
    using std::to_string;
    uint32_t thread_id = getThreadID();
    if ( thread_id >= MAX_THREADS )
        return nullptr;
    // Search for the thread-specific timer and create it if necessary (does not need blocking)
    size_t key         = GET_TIMER_HASH( id ); // Get the hash index
    store_timer* timer = d_threadData[thread_id].timers[key];
    if ( !timer ) {
        if ( !create )
            return nullptr;
        // The timer does not exist, create it
        auto new_timer = new store_timer;
        d_bytes.fetch_add( sizeof( store_timer ) );
        new_timer->id                       = id;
        new_timer->is_active                = false;
        new_timer->trace_index              = d_threadData[thread_id].N++;
        new_timer->timer_data               = getTimerData( id, message, filename, line );
        d_threadData[thread_id].timers[key] = new_timer;
        timer                               = new_timer;
    }
    while ( timer->id != id ) {
        // Check if there is another entry to check (and create one if necessary)
        if ( timer->next == nullptr ) {
            if ( !create )
                return nullptr;
            auto new_timer = new store_timer;
            d_bytes.fetch_add( sizeof( store_timer ) );
            new_timer->id          = id;
            new_timer->is_active   = false;
            new_timer->trace_index = d_threadData[thread_id].N++;
            new_timer->timer_data  = getTimerData( id, message, filename, line );
            timer->next            = new_timer;
        }
        // Advance to the next entry
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
constexpr inline std::string_view ProfilerApp::stripPath( std::string_view filename )
{
    const char* s = filename.data();
    while ( *s ) {
        if ( *s == 47 || *s == 92 )
            filename = s + 1;
        ++s;
    }
    return filename;
}
constexpr inline uint32_t ProfilerApp::hashString( std::string_view str )
{
    const char* s = str.data();
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
    uint64_t key = ( static_cast<uint64_t>( v2 ) << 32 ) + static_cast<uint64_t>( v1 ^ v2 );
    return key;
}


#endif
