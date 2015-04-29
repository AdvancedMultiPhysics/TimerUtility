#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <vector>
#include <mex.h>
#include <math.h>
#include "ProfilerApp.h"


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

    // Save N_procs
    plhs[0] = mxCreateDoubleScalar(data.N_procs);

    // Save the timer data
    const char* timer_names[] = { "id", "message", "file", "path", "start", "stop", "trace" };
    plhs[1] = mxCreateStructMatrix( data.timers.size(), 1, 7, timer_names );
    for (size_t i=0; i<data.timers.size(); i++) {
        const TimerResults& timer = data.timers[i];
        const std::string timer_id = timer.id.string();
        mxSetFieldByNumber(plhs[1],i,0,mxCreateString(timer_id.c_str()));
        mxSetFieldByNumber(plhs[1],i,1,mxCreateString(timer.message.c_str()));
        mxSetFieldByNumber(plhs[1],i,2,mxCreateString(timer.file.c_str()));
        mxSetFieldByNumber(plhs[1],i,3,mxCreateString(timer.path.c_str()));
        mxSetFieldByNumber(plhs[1],i,4,mxCreateDoubleScalar(timer.start));
        mxSetFieldByNumber(plhs[1],i,5,mxCreateDoubleScalar(timer.stop));
        // Save the traces
        const char* trace_names[] = { "id", "N", "thread", "rank", "min", "max", "tot", "active", "start", "stop" };
        mxSetFieldByNumber(plhs[1],i,6,mxCreateStructMatrix(timer.trace.size(),1,10,trace_names));
        mxArray *trace_ptr = mxGetFieldByNumber(plhs[1],i,6);
        for (size_t j=0; j<timer.trace.size(); j++) {
            const TraceResults& trace = timer.trace[j];
            const std::string trace_id = timer.id.string();
            mxSetFieldByNumber(trace_ptr,j,0,mxCreateString(trace_id.c_str()));
            mxSetFieldByNumber(trace_ptr,j,1,mxCreateDoubleScalar(trace.N));
            mxSetFieldByNumber(trace_ptr,j,2,mxCreateDoubleScalar(trace.thread));
            mxSetFieldByNumber(trace_ptr,j,3,mxCreateDoubleScalar(trace.rank));
            mxSetFieldByNumber(trace_ptr,j,4,mxCreateDoubleScalar(trace.min));
            mxSetFieldByNumber(trace_ptr,j,5,mxCreateDoubleScalar(trace.max));
            mxSetFieldByNumber(trace_ptr,j,6,mxCreateDoubleScalar(trace.tot));
            mxSetFieldByNumber(trace_ptr,j,7,mxCreateCellMatrix(1,trace.N_active));
            for (size_t k=0; k<trace.N_active; k++) {
                const std::string active_id = trace.active()[k].string();
                mxSetCell(mxGetFieldByNumber(trace_ptr,j,7),k,mxCreateString(active_id.c_str()));
            }
            mxSetFieldByNumber(trace_ptr,j,8,mxCreateDoubleMatrix(1,trace.N_trace,mxREAL));
            mxSetFieldByNumber(trace_ptr,j,9,mxCreateDoubleMatrix(1,trace.N_trace,mxREAL));
            if ( trace.N_trace > 0 ) {
                const double *start = trace.start();
                const double *stop = trace.stop();
                double *start_out = mxGetPr(mxGetFieldByNumber(trace_ptr,j,8));
                double *stop_out = mxGetPr(mxGetFieldByNumber(trace_ptr,j,9));
                memcpy( start_out, start, trace.N_trace*sizeof(double) );
                memcpy( stop_out,  stop,  trace.N_trace*sizeof(double) );
            }
        }
    }

    // Save the memory data
    const char* memory_names[] = { "rank", "time", "bytes" };
    plhs[2] = mxCreateStructMatrix( data.memory.size(), 1, 3, memory_names );
    for (size_t i=0; i<data.memory.size(); i++) {
        ASSERT(data.memory[i].time.size()==data.memory[i].bytes.size());
        size_t N = data.memory[i].time.size();
        mxSetFieldByNumber(plhs[2],i,0,mxCreateDoubleScalar(data.memory[i].rank));
        mxSetFieldByNumber(plhs[2],i,1,mxCreateDoubleMatrix(1,N,mxREAL));
        ASSERT(mxGetNumberOfElements(mxGetFieldByNumber(plhs[2],i,1))==N);
        mxSetFieldByNumber(plhs[2],i,2,mxCreateDoubleMatrix(1,N,mxREAL));
        ASSERT(mxGetNumberOfElements(mxGetFieldByNumber(plhs[2],i,2))==N);
        double *time = mxGetPr(mxGetFieldByNumber(plhs[2],i,1));
        double *size = mxGetPr(mxGetFieldByNumber(plhs[2],i,2));
        for (size_t j=0; j<N; j++) {
            time[j] = static_cast<double>(data.memory[i].time[j]);
            size[j] = static_cast<double>(data.memory[i].bytes[j]);
        }
    }
    
    // Clear data
    data.timers.clear();
    data.memory.clear();
}


