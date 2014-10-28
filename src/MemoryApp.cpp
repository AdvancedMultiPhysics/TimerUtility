#include "MemoryApp.h"
#include <malloc.h>


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    // Using windows
    #define USE_WINDOWS
    #define NOMINMAX
    #include <stdlib.h>
    #include <windows.h>
    #include <process.h>
#elif defined(__APPLE__)
    // Using MAC
    #define USE_MAC
    #include <libkern/OSAtomic.h>
#elif defined(__linux) || defined(__unix) || defined(__posix)
    // Using Linux
    #define USE_LINUX
    #include <unistd.h>
#else
    #error Unknown OS
#endif



/***********************************************************************
* Atomic operations                                                    *
* atomic_add(X,Y) - Add Y to X  (X is a ptr)                           *
* atomic_increment(X) - Increment X  (X is a ptr)                      *
***********************************************************************/
#if defined(USE_WINDOWS)
    typedef __int64 int64_atomic;
    #define atomic_add( X, Y ) \
        InterlockedExchangeAdd64(X,Y)
    #define atomic_increment(X) \
        InterlockedIncrement64(X)
#elif defined(USE_MAC)
    typedef int64_t int64_atomic;
    #define atomic_add( X, Y ) \
        OSAtomicAdd64Barrier(X,Y)
    #define atomic_increment(X) \
        OSAtomicIncrement64Barrier(X)
#elif defined(USE_LINUX)
    typedef long int int64_atomic;
    #define atomic_add( X, Y ) \
        __sync_add_and_fetch(X,Y)
    #define atomic_increment(X) \
        __sync_add_and_fetch(X,1)
#else
    #error Unknown OS
#endif


/***********************************************************************
* Initialize variables                                                 *
***********************************************************************/
int64_atomic MemoryApp::bytes_allocated = 0;
int64_atomic MemoryApp::bytes_deallocated = 0;
int64_atomic MemoryApp::calls_new = 0;
int64_atomic MemoryApp::calls_delete = 0;
#if defined(USE_MAC) || defined(USE_LINUX)
    size_t MemoryApp::page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
    size_t MemoryApp::page_size = 0;
#endif


/***********************************************************************
* Functions to overload new/delete                                     *
***********************************************************************/
#ifdef USE_WINDOWS
    #define get_malloc_size(X) _msize(X)
#elif defined(USE_LINUX)
    #define get_malloc_size(X) malloc_usable_size(X)
#elif defined(USE_MAC)
    #define get_malloc_size(X) malloc_size(X)
#else
    #error Unknown OS
#endif
#ifndef DISABLE_NEW_OVERLOAD
    void* operator new(size_t size) throw(std::bad_alloc) 
    {
        void* ret = malloc(size);
        if (!ret) throw std::bad_alloc();
        size_t block_size = get_malloc_size(ret);
        atomic_add(&MemoryApp::bytes_allocated,block_size);
        atomic_increment(&MemoryApp::calls_new);
        return ret;
    }
    void* operator new[] (size_t size) throw(std::bad_alloc) 
    {
        void* ret = malloc(size);
        if (!ret) throw std::bad_alloc();
        size_t block_size = get_malloc_size(ret);
        atomic_add(&MemoryApp::bytes_allocated,block_size);
        atomic_increment(&MemoryApp::calls_new);
        return ret;
    }
    void operator delete(void* data) throw()
    {
        size_t block_size = get_malloc_size(data);
        free(data);
        atomic_add(&MemoryApp::bytes_deallocated,block_size);
        atomic_increment(&MemoryApp::calls_delete);
    }
    void operator delete[] (void* data) throw()
    {
        size_t block_size = get_malloc_size(data);
        free(data);
        atomic_add(&MemoryApp::bytes_deallocated,block_size);
        atomic_increment(&MemoryApp::calls_delete);
    }
#endif


/***********************************************************************
* Class functions                                                      *
***********************************************************************/
void MemoryApp::print( std::ostream& os )
{
    size_t t1 = MemoryApp::bytes_allocated;
    size_t t2 = MemoryApp::bytes_deallocated;
    size_t t3 = MemoryApp::calls_new;
    size_t t4 = MemoryApp::calls_delete;
    os << "Statistics from new/delete:\n";
    os << "   Bytes allocated: " << t1 << std::endl;
    os << "   Bytes deallocated: " << t2 << std::endl;
    os << "   Bytes in use: " << t1-t2 << std::endl;
    os << "   Number of calls to new: " << t3 << std::endl;
    os << "   Number of calls to delete: " << t4 << std::endl;
    os << "   Number of calls to new without delete: " << t3-t4 << std::endl;
    os << "   Total memory in use: " << MemoryApp::getTotalMemoryUsage() << std::endl;
}
size_t MemoryApp::getSystemMemory()
{
    size_t N_bytes = 0;
    #if defined(USE_LINUX)
        static long pages = sysconf(_SC_PHYS_PAGES);
        N_bytes = pages * page_size;
    #elif defined(USE_MAC)
        int mib[2] = { CTL_HW, HW_MEMSIZE };
        u_int namelen = sizeof(mib) / sizeof(mib[0]);
        uint64_t size;
        size_t len = sizeof(size);
        if (sysctl(mib, namelen, &size, &len, NULL, 0) == 0)
            N_bytes = size;
    #elif defined(USE_WINDOWS)
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        N_bytes = status.ullTotalPhys;
    #endif
    return N_bytes;
}


