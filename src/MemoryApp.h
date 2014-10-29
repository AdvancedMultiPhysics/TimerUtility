#ifndef included_MemoryApp
#define included_MemoryApp


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <vector>


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    #define USE_WINDOWS
#elif defined(__APPLE__)
    #define USE_MAC
#else
    #define USE_LINUX
#endif


#ifdef USE_WINDOWS
    // Windows
    #include <windows.h>
    #include <string>
#elif defined(USE_MAC)
    // Mac
    #include <sys/time.h>
    #include <pthread.h>
    #include <string.h>
#elif defined(USE_LINUX)
    // Linux
    #include <sys/time.h>
    #include <pthread.h>
    #include <string.h>
    #include <malloc.h>
#else
    #error Unknown OS
#endif




/** \class MemoryApp
  *
  * This class provides some basic utilities for monitoring and querying the memory usage.
  * This class works by overloading the C++ new and delete operators to include memory statistics.
  * It does not monitor direct calls to malloc.  All functions are thread-safe.
  */
class MemoryApp {
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
        size_t bytes_new;           //!<  Total number of bytes allocated by new
        size_t bytes_delete;        //!<  Total number of bytes de-allocated by new
        size_t N_new;               //!<  Total number of calls to new
        size_t N_delete;            //!<  Total number of calls to delete
        size_t tot_bytes_used;      //!<  Total memory used by program stack+heap
        size_t system_memory;       //!<  Total physical memory on the machine
        size_t stack_used;          //!<  An estimate for the current stack size
        size_t stack_size;          //!<  The maximum stack size
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
    static inline size_t getMemoryUsage( ) { return d_bytes_allocated-d_bytes_deallocated; }

    /**                 
     * @brief  Return the total memory usage
     * @details  This function will attempt to return the total memory in use
     *   including the stack and memory allocated with malloc.
     */
    static inline size_t getTotalMemoryUsage( );

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
    MemoryApp( );
    ~MemoryApp();

    // Private data
    #if defined(USE_WINDOWS)
        typedef __int64 int64_atomic;
    #elif defined(USE_MAC)
        typedef int64_t int64_atomic;
    #elif defined(USE_LINUX)
        typedef long int int64_atomic;
    #else
        #error Unknown OS
    #endif
    static int64_atomic d_bytes_allocated;
    static int64_atomic d_bytes_deallocated;
    static int64_atomic d_calls_new;
    static int64_atomic d_calls_delete;
    static size_t d_page_size;
    static size_t d_physical_memory;
    static void* d_base_frame;

    // Friends
    friend void* operator new(size_t size) throw(std::bad_alloc);
    friend void* operator new[] (size_t size) throw(std::bad_alloc);
    friend void operator delete(void* data) throw();
    friend void operator delete[] (void* data) throw();
};


/***********************************************************************
* Function to return the current memory usage                          *
***********************************************************************/
inline size_t MemoryApp::getTotalMemoryUsage( )
{
    size_t N_bytes = 0;
    #if defined(USE_LINUX)
        struct mallinfo meminfo = mallinfo();
        size_t size_hblkhd = static_cast<unsigned int>( meminfo.hblkhd );
        size_t size_uordblks = static_cast<unsigned int>( meminfo.uordblks );
        N_bytes = size_hblkhd + size_uordblks;
        // Correct for possible 32-bit wrap around
        size_t N_bytes_new = d_bytes_allocated-d_bytes_deallocated;
        while( N_bytes<N_bytes_new )
            N_bytes += 0x100000000;
    #elif defined(USE_MAC)
        struct task_basic_info t_info;
        mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
        kern_return_t rtn = task_info( mach_task_self(), 
            TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count );
        if ( rtn != KERN_SUCCESS ) { return 0; }
        N_bytes = t_info.virtual_size;
    #elif defined(USE_WINDOWS)
        PROCESS_MEMORY_COUNTERS memCounter;
        GetProcessMemoryInfo( GetCurrentProcess(), &memCounter, sizeof(memCounter) );
        N_bytes = memCounter.WorkingSetSize;
    #endif
    return N_bytes;
}


#endif


