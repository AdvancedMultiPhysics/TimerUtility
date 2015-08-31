#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <mex.h>
#include <math.h>
#include <algorithm>
#include "ProfilerApp.h"
#include "matlab_helpers.h"


inline mxArray* mxCreateDoubleScalar( double val )
{
    mxArray *arr = mxCreateDoubleMatrix(1,1,mxREAL);
    double *tmp = mxGetPr(arr);
    tmp[0] = val;
    return arr;
}


#define ASSERT(EXP) do {                                    \
    if ( !(EXP) ) {                                         \
        std::stringstream stream;                           \
        stream << "Failed assertion: " << #EXP              \
            << " " << __FILE__ << " " << __LINE__;          \
        mexErrMsgTxt(stream.str().c_str());                 \
    }                                                       \
}while(0)


// Get a std::vector<int> for the active ids
inline std::vector<int> getActive( const std::map<id_struct,int>& id_map, const TraceResults& trace )
{
    std::vector<int> active(trace.N_active);
    for (size_t i=0; i<active.size(); i++) {
        std::map<id_struct,int>::const_iterator it = id_map.find(trace.active()[i]);
        ASSERT(it!=id_map.end());
        active[i] = it->second;
    }
    return active;
}


// Get a ptr from std::vector
template<class TYPE>
inline TYPE* getPtr( std::vector<TYPE>& x )
{
    return x.empty() ? NULL:&x[0];
}
template<class TYPE>
inline const TYPE* getPtr( const std::vector<TYPE>& x )
{
    return x.empty() ? NULL:&x[0];
}


// Save a single array
template<class TYPE>
inline mxArray* saveSingleMatrix( size_t N, size_t M, const TYPE *x )
{
    mxArray* ptr = mxCreateNumericMatrix(N,M,mxSINGLE_CLASS,mxREAL);
    float *y = (float*) mxGetData(ptr);
    for (size_t i=0; i<N*M; i++)
        y[i] = static_cast<float>(x[i]);
    return ptr;
}


