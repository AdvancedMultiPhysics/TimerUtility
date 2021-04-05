#ifndef included_MemoryApp
#define included_MemoryApp


#include "ProfilerDefinitions.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


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
        MemoryStats() { memset( this, 0, sizeof( MemoryStats ) ); }
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
    static size_t getTotalMemoryUsage() noexcept;

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
    static volatile std::atomic_int64_t d_bytes_allocated;
    static volatile std::atomic_int64_t d_bytes_deallocated;
    static volatile std::atomic_int64_t d_calls_new;
    static volatile std::atomic_int64_t d_calls_delete;
    static size_t d_page_size;
    static size_t d_physical_memory;
    static void* d_base_frame;

#ifndef TIMER_DISABLE_NEW_OVERLOAD
    // Overload new/delete are friends
    friend void* operator new( std::size_t size );
    friend void* operator new[]( std::size_t size );
    friend void* operator new( std::size_t size, const std::nothrow_t& ) noexcept;
    friend void* operator new[]( std::size_t size, const std::nothrow_t& ) noexcept;
    friend void operator delete( void* data ) noexcept;
    friend void operator delete[]( void* data ) noexcept;
    friend void operator delete(void* data, const std::nothrow_t&) noexcept;
    friend void operator delete[]( void* data, const std::nothrow_t& ) noexcept;
    friend void operator delete( void* data, std::size_t ) noexcept;
    friend void operator delete[]( void* data, std::size_t ) noexcept;
#endif
};


#endif
