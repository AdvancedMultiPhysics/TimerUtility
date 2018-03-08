#ifndef included_TimerUtilityAtomicHelpers
#define included_TimerUtilityAtomicHelpers
#include <stdint.h>
#include <stdio.h>
#include <typeinfo>

// Choose the OS
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
// Using windows
#define USE_WINDOWS
#define NOMINMAX
#include <process.h>
#include <stdlib.h>
#include <windows.h>
#elif defined( __APPLE__ )
// Using MAC
#define USE_MAC
#include <libkern/OSAtomic.h>
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
// Using Linux
#define USE_LINUX
#include <unistd.h>
#if !defined( __GNUC__ )
#define USE_PTHREAD_ATOMIC_LOCK
#include "pthread.h"
#endif
#else
#error Unknown OS
#endif


namespace TimerUtility {


/** \namespace atomic
 * \brief Functions for atomic operations
 * \details This class provides wrapper routines to access simple atomic operations.
 *    Since atomic operations are system dependent, these functions are necessary
 *    to provide a platform independent interface.  We also provide some typedef
 *    variables to wrap OS dependencies.  Currently we have 32 and 64 bit integers:
 *    int32_atomic and int64_atomic.  In all cases the operations use the barrier
 *    versions provided by the compiler/OS if availible.  In most cases, these builtins
 *    are considered a full barrier. That is, no memory operand will be moved across
 *    the operation, either forward or backward. Further, instructions will be issued
 *    as necessary to prevent the processor from speculating loads across the operation
 *    and from queuing stores after the operation.
 *    Note: for all functions the variable being modified must be volatile to prevent
 *    compiler optimization that may cache the value.
 */
namespace atomic {


// Define int32_atomic, int64_atomic
#include <stdint.h>
#if defined( USE_WINDOWS )
typedef long int32_atomic;
typedef __int64 int64_atomic;
#define NO_INST_ATTR
#elif defined( USE_MAC )
typedef int32_t int32_atomic;
typedef int64_t int64_atomic;
#define NO_INST_ATTR
#elif defined( __GNUC__ )
typedef int int32_atomic;
typedef long int int64_atomic;
#define NO_INST_ATTR __attribute__( ( no_instrument_function ) )
#elif defined( USE_PTHREAD_ATOMIC_LOCK )
typedef int int32_atomic;
typedef long int int64_atomic;
#define NO_INST_ATTR
#else
#error Unknown OS
#endif


/**
 * \brief Increment returning the new value
 * \details Increment x and return the new value
 * \param[in] x     The pointer to the value to increment
 */
inline int32_atomic atomic_increment( int32_atomic volatile *x ) NO_INST_ATTR;

/**
 * \brief Increment returning the new value
 * \details Increment x and return the new value
 * \param[in] x     The pointer to the value to increment
 */
inline int64_atomic atomic_increment( int64_atomic volatile *x ) NO_INST_ATTR;

/**
 * \brief Decrement returning the new value
 * \details Decrement x and return the new value
 * \param[in] x     The pointer to the value to decrement
 */
inline int32_atomic atomic_decrement( int32_atomic volatile *x ) NO_INST_ATTR;

/**
 * \brief Decrement returning the new value
 * \details Decrement x and return the new value
 * \param[in] x     The pointer to the value to decrement
 */
inline int64_atomic atomic_decrement( int64_atomic volatile *x ) NO_INST_ATTR;

/**
 * \brief Add returning the new value
 * \details Add y to x and return the new value
 * \param[in] x     The pointer to the value to add to
 * \param[in] y     The value to add
 */
inline int32_atomic atomic_add( int32_atomic volatile *x, int32_atomic y ) NO_INST_ATTR;

/**
 * \brief Add returning the new value
 * \details Add y to x and return the new value
 * \param[in] x     The pointer to the value to add to
 * \param[in] y     The value to add
 */
inline int64_atomic atomic_add( int64_atomic volatile *x, int64_atomic y ) NO_INST_ATTR;

/**
 * \brief Compare the given value and swap
 * \details Compare the existing value and swap if it matches.
 * \return Returns true if the swap was performed
 * \param[in] v     The pointer to the value to check and swap
 * \param[in] x     The value to compare
 * \param[in] y     The value to swap iff *v==x
 */
inline bool atomic_compare_and_swap( int32_atomic volatile *v, int32_atomic x, int32_atomic y );

/**
 * \brief Compare the given value and swap
 * \details Compare the existing value and swap if it matches.
 * \return Returns true if the swap was performed
 * \param[in] v     The pointer to the value to check and swap
 * \param[in] x     The value to compare
 * \param[in] y     The value to swap iff *v==x
 */
inline bool atomic_compare_and_swap( int64_atomic volatile *v, int64_atomic x, int64_atomic y );


// Define increment/decrement/add operators for int32, int64
#if defined( USE_WINDOWS )
inline int32_atomic atomic_increment( int32_atomic volatile *x )
{
    return InterlockedIncrement( x );
}
inline int64_atomic atomic_increment( int64_atomic volatile *x )
{
    return InterlockedIncrement64( x );
}
inline int32_atomic atomic_decrement( int32_atomic volatile *x )
{
    return InterlockedDecrement( x );
}
inline int64_atomic atomic_decrement( int64_atomic volatile *x )
{
    return InterlockedDecrement64( x );
}
inline int32_atomic atomic_add( int32_atomic volatile *x, int32_atomic y )
{
    return InterlockedExchangeAdd( x, y ) + y;
}
inline int64_atomic atomic_add( int64_atomic volatile *x, int64_atomic y )
{
    return InterlockedExchangeAdd64( x, y ) + y;
}
inline bool atomic_compare_and_swap( int32_atomic volatile *v, int32_atomic x, int32_atomic y )
{
    return InterlockedCompareExchange( v, y, x ) == x;
}
inline bool atomic_compare_and_swap( int64_atomic volatile *v, int64_atomic x, int64_atomic y )
{
    return InterlockedCompareExchange64( v, y, x ) == x;
}
#elif defined( USE_MAC )
inline int32_atomic atomic_increment( int32_atomic volatile *x )
{
    return OSAtomicIncrement32Barrier( x );
}
inline int64_atomic atomic_increment( int64_atomic volatile *x )
{
    return OSAtomicIncrement64Barrier( x );
}
inline int32_atomic atomic_decrement( int32_atomic volatile *x )
{
    return OSAtomicDecrement32Barrier( x );
}
inline int64_atomic atomic_decrement( int64_atomic volatile *x )
{
    return OSAtomicDecrement64Barrier( x );
}
inline int32_atomic atomic_add( int32_atomic volatile *x, int32_atomic y )
{
    return OSAtomicAdd32Barrier( y, x );
}
inline int64_atomic atomic_add( int64_atomic volatile *x, int64_atomic y )
{
    return OSAtomicAdd64Barrier( y, x );
}
inline bool atomic_compare_and_swap( int32_atomic volatile *v, int32_atomic x, int32_atomic y )
{
    return OSAtomicCompareAndSwap32Barrier( x, y, v );
}
inline bool atomic_compare_and_swap( int64_atomic volatile *v, int64_atomic x, int64_atomic y )
{
    return OSAtomicCompareAndSwap64Barrier( x, y, v );
}
#elif defined( __GNUC__ )
int32_atomic atomic_increment( int32_atomic volatile *x ) { return __sync_add_and_fetch( x, 1 ); }
int64_atomic atomic_increment( int64_atomic volatile *x ) { return __sync_add_and_fetch( x, 1 ); }
int32_atomic atomic_decrement( int32_atomic volatile *x ) { return __sync_sub_and_fetch( x, 1 ); }
int64_atomic atomic_decrement( int64_atomic volatile *x ) { return __sync_sub_and_fetch( x, 1 ); }
inline int32_atomic atomic_add( int32_atomic volatile *x, int32_atomic y )
{
    return __sync_add_and_fetch( x, y );
}
inline int64_atomic atomic_add( int64_atomic volatile *x, int64_atomic y )
{
    return __sync_add_and_fetch( x, y );
}
inline bool atomic_compare_and_swap( int32_atomic volatile *v, int32_atomic x, int32_atomic y )
{
    return __sync_bool_compare_and_swap( v, x, y );
}
inline bool atomic_compare_and_swap( int64_atomic volatile *v, int64_atomic x, int64_atomic y )
{
    return __sync_bool_compare_and_swap( v, x, y );
}
#elif defined( USE_PTHREAD_ATOMIC_LOCK )
extern pthread_mutex_t atomic_pthread_lock;
inline int32_atomic atomic_increment( int32_atomic volatile *x )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    int32_atomic y = ++( *x );
    pthread_mutex_unlock( &atomic_pthread_lock );
    return y;
}
inline int64_atomic atomic_increment( int64_atomic volatile *x )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    int64_atomic y = ++( *x );
    pthread_mutex_unlock( &atomic_pthread_lock );
    return y;
}
inline int32_atomic atomic_decrement( int32_atomic volatile *x )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    int32_atomic y = --( *x );
    pthread_mutex_unlock( &atomic_pthread_lock );
    return y;
}
inline int64_atomic atomic_decrement( int64_atomic volatile *x )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    int64_atomic y = --( *x );
    pthread_mutex_unlock( &atomic_pthread_lock );
    return y;
}
inline int32_atomic atomic_add( int32_atomic volatile *x, int32_atomic y )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    *x += y;
    int32_atomic z = *x;
    pthread_mutex_unlock( &atomic_pthread_lock );
    return z;
}
inline int64_atomic atomic_add( int64_atomic volatile *x, int64_atomic y )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    *x += y;
    int64_atomic z = *x;
    pthread_mutex_unlock( &atomic_pthread_lock );
    return z;
}
inline bool atomic_compare_and_swap( int32_atomic volatile *v, int32_atomic x, int32_atomic y )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    bool test = *v == x;
    *v        = test ? y : x;
    pthread_mutex_unlock( &atomic_pthread_lock );
    return test;
}
inline bool atomic_compare_and_swap( int64_atomic volatile *v, int64_atomic x, int64_atomic y )
{
    pthread_mutex_lock( &atomic_pthread_lock );
    bool test = *v == x;
    *v        = test ? y : x;
    pthread_mutex_unlock( &atomic_pthread_lock );
    return test;
}
#else
#error Unknown OS
#endif


// Define an atomic counter
struct counter_t {
public:
    // Constructor
    inline counter_t() : count( 0 ) {}
    // Destructor
    inline ~counter_t() {} // Destructor
    // Increment returning the new value
    inline int increment() { return atomic_increment( &count ); }
    // Decrement returning the new value
    inline int decrement() { return atomic_decrement( &count ); }
    // Set the current value of the count
    inline void setCount( int val ) { count = val; }
    // Get the current value of the count
    inline int getCount() const { return count; }

private:
    counter_t( const counter_t & );
    counter_t &operator=( const counter_t & );
    volatile int32_atomic count;
};


} // namespace atomic
} // namespace TimerUtility


#endif
