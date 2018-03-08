#ifndef included_MemoryApp
#define included_MemoryApp


#include "ProfilerAtomicHelpers.h"

#include <cstring>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 )
#include <Psapi.h>
#include <windows.h>
#elif defined( __APPLE__ )
#include <mach/mach.h>
#else
#include <malloc.h>
#endif


/** \class MemoryApp
 *
 * This class provides some basic utilities for monitoring and querying the memory usage.
 * This class works by overloading the C++ new and delete operators to include memory statistics.
 * It does not monitor direct calls to malloc.  All functions are thread-safe.
 */
class MemoryApp final
{
public:
    /** \class MemoryStats
     * This is a structure to hold memory statistics.  This will include
     * information about the number of bytes allocated/deleted by new/delete
     * throughout the lifetime of the program and the total memory used by
     * the program.  The number of bytes currently in use by new/delete can be
     * obtained by subtracting the deallocated bytes for the allocated bytes.
     * Note: if overloading new/delete is disabled, some of the fields may be zero.
     */
    struct MemoryStats {
        size_t bytes_new;      //!<  Total number of bytes allocated by new
        size_t bytes_delete;   //!<  Total number of bytes de-allocated by new
        size_t N_new;          //!<  Total number of calls to new
        size_t N_delete;       //!<  Total number of calls to delete
        size_t tot_bytes_used; //!<  Total memory used by program stack+heap
        size_t system_memory;  //!<  Total physical memory on the machine
        size_t stack_used;     //!<  An estimate for the current stack size
        size_t stack_size;     //!<  The maximum stack size
        //! Empty constuctor
        MemoryStats() { memset(this,0,sizeof(MemoryStats)); }
    };

    /**
     * @brief  Print memory statistics
     * @details  This function will print some basic memory statistics for new/delete.
     *    More specifically it will print the total number of bytes allocated,
     *    detroyed, in use, and number of calls.
     * @param os            Output stream to print the results
     */
    static void print( std::ostream& os );

    /**
     * @brief  Get the number of bytes in use
     * @details  This function will return the number of bytes in use by new/delete
     */
    static inline size_t getMemoryUsage() { return d_bytes_allocated - d_bytes_deallocated; }

    /**
     * @brief  Return the total memory usage
     * @details  This function will attempt to return the total memory in use
     *   including the stack and memory allocated with malloc.
     */
    static inline size_t getTotalMemoryUsage();

    /*!
     * Function to get the memory availible.
     * This function will return the total memory availible
     * Note: depending on the implimentation, this number may be rounded to
     * to a multiple of the page size.
     * If this function fails, it will return 0.
     */
    static inline size_t getSystemMemory() { return d_physical_memory; }


    /*!
     * Function to get the memory statistics
     */
    static MemoryStats getMemoryStats();

private:
    // Private constructor/destructor
    MemoryApp();
    ~MemoryApp();

    // Private data
    using int64_atomic = TimerUtility::atomic::int64_atomic;
    static int64_atomic d_bytes_allocated;
    static int64_atomic d_bytes_deallocated;
    static int64_atomic d_calls_new;
    static int64_atomic d_calls_delete;
    static size_t d_page_size;
    static size_t d_physical_memory;
    static void* d_base_frame;

#ifndef TIMER_DISABLE_NEW_OVERLOAD
    // Overload new/delete are friends
    friend void* operator new( size_t size );
    friend void* operator new[]( size_t size );
    friend void operator delete( void* data ) noexcept;
    friend void operator delete[]( void* data ) noexcept;
    friend void* operator new( size_t size, const std::nothrow_t& ) noexcept;
    friend void* operator new[]( size_t size, const std::nothrow_t& ) noexcept;
    friend void operator delete(void* data, const std::nothrow_t&) noexcept;
    friend void operator delete[]( void* data, const std::nothrow_t& ) noexcept;
#if CXX_STD == 14
    friend void operator delete( void* data, std::size_t ) noexcept;
    friend void operator delete[]( void* data, std::size_t ) noexcept;
#endif
#endif
};


/***********************************************************************
 * Function to return the current memory usage                          *
 ***********************************************************************/
inline size_t MemoryApp::getTotalMemoryUsage()
{
    size_t N_bytes = 0;
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
#else
    // Linux
    struct mallinfo meminfo = mallinfo();
    size_t size_hblkhd      = static_cast<unsigned int>( meminfo.hblkhd );
    size_t size_uordblks    = static_cast<unsigned int>( meminfo.uordblks );
    N_bytes                 = size_hblkhd + size_uordblks;
    // Correct for possible 32-bit wrap around
    size_t N_bytes_new = d_bytes_allocated - d_bytes_deallocated;
    while ( N_bytes < N_bytes_new )
        N_bytes += 0x100000000;
#endif
    return N_bytes;
}


#endif