/******************************************************************
* This is the MATLAB interface                                    *
******************************************************************/
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{

    //  Check that there are the proper number of arguments on each side */
    if ( nrhs!=2 || nlhs!=3 )
        mexErrMsgTxt("Error, Incorrect number of inputs or outputs.\n"
            "Function to load the results of a timer file.\n"
            "Usage:\n"
            "   [N_procs,timers,memory] = load_timer_file(file,global);\n");

    // Load file
    if (mxGetM(prhs[0])!=1)
        mexErrMsgTxt("Input must be a 1xn string");
    char *input_buf = mxArrayToString(prhs[0]);
    std::string filename(input_buf);
    mxFree(input_buf);

    // Load global
    bool global = mxGetScalar(prhs[1])!=0;

    // Load the data from the timer files
    TimerMemoryResults data = ProfilerApp::load( filename, -1, global );
    if ( data.timers.empty() )
        mexErrMsgTxt("No timers in file");

    // Get the number of threads
    int N_threads = 0;
    for (size_t i=0; i<data.timers.size(); i++) {
        for (size_t j=0; j<data.timers[i].trace.size(); j++)
            N_threads = std::max<int>(N_threads,data.timers[i].trace[j].thread+1);
    }

    // Create a map of all timer ids to int
    std::map<id_struct,int> id_map;
    for (size_t i=0; i<data.timers.size(); i++) {
        if ( id_map.find(data.timers[i].id)==id_map.end() )
            id_map.insert(std::pair<id_struct,int>(data.timers[i].id,id_map.size()));
    }

    // Save N_procs
    plhs[0] = mxCreateDoubleScalar(data.N_procs);

    // Save the timer data
    const char* timer_names[] = { "id", "message", "file", "path", "start", "stop", "trace" };
    plhs[1] = mxCreateStructMatrix( data.timers.size(), 1, 7, timer_names );
    for (size_t i=0; i<data.timers.size(); i++) {
        const TimerResults& timer = data.timers[i];
        mxSetFieldByNumber(plhs[1],i,0,mxCreateDoubleScalar(id_map[timer.id]+1));
        mxSetFieldByNumber(plhs[1],i,1,mxCreateString(timer.message.c_str()));
        mxSetFieldByNumber(plhs[1],i,2,mxCreateString(timer.file.c_str()));
        mxSetFieldByNumber(plhs[1],i,3,mxCreateString(timer.path.c_str()));
        mxSetFieldByNumber(plhs[1],i,4,mxCreateDoubleScalar(timer.start));
        mxSetFieldByNumber(plhs[1],i,5,mxCreateDoubleScalar(timer.stop));
        if ( timer.trace.empty() ) {
            mxSetFieldByNumber(plhs[1],i,6,mxCreateStructMatrix(0,1,0,NULL));
            continue;
        }
        // Get a list of all trace ids
        std::map<std::vector<int>,int> active_map;
        for (size_t j=0; j<timer.trace.size(); j++) {
            ASSERT(timer.trace[j].id==timer.id);
            std::vector<int> active = getActive(id_map,timer.trace[j]);
            active_map.insert(std::pair<std::vector<int>,int>(active,active_map.size()));
        }
        std::map<std::vector<int>,int>::iterator it=active_map.begin();
        for (size_t j=0; j<active_map.size(); ++j, ++it)
            it->second = j;
        // Allocate N, min, max, tot, start, stop
        std::vector<mxArray*> N(active_map.size()), min(active_map.size()), max(active_map.size()),
            tot(active_map.size()), start(active_map.size()), stop(active_map.size());
        for (size_t j=0; j<active_map.size(); ++j) {
            N[j] = mxCreateDoubleMatrix(N_threads,data.N_procs,mxREAL);
            min[j] = mxCreateDoubleMatrix(N_threads,data.N_procs,mxREAL);
            max[j] = mxCreateDoubleMatrix(N_threads,data.N_procs,mxREAL);
            tot[j] = mxCreateDoubleMatrix(N_threads,data.N_procs,mxREAL);
            start[j] = mxCreateCellMatrix(N_threads,data.N_procs);
            stop[j]  = mxCreateCellMatrix(N_threads,data.N_procs);
        }
        // Fill N, min, max, tot, start, stop
        for (size_t j=0; j<timer.trace.size(); j++) {
            const TraceResults& trace = timer.trace[j];
            std::vector<int> active = getActive(id_map,timer.trace[j]);
            int k = active_map[active];
            int index = trace.thread + trace.rank*N_threads;
            mxGetPr(N[k])[index] = trace.N;
            mxGetPr(min[k])[index] = trace.min;
            mxGetPr(max[k])[index] = trace.max;
            mxGetPr(tot[k])[index] = trace.tot;
            if ( trace.N_trace > 0 ) {
                mxSetCell(start[k],index,saveSingleMatrix(1,trace.N_trace,trace.start()));
                mxSetCell(stop[k], index,saveSingleMatrix(1,trace.N_trace,trace.stop()));
            }
        }
        // Create the TraceClass
        mxArray *trace_ptr = mxCreateClassMatrix(active_map.size(),1,"TraceClass");
        mxSetFieldByNumber(plhs[1],i,6,trace_ptr);
        it=active_map.begin();
        for (size_t j=0; j<active_map.size(); ++j, ++it) {
            mxSetProperty(trace_ptr,j,"id",mxCreateDoubleScalar(id_map[timer.id]+1));
            mxSetProperty(trace_ptr,j,"N",N[j]);
            mxSetProperty(trace_ptr,j,"min",min[j]);
            mxSetProperty(trace_ptr,j,"max",max[j]);
            mxSetProperty(trace_ptr,j,"tot",tot[j]);
            mxSetProperty(trace_ptr,j,"start",start[j]);
            mxSetProperty(trace_ptr,j,"stop",stop[j]);
            const std::vector<int>& active = it->first;
            mxArray* active_ptr = mxCreateDoubleMatrix(1,active.size(),mxREAL);
            double *active2 = mxGetPr(active_ptr);
            for (size_t k=0; k<active.size(); k++)
                active2[k] = active[k]+1;
            mxSetProperty(trace_ptr,j,"active",active_ptr);
        }
    }

    // Save the memory data
    const char* memory_names[] = { "rank", "time", "bytes" };
    plhs[2] = mxCreateStructMatrix( data.memory.size(), 1, 3, memory_names );
    for (size_t i=0; i<data.memory.size(); i++) {
        ASSERT(data.memory[i].time.size()==data.memory[i].bytes.size());
        size_t N = data.memory[i].time.size();
        mxSetFieldByNumber(plhs[2],i,0,mxCreateDoubleScalar(data.memory[i].rank));
        mxSetFieldByNumber(plhs[2],i,1,saveSingleMatrix(1,N,getPtr(data.memory[i].time)));
        mxSetFieldByNumber(plhs[2],i,2,saveSingleMatrix(1,N,getPtr(data.memory[i].bytes)));
    }
    
    // Clear data
    data.timers.clear();
    data.memory.clear();
}


