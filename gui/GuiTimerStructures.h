#ifndef GuiTimerStructures_H
#define GuiTimerStructures_H

#include "Array.h"
#include "ProfilerApp.h"

#include <set>
#include <vector>


// Struct to hold the summary info for a trace
struct TraceSummary {
    id_struct id;           //!<  Timer ID
    uint64_t stack;         //!<  Calling stack
    uint64_t stack2;        //!<  Stack including this trace
    std::set<int> threads;  //!<  Threads that are active for this timer
    std::vector<int> N;     //!<  Number of calls
    std::vector<float> min; //!<  Minimum time
    std::vector<float> max; //!<  Maximum time
    std::vector<float> tot; //!<  Total time
    TraceSummary() {}
    ~TraceSummary() {}
};


// Struct to hold the summary info for a timer
struct TimerSummary {
    id_struct id;                           //!<  Timer ID
    std::string message;                    //!<  Timer message
    std::string file;                       //!<  Timer file
    int line;                               //!<  Timer start line
    std::vector<int> threads;               //!<  Threads that are active for this timer
    std::vector<int> N;                     //!<  Number of calls
    std::vector<float> min;                 //!<  Minimum time
    std::vector<float> max;                 //!<  Maximum time
    std::vector<float> tot;                 //!<  Total time
    std::vector<const TraceSummary*> trace; //!< List of all active traces for the timer
    TimerSummary() : line( -1 ) {}
    ~TimerSummary() {}
};


// Structure to hold a timeline for a timer
struct TimerTimeline {
    id_struct id;        //!<  Timer ID
    std::string message; //!<  Timer message
    std::string file;    //!<  Timer file
    double tot;          //!<  Total time spent in timer (all threads/ranks)
    BoolArray active;    //!<  Active image
    TimerTimeline() : tot( 0 ) {}
    ~TimerTimeline() {}
};


#endif
