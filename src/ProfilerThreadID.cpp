#include "ProfilerThreadID.h"


volatile TimerUtility::atomic::int32_atomic TimerUtility::ProfilerThreadIndex::N_threads = 0;
#if CXX_STD==98 || !defined(CXX_STD)
    volatile TimerUtility::atomic::int32_atomic TimerUtility::ProfilerThreadIndex::map[2*ProfilerThreadIndexHashMapSize];
    bool TimerUtility::initialize_map()
    {
        for (size_t i=0; i<2*ProfilerThreadIndexHashMapSize; i++)
            TimerUtility::ProfilerThreadIndex::map[i] = -1;
        return true;
    }
    static bool initialized = TimerUtility::initialize_map();
#endif

