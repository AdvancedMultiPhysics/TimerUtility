#include "MemoryApp.h"

#include <algorithm>
#include <cmath>
#include <string>


// clang-format off
#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
    // Using windows
    #include <malloc.h>
    #include <process.h>
    #include <stdlib.h>
    // clang-format off
    #include <windows.h>
    #include <Psapi.h>
    // clang-format on
    #define get_malloc_size( X ) _msize( X )
#elif defined( __APPLE__ )
    // Using MAC
    #include <libkern/OSAtomic.h>
    #include <mach/mach.h>
    #include <malloc/malloc.h>
    #include <pthread.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>
    #define get_malloc_size( X ) malloc_size( X )
#elif defined( __linux ) || defined( __linux__ ) || defined( __unix ) || defined( __posix )
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
    if ( sysctl( mib, namelen, &size, &len, nullptr, 0 ) == 0 )
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
volatile std::atomic_int64_t MemoryApp::d_bytes_allocated( 0 );
volatile std::atomic_int64_t MemoryApp::d_bytes_deallocated( 0 );
volatile std::atomic_int64_t MemoryApp::d_calls_new( 0 );
volatile std::atomic_int64_t MemoryApp::d_calls_delete( 0 );
#if defined( USE_MAC ) || defined( USE_LINUX )
size_t MemoryApp::d_page_size = static_cast<size_t>( sysconf( _SC_PAGESIZE ) );
#else
size_t MemoryApp::d_page_size = 0;
#endif
size_t MemoryApp::d_physical_memory = getPhysicalMemory();
#if defined( __GNUC__ )
void* MemoryApp::d_base_frame = __builtin_frame_address( 0 );
#else
void* MemoryApp::d_base_frame = 0;
#endif


/***********************************************************************
 * Function to return the current memory usage                          *
 ***********************************************************************/
// clang-format off
size_t MemoryApp::getTotalMemoryUsage() noexcept
{
    size_t N_bytes = 0;
    try {
        #if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
            // Windows
            PROCESS_MEMORY_COUNTERS memCounter;
            GetProcessMemoryInfo( GetCurrentProcess(), &memCounter, sizeof( memCounter ) );
            N_bytes = memCounter.WorkingSetSize;
        #elif defined( __APPLE__ )
            // MAC
            struct task_basic_info t_info;
            mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
            kern_return_t rtn =
                task_info( mach_task_self(), TASK_BASIC_INFO, (task_info_t) &t_info, &t_info_count );
            if ( rtn != KERN_SUCCESS )
                return 0;
            N_bytes = t_info.virtual_size;
        #elif defined( HAVE_MALLINFO2 )
            // Linux - mallinfo2
            auto meminfo = mallinfo2();
            N_bytes      = meminfo.hblkhd + meminfo.uordblks;
        #else
            // Linux - Deprecated mallinfo
            auto meminfo = mallinfo();
            size_t size_hblkhd   = static_cast<unsigned int>( meminfo.hblkhd );
            size_t size_uordblks = static_cast<unsigned int>( meminfo.uordblks );
            N_bytes              = size_hblkhd + size_uordblks;
            // Correct for possible 32-bit wrap around
            size_t N_bytes_new = d_bytes_allocated - d_bytes_deallocated;
            while ( N_bytes < N_bytes_new )
                N_bytes += 0x100000000;
        #endif
    } catch ( ... ) {
        N_bytes = d_bytes_allocated - d_bytes_deallocated;
    }
    return N_bytes;
}
// clang-format on


/***********************************************************************
 * Overload new/delete                                                  *
 ***********************************************************************/
#ifndef TIMER_DISABLE_NEW_OVERLOAD
void* operator new( std::size_t size )
{
    void* ret = malloc( size );
    if ( !ret )
        throw std::bad_alloc();
    auto block_size = get_malloc_size( ret );
    MemoryApp::d_bytes_allocated.fetch_add( block_size );
    ++MemoryApp::d_calls_new;
    return ret;
}
void* operator new[]( std::size_t size )
{
    void* ret = malloc( size );
    if ( !ret )
        throw std::bad_alloc();
    auto block_size = get_malloc_size( ret );
    MemoryApp::d_bytes_allocated.fetch_add( block_size );
    ++MemoryApp::d_calls_new;
    return ret;
}
void* operator new( std::size_t size, const std::nothrow_t& ) noexcept
{
    void* ret = malloc( size );
    if ( !ret )
        return nullptr;
    auto block_size = get_malloc_size( ret );
    MemoryApp::d_bytes_allocated.fetch_add( block_size );
    ++MemoryApp::d_calls_new;
    return ret;
}
void* operator new[]( std::size_t size, const std::nothrow_t& ) noexcept
{
    void* ret = malloc( size );
    if ( !ret )
        return nullptr;
    auto block_size = get_malloc_size( ret );
    MemoryApp::d_bytes_allocated.fetch_add( block_size );
    ++MemoryApp::d_calls_new;
    return ret;
}
void operator delete( void* data ) noexcept
{
    if ( data != nullptr ) {
        auto block_size = get_malloc_size( data );
        free( data );
        MemoryApp::d_bytes_deallocated.fetch_add( block_size );
        ++MemoryApp::d_calls_delete;
    }
}
void operator delete[]( void* data ) noexcept
{
    if ( data != nullptr ) {
        auto block_size = get_malloc_size( data );
        free( data );
        MemoryApp::d_bytes_deallocated.fetch_add( block_size );
        ++MemoryApp::d_calls_delete;
    }
}
void operator delete( void* data, const std::nothrow_t& ) noexcept
{
    if ( data != nullptr ) {
        auto block_size = get_malloc_size( data );
        free( data );
        MemoryApp::d_bytes_deallocated.fetch_add( block_size );
        ++MemoryApp::d_calls_delete;
    }
}
void operator delete[]( void* data, const std::nothrow_t& ) noexcept
{
    if ( data != nullptr ) {
        auto block_size = get_malloc_size( data );
        free( data );
        MemoryApp::d_bytes_deallocated.fetch_add( block_size );
        ++MemoryApp::d_calls_delete;
    }
}
void operator delete( void* data, std::size_t ) noexcept
{
    if ( data != nullptr ) {
        auto block_size = get_malloc_size( data );
        free( data );
        MemoryApp::d_bytes_deallocated.fetch_add( block_size );
        ++MemoryApp::d_calls_delete;
    }
}
void operator delete[]( void* data, std::size_t ) noexcept
{
    if ( data != nullptr ) {
        auto block_size = get_malloc_size( data );
        free( data );
        MemoryApp::d_bytes_deallocated.fetch_add( block_size );
        ++MemoryApp::d_calls_delete;
    }
}
#endif


/***********************************************************************
 * Class functions                                                      *
 ***********************************************************************/
#if defined( _GNU_SOURCE ) || defined( __GNUC__ )
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
#elif defined( __GNUC__ )
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


/****************************************************************************
 *  Check if we are running withing valgrind                                 *
 ****************************************************************************/
#if __has_include( "valgrind.h" )
#include "valgrind.h"
static bool running_valgrind() { return RUNNING_ON_VALGRIND; }
#else
static bool running_valgrind()
{
    std::string x;
    auto tmp = std::getenv( "LD_PRELOAD" );
    if ( tmp )
        x = std::string( tmp );
    size_t pos = std::min<size_t>( x.find( "/valgrind/" ), x.find( "/vgpreload" ) );
    return pos != std::string::npos;
}
#endif
const bool MemoryApp::d_valgrind = running_valgrind();
