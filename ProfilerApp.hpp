#ifndef included_ProfilerApp_hpp
#define included_ProfilerApp_hpp

#include "ProfilerApp.h"

#include <sstream>
#include <stdexcept>


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
 * Integer log2                                                         *
 ***********************************************************************/
constexpr int log2int( uint64_t x )
{
    int r = 0;
    while ( x > 1 ) {
        r++;
        x = x >> 1;
    }
    return r;
}


/***********************************************************************
 * Get the time elapsed in ns                                           *
 * Note we implement this because duration_cast takes too long in debug *
 ***********************************************************************/
template<class TYPE>
constexpr int64_t diff_ns( std::chrono::time_point<TYPE> t2, std::chrono::time_point<TYPE> t1 )
{
    using PERIOD = typename std::chrono::time_point<TYPE>::period;
    if constexpr ( std::ratio_equal_v<PERIOD, std::nano> &&
                   sizeof( std::chrono::time_point<TYPE> ) == 8 ) {
        return *reinterpret_cast<const int64_t*>( &t2 ) - *reinterpret_cast<const int64_t*>( &t1 );
    } else {
        return std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count();
    }
}


/***********************************************************************
 * Function to get the timer for a particular block of code             *
 * Note: This function performs some blocking as necessary.             *
 ***********************************************************************/
inline ProfilerApp::store_timer* ProfilerApp::getBlock( uint64_t id )
{
    uint32_t thread_id = getThreadID();
    if ( thread_id >= MAX_THREADS )
        return nullptr;
    constexpr uint64_t mask = ( (uint64_t) 0x1 << log2int( TIMER_HASH_SIZE ) ) - 1;
    static_assert( mask + 1 == TIMER_HASH_SIZE );
    uint64_t key       = id & mask; // Get the hash index
    store_timer* timer = d_threadData[thread_id].timers[key];
    if ( !timer )
        return nullptr;
    while ( timer->id != id ) {
        if ( timer->next == nullptr )
            return nullptr;
        timer = timer->next;
    }
    return timer;
}
inline ProfilerApp::store_timer* ProfilerApp::getBlock(
    uint64_t id, const char* message, const char* filename, int line )
{
    uint32_t thread_id = getThreadID();
    if ( thread_id >= MAX_THREADS )
        return nullptr;
    constexpr uint64_t mask = ( (uint64_t) 0x1 << log2int( TIMER_HASH_SIZE ) ) - 1;
    static_assert( mask + 1 == TIMER_HASH_SIZE );
    uint64_t key       = id & mask; // Get the hash index
    store_timer* timer = d_threadData[thread_id].timers[key];
    if ( !timer ) {
        auto new_timer = new store_timer;
        d_bytes.fetch_add( sizeof( store_timer ) );
        new_timer->id                       = id;
        new_timer->timer_data               = getTimerData( id, message, filename, line );
        d_threadData[thread_id].timers[key] = new_timer;
        timer                               = new_timer;
    }
    while ( timer->id != id ) {
        if ( timer->next == nullptr ) {
            auto new_timer = new store_timer;
            d_bytes.fetch_add( sizeof( store_timer ) );
            new_timer->id         = id;
            new_timer->timer_data = getTimerData( id, message, filename, line );
            timer->next           = new_timer;
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
constexpr inline std::string_view ProfilerApp::stripPath( std::string_view filename )
{
    if ( filename.empty() )
        return std::string_view();
    const char* s = filename.data();
    while ( *s ) {
        if ( *s == 47 || *s == 92 )
            filename = s + 1;
        ++s;
    }
    return filename;
}
constexpr inline uint64_t ProfilerApp::hashString( std::string_view str )
{
    if ( str.empty() )
        return 0;
    const char* s = str.data();
    uint64_t c    = 0;
    uint64_t hash = 5381;
    while ( ( c = *s++ ) ) {
        // hash = hash * 33 ^ c
        hash = ( ( hash << 5 ) + hash ) ^ c;
    }
    return hash;
}
constexpr inline uint64_t ProfilerApp::getTimerId(
    const char* message, const char* filename, int line )
{
    uint64_t v1 = static_cast<uint64_t>( hashString( stripPath( filename ) ) ) << 32;
    uint64_t v2 = hashString( message );
    uint64_t v3 = 0x9E3779B97F4A7C15 * line;
    return v1 ^ v2 ^ v3;
}


#endif
