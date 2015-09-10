// Helper class to get a thread index for the current thread
// Note: this class is thread-safe
#ifndef INCLUDED_ProfilerThreadIndex_H
#define INCLUDED_ProfilerThreadIndex_H

#include "ProfilerAtomicHelpers.h"
#include "ProfilerDefinitions.h"

#include <stdint.h>


#ifndef ENABLE_THREAD_LOCAL
    #if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
        #define USE_WINDOWS
        #include <windows.h>
    #elif defined(__APPLE__)
        #define USE_MAC
        #include <pthread.h>
    #else
        #define USE_LINUX
        #include <pthread.h>
    #endif
    #define ProfilerThreadIndexHashMapSize 1024
#endif


namespace TimerUtility {

bool initialize_map();


// Class definition
class ProfilerThreadIndex
{
public:
    static inline int getThreadIndex();
    static inline int getNThreads() { return N_threads; }
protected:
    static volatile atomic::int32_atomic N_threads;
    #ifndef ENABLE_THREAD_LOCAL
        static volatile atomic::int32_atomic map[2*ProfilerThreadIndexHashMapSize];
        friend bool initialize_map();
    #endif

private:
    ProfilerThreadIndex();
};



// Get the id for the current thread
inline int ProfilerThreadIndex::getThreadIndex()
{
    #ifdef ENABLE_THREAD_LOCAL
        thread_local static int id = atomic::atomic_increment(&N_threads)-1;
        return id;
    #else
        // Get a uint64_t id for the thread
        #ifdef USE_WINDOWS
            DWORD tmp_thread_id = GetCurrentThreadId();
            uint64_t id = (uint64_t) tmp_thread_id;
        #elif defined(USE_LINUX) || defined(USE_MAC)
            pthread_t tmp_thread_id = pthread_self();
            uint64_t id = (uint64_t) tmp_thread_id;
        #else
            #error Unknown OS
        #endif
        // Create a hash key
        int hash = static_cast<int>( ((id*0x9E3779B97F4A7C15)>>48)%ProfilerThreadIndexHashMapSize );
        // Lookup the value in the hash table, adding if necessary
        int i = hash;
        while ( map[2*i]!=hash ) {
            if ( map[2*i]==-1 ) {
                atomic::int32_atomic result = atomic::atomic_compare_and_swap(&map[2*i],-1,hash);
                if ( result == -1 ) {
                    int id = atomic::atomic_increment(&N_threads)-1;
                    map[2*i+1] = id;
                }
            } else {
                i = (i+1)%ProfilerThreadIndexHashMapSize;
            }
        }
        return map[2*i+1];
    #endif
}


} // TimerUtility namespace

#endif

