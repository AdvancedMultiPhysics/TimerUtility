#ifndef GuiTimerStructures_H
#define GuiTimerStructures_H

#include "ProfilerApp.h"

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



// Simple matrix class
template<class TYPE>
class Matrix {
public:
    typedef std::shared_ptr<Matrix<TYPE>> shared_ptr;
    int N;
    int M;
    TYPE *data;
    Matrix(): N(0), M(0), data(NULL) {}
    Matrix(int N_, int M_): N(N_), M(M_), data(new TYPE[N_*M_]) { fill(0); }
    ~Matrix() { delete [] data; }
    TYPE& operator()(int i, int j) { return data[i+j*N]; }
    void fill( TYPE x ) { 
        for (int i=0; i<N*M; i++)
            data[i] = x;
    }
    TYPE min() const { 
        TYPE x = data[0];
        for (int i=0; i<N*M; i++)
            x = std::min(x,data[i]);
        return x;
    }
    TYPE max() const { 
        TYPE x = data[0];
        for (int i=0; i<N*M; i++)
            x = std::max(x,data[i]);
        return x;
    }
    TYPE sum() const { 
        TYPE x = 0;
        for (int i=0; i<N*M; i++)
            x += data[i];
        return x;
    }
    double mean() const { 
        TYPE x = sum();
        return static_cast<double>(x)/(N*M);
    }
protected:
    Matrix(const Matrix&);            // Private copy constructor
    Matrix& operator=(const Matrix&); // Private assignment operator
};


#endif

