#include "ProfilerThreadID.h"


volatile TimerUtility::atomic::int32_atomic TimerUtility::ProfilerThreadIndex::N_threads = 0;
#ifndef TIMER_ENABLE_THREAD_LOCAL
volatile TimerUtility::atomic::int32_atomic
    TimerUtility::ProfilerThreadIndex::map[2 * ProfilerThreadIndex::HashMapSize];
bool TimerUtility::ProfilerThreadIndex::initialize_map()
{
    for ( size_t i = 0; i < 2 * HashMapSize; i++ )
        map[i] = -1;
    return true;
}
static bool initialized = TimerUtility::ProfilerThreadIndex::initialize_map();
#else
bool TimerUtility::ProfilerThreadIndex::initialize_map() { return true; }
#endif
