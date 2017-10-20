// Helper class to get a thread index for the current thread
// Note: this class is thread-safe
#ifndef INCLUDED_ProfilerThreadIndex_H
#define INCLUDED_ProfilerThreadIndex_H

#include "ProfilerAtomicHelpers.h"
#include "ProfilerDefinitions.h"

#include <thread>


namespace TimerUtility {


// Class definition
class ProfilerThreadIndex final
{
public:
    static inline int getThreadIndex();
    static inline int getNThreads() { return N_threads; }
    static bool initialize_map();

protected:
    static volatile atomic::int32_atomic N_threads;
#ifndef TIMER_ENABLE_THREAD_LOCAL
    static const size_t HashMapSize = 1024;
    static volatile atomic::int32_atomic map[2 * HashMapSize];
#endif

private:
    ProfilerThreadIndex();
};


// Get the id for the current thread
inline int ProfilerThreadIndex::getThreadIndex()
{
#ifdef TIMER_ENABLE_THREAD_LOCAL
    thread_local static int id = atomic::atomic_increment( &N_threads ) - 1;
    return id;
#else
    // Get a uint64_t id for the thread
    std::thread::id id = std::this_thread::get_id();
    static std::hash<std::thread::id> hasher;
    int32_t hash = static_cast<int32_t>( hasher( id ) );
    // Lookup the value in the hash table, adding if necessary
    int i = hash % HashMapSize;
    while ( map[2 * i] != hash ) {
        if ( map[2 * i] == -1 ) {
            if ( atomic::atomic_compare_and_swap( &map[2 * i], -1, hash ) ) {
                int id         = atomic::atomic_increment( &N_threads ) - 1;
                map[2 * i + 1] = id;
            }
        } else {
            i = ( i + 1 ) % HashMapSize;
        }
    }
    return map[2 * i + 1];
#endif
}


} // namespace TimerUtility

#endif
