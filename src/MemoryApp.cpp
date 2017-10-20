#include "MemoryApp.h"
#include "ProfilerAtomicHelpers.h"
#include <algorithm>
#include <cmath>


// clang-format off
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
    // Using windows
    #include <malloc.h>
    #include <process.h>
    #include <stdlib.h>
    #include <windows.h>
    #define get_malloc_size( X ) _msize( X )
#elif defined( __APPLE__ )
    // Using MAC
    #include <libkern/OSAtomic.h>
    #include <malloc/malloc.h>
    #include <pthread.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>
    #define get_malloc_size( X ) malloc_size( X )
#elif defined( __linux ) || defined( __unix ) || defined( __posix )
    // Using Linux
    #include <malloc.h>
    #include <pthread.h>
    #include <sys/types.h>
    #include <unistd.h>
    #define get_malloc_size( X ) malloc_usable_size( X )
#else
    #error Unknown OS
#endif
// clang-format on


/***********************************************************************
 * Return the physical system memory                                    *
 ***********************************************************************/
static size_t getPhysicalMemory()
{
    size_t N_bytes = 0;
#if defined( USE_LINUX )
    auto page_size    = static_cast<size_t>( sysconf( _SC_PAGESIZE ) );
    static long pages = sysconf( _SC_PHYS_PAGES );
    N_bytes           = pages * page_size;
#elif defined( USE_MAC )
    int mib[2]    = { CTL_HW, HW_MEMSIZE };
    u_int namelen = sizeof( mib ) / sizeof( mib[0] );
    uint64_t size;
    size_t len = sizeof( size );
    if ( sysctl( mib, namelen, &size, &len, NULL, 0 ) == 0 )
        N_bytes = size;
#elif defined( USE_WINDOWS )
    MEMORYSTATUSEX status;
    status.dwLength = sizeof( status );
    GlobalMemoryStatusEx( &status );
    N_bytes = status.ullTotalPhys;
#endif
    return N_bytes;
}


/***********************************************************************
 * Initialize variables                                                 *
 ***********************************************************************/
TimerUtility::atomic::int64_atomic MemoryApp::d_bytes_allocated   = 0;
TimerUtility::atomic::int64_atomic MemoryApp::d_bytes_deallocated = 0;
TimerUtility::atomic::int64_atomic MemoryApp::d_calls_new         = 0;
TimerUtility::atomic::int64_atomic MemoryApp::d_calls_delete      = 0;
#if defined( USE_MAC ) || defined( USE_LINUX )
size_t MemoryApp::d_page_size = static_cast<size_t>( sysconf( _SC_PAGESIZE ) );
#else
size_t MemoryApp::d_page_size = 0;
#endif
size_t MemoryApp::d_physical_memory = getPhysicalMemory();
#if USING_GCC
void* MemoryApp::d_base_frame = __builtin_frame_address( 0 );
#else
void* MemoryApp::d_base_frame = 0;
#endif


/***********************************************************************
 * Functions to overload new/delete                                     *
 ***********************************************************************/
