#ifndef GuiTimerStructures_H
#define GuiTimerStructures_H

#include "ProfilerApp.h"
#include "Array.h"

#include <vector>
#include <set>


// Struct to hold the summary info for a trace
struct TraceSummary {
    id_struct id;                       //!<  Timer ID
    std::vector<id_struct> active;      //!<  Ids that are active for this trace
    std::set<int> threads;              //!<  Threads that are active for this timer
    std::vector<int> N;                 //!<  Number of calls
    std::vector<float> min;             //!<  Minimum time
    std::vector<float> max;             //!<  Maximum time
    std::vector<float> tot;             //!<  Total time
    TraceSummary() {}
    ~TraceSummary() {}
};


// Struct to hold the summary info for a timer
struct TimerSummary {
    id_struct id;                       //!<  Timer ID
    std::string message;                //!<  Timer message
    std::string file;                   //!<  Timer file
    int start;                          //!<  Timer start line (-1: never defined)
    int stop;                           //!<  Timer stop line (-1: never defined)
    std::vector<int> threads;           //!<  Threads that are active for this timer
    std::vector<int> N;                 //!<  Number of calls
    std::vector<float> min;             //!<  Minimum time
    std::vector<float> max;             //!<  Maximum time
    std::vector<float> tot;             //!<  Total time
    std::vector<const TraceSummary*> trace; //!< List of all active traces for the timer
    TimerSummary(): start(-1), stop(-1) {}
    ~TimerSummary() {}
};


// Structure to hold a timeline for a timer
struct TimerTimeline {
    id_struct id;                       //!<  Timer ID
    std::string message;                //!<  Timer message
    std::string file;                   //!<  Timer file
    double tot;                         //!<  Total time spent in timer (all threads/ranks)
    BoolArray active;                   //!<  Active image
    TimerTimeline() {}
    ~TimerTimeline() {}
};



#endif