#ifndef TIMER_DISABLE_NEW_OVERLOAD
void* operator new( size_t size )
{
    void* ret = malloc( size );
    if ( !ret )
        throw std::bad_alloc();
    const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( ret );
    TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_allocated, block_size );
    TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_new );
    return ret;
}
void* operator new[]( size_t size )
{
    void* ret = malloc( size );
    if ( !ret )
        throw std::bad_alloc();
    const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( ret );
    TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_allocated, block_size );
    TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_new );
    return ret;
}
void* operator new( size_t size, const std::nothrow_t& ) noexcept
{
    void* ret = malloc( size );
    if ( !ret )
        return nullptr;
    const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( ret );
    TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_allocated, block_size );
    TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_new );
    return ret;
}
void* operator new[]( size_t size, const std::nothrow_t& ) noexcept
{
    void* ret = malloc( size );
    if ( !ret )
        return nullptr;
    const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( ret );
    TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_allocated, block_size );
    TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_new );
    return ret;
}
void operator delete( void* data ) noexcept
{
    if ( data != nullptr ) {
        const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( data );
        free( data );
        TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_deallocated, block_size );
        TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_delete );
    }
}
void operator delete[]( void* data ) noexcept
{
    if ( data != nullptr ) {
        const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( data );
        free( data );
        TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_deallocated, block_size );
        TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_delete );
    }
}
void operator delete(void* data, const std::nothrow_t&) noexcept
{
    if ( data != nullptr ) {
        const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( data );
        free( data );
        TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_deallocated, block_size );
        TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_delete );
    }
}
void operator delete[]( void* data, const std::nothrow_t& ) noexcept
{
    if ( data != nullptr ) {
        const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( data );
        free( data );
        TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_deallocated, block_size );
        TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_delete );
    }
}
#if CXX_STD == 14
void operator delete( void* data, std::size_t ) noexcept
{
    if ( data != NULL ) {
        const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( data );
        free( data );
        TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_deallocated, block_size );
        TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_delete );
    }
}
void operator delete[]( void* data, std::size_t ) noexcept
{
    if ( data != NULL ) {
        const TimerUtility::atomic::int64_atomic block_size = get_malloc_size( data );
        free( data );
        TimerUtility::atomic::atomic_add( &MemoryApp::d_bytes_deallocated, block_size );
        TimerUtility::atomic::atomic_increment( &MemoryApp::d_calls_delete );
    }
}
#endif
#endif


/***********************************************************************
 * Class functions                                                      *
 ***********************************************************************/
#if defined( _GNU_SOURCE ) || defined( USING_GCC )
static size_t subtract_address_abs( const void* x1, const void* x2 )
{
    // Return the absolute difference between two addresses abs(x-y)
    auto y1      = reinterpret_cast<size_t>( x1 );
    auto y2      = reinterpret_cast<size_t>( x2 );
    auto v1      = static_cast<int64_t>( y1 );
    auto v2      = static_cast<int64_t>( y2 );
    int64_t diff = v1 > v2 ? ( v1 - v2 ) : ( v2 - v1 );
    return static_cast<size_t>( diff );
}
#endif
MemoryApp::MemoryStats MemoryApp::getMemoryStats()
{
    MemoryStats stats;
    stats.bytes_new      = MemoryApp::d_bytes_allocated;
    stats.bytes_delete   = MemoryApp::d_bytes_deallocated;
    stats.N_new          = MemoryApp::d_calls_new;
    stats.N_delete       = MemoryApp::d_calls_delete;
    stats.tot_bytes_used = MemoryApp::getTotalMemoryUsage();
    stats.system_memory  = MemoryApp::d_physical_memory;
    stats.stack_used     = 0;
    stats.stack_size     = 0;
#if defined( _GNU_SOURCE )
    pthread_attr_t attr;
    void* stackaddr;
    size_t stacksize;
    pthread_getattr_np( pthread_self(), &attr );
    pthread_attr_getstack( &attr, &stackaddr, &stacksize );
    pthread_attr_destroy( &attr );
    stats.stack_used = stacksize - subtract_address_abs( stackaddr, &stackaddr );
    stats.stack_size = stacksize;
#elif defined( USING_GCC )
    stats.stack_used = subtract_address_abs( d_base_frame, __builtin_frame_address( 0 ) );
#endif
    return stats;
}
void MemoryApp::print( std::ostream& os )
{
    MemoryStats stats = getMemoryStats();
    os << "Statistics from new/delete:\n";
    os << "   Bytes allocated:   " << stats.bytes_new << std::endl;
    os << "   Bytes deallocated: " << stats.bytes_delete << std::endl;
    os << "   Bytes in use: " << stats.bytes_new - stats.bytes_delete << std::endl;
    os << "   Number of calls to new:    " << stats.N_new << std::endl;
    os << "   Number of calls to delete: " << stats.N_delete << std::endl;
    os << "   Number of calls to new without delete: " << stats.N_new - stats.N_delete << std::endl;
    os << "   Total memory in use: " << stats.tot_bytes_used << std::endl;
    os << "   Stack used: " << stats.stack_used << std::endl;
    os << "   Stack size: " << stats.stack_size << std::endl;
}
