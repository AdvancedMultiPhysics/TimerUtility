#include "ProfilerApp.h"
#include "matlab_helpers.h"
#include <algorithm>
#include <map>
#include <math.h>
#include <mex.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>


#define ASSERT( EXP )                                                                     \
    do {                                                                                  \
        if ( !( EXP ) ) {                                                                 \
            std::stringstream stream;                                                     \
            stream << "Failed assertion: " << #EXP << " " << __FILE__ << " " << __LINE__; \
            mexErrMsgTxt( stream.str().c_str() );                                         \
        }                                                                                 \
    } while ( 0 )


// Get a std::vector<int> for the active ids
inline std::vector<int> getActive(
    const std::map<id_struct, int>& id_map, const TraceResults& trace )
{
    std::vector<int> active( trace.N_active );
    for ( size_t i = 0; i < active.size(); i++ ) {
        auto it = id_map.find( trace.active[i] );
        ASSERT( it != id_map.end() );
        active[i] = it->second;
    }
    return active;
}


// Load variables
std::string loadString( const mxArray* ptr )
{
    if ( !ptr )
        mexErrMsgTxt( "input is a NULL pointer" );
    if ( !mxIsChar( ptr ) )
        mexErrMsgTxt( "input is not a char array" );
    char* tmp = mxArrayToString( ptr );
    std::string str( tmp );
    mxFree( tmp );
    return str;
}


// Save variables
inline mxArray* save( const std::string& str ) { return mxCreateString( str.c_str() ); }
inline mxArray* save( uint64_t x )
{
    auto mx = mxCreateNumericMatrix( 1, 1, mxUINT64_CLASS, mxREAL );
    auto* y = (uint64_t*) mxGetPr( mx );
    *y      = x;
    return mx;
}
inline mxArray* save( const std::vector<uint64_t>& x )
{
    auto mx = mxCreateNumericMatrix( 1, x.size(), mxUINT64_CLASS, mxREAL );
    auto* y = (uint64_t*) mxGetPr( mx );
    for ( size_t i = 0; i < x.size(); i++ )
        y[i] = x[i];
    return mx;
}
inline mxArray* save( const std::vector<float>& x )
{
    auto mx = mxCreateNumericMatrix( 1, x.size(), mxSINGLE_CLASS, mxREAL );
    auto* y = (float*) mxGetPr( mx );
    for ( size_t i = 0; i < x.size(); i++ )
        y[i] = x[i];
    return mx;
}
inline mxArray* save( const std::vector<std::vector<uint64_t>>& x )
{
    auto mx = mxCreateCellMatrix( 1, x.size() );
    for ( size_t i = 0; i < x.size(); i++ )
        mxSetCell( mx, i, save( x[i] ) );
    return mx;
}


/******************************************************************
 * This is the MATLAB interface                                    *
 ******************************************************************/
void mexFunction( int nlhs, mxArray* plhs[], int nrhs, const mxArray* prhs[] )
{

    //  Check that there are the proper number of arguments on each side */
    if ( nrhs != 2 || nlhs != 3 )
        mexErrMsgTxt( "Error, Incorrect number of inputs or outputs.\n"
                      "Function to load the results of a timer file.\n"
                      "Usage:\n"
                      "   [N_procs,timers,memory] = load_timer_file(file,global);\n" );

    // Load file
    auto filename = loadString( prhs[0] );

    // Load global
    bool global = mxGetScalar( prhs[1] ) != 0;

    // Load the data from the timer files
    auto data = ProfilerApp::load( filename, -1, global );
    if ( data.timers.empty() )
        mexErrMsgTxt( "No timers in file" );

    // Get the number of threads
    int N_threads = 0;
    for ( size_t i = 0; i < data.timers.size(); i++ ) {
        for ( size_t j = 0; j < data.timers[i].trace.size(); j++ )
            N_threads = std::max<int>( N_threads, data.timers[i].trace[j].thread + 1 );
    }

    // Create a map of all timer ids to int
    std::map<id_struct, int> id_map;
    for ( size_t i = 0; i < data.timers.size(); i++ ) {
        if ( id_map.find( data.timers[i].id ) == id_map.end() )
            id_map.insert( std::pair<id_struct, int>( data.timers[i].id, id_map.size() ) );
    }

    // Save N_procs
    plhs[0] = save( data.N_procs );

    // Save the timer data
    const char* fields[] = { "id", "message", "file", "path", "start", "stop", "trace" };
    plhs[1]              = mxCreateStructMatrix( data.timers.size(), 1, 7, fields );
    for ( size_t i = 0; i < data.timers.size(); i++ ) {
        const TimerResults& timer = data.timers[i];
        mxSetFieldByNumber( plhs[1], i, 0, save( id_map[timer.id] + 1 ) );
        mxSetFieldByNumber( plhs[1], i, 1, save( timer.message ) );
        mxSetFieldByNumber( plhs[1], i, 2, save( timer.file ) );
        mxSetFieldByNumber( plhs[1], i, 3, save( timer.path ) );
        mxSetFieldByNumber( plhs[1], i, 4, save( timer.start ) );
        mxSetFieldByNumber( plhs[1], i, 5, save( timer.stop ) );
        if ( timer.trace.empty() ) {
            mxSetFieldByNumber( plhs[1], i, 6, mxCreateStructMatrix( 0, 1, 0, nullptr ) );
            continue;
        }
        // Get a list of all trace ids
        std::map<std::vector<int>, int> active_map;
        for ( size_t j = 0; j < timer.trace.size(); j++ ) {
            ASSERT( timer.trace[j].id == timer.id );
            auto active = getActive( id_map, timer.trace[j] );
            active_map.insert( std::pair<std::vector<int>, int>( active, active_map.size() ) );
        }
        auto it = active_map.begin();
        for ( size_t j = 0; j < active_map.size(); ++j, ++it )
            it->second = j;
        // Allocate N, min, max, tot, start, stop
        size_t Nt = active_map.size();
        std::vector<std::vector<uint64_t>> N( Nt );
        std::vector<std::vector<float>> min( Nt ), max( Nt ), tot( Nt );
        std::vector<std::vector<std::vector<uint64_t>>> start( Nt ), stop( Nt );
        for ( size_t j = 0; j < Nt; j++ ) {
            N[j].resize( data.N_procs * N_threads, 0 );
            min[j].resize( data.N_procs * N_threads, 0 );
            max[j].resize( data.N_procs * N_threads, 0 );
            tot[j].resize( data.N_procs * N_threads, 0 );
            start[j].resize( data.N_procs * N_threads );
            stop[j].resize( data.N_procs * N_threads );
        }
        // Fill N, min, max, tot, start, stop
        for ( const auto& trace : timer.trace ) {
            auto active   = getActive( id_map, trace );
            int k         = active_map[active];
            int index     = trace.thread + trace.rank * N_threads;
            N[k][index]   = trace.N;
            min[k][index] = trace.min;
            max[k][index] = trace.max;
            tot[k][index] = trace.tot;
            if ( trace.N_trace > 0 ) {
                start[k][index].reserve( trace.N_trace );
                stop[k][index].reserve( trace.N_trace );
                uint64_t last = 0;
                for ( size_t m = 0; m < trace.N_trace; m++ ) {
                    uint64_t t1 = last + trace.times[2 * m + 0];
                    uint64_t t2 = t1 + trace.times[2 * m + 1];
                    last        = t2;
                    if ( t1 == t2 )
                        continue;
                    start[k][index].push_back( t1 );
                    stop[k][index].push_back( t2 );
                }
            }
        }
        // Create the TraceClass
        auto trace_ptr = mxCreateClassMatrix( active_map.size(), 1, "TraceClass" );
        mxSetFieldByNumber( plhs[1], i, 6, trace_ptr );
        it = active_map.begin();
        for ( size_t j = 0; j < active_map.size(); ++j, ++it ) {
            mxSetProperty( trace_ptr, j, "id", save( id_map[timer.id] + 1 ) );
            mxSetProperty( trace_ptr, j, "N", save( N[j] ) );
            mxSetProperty( trace_ptr, j, "min", save( min[j] ) );
            mxSetProperty( trace_ptr, j, "max", save( max[j] ) );
            mxSetProperty( trace_ptr, j, "tot", save( tot[j] ) );
            mxSetProperty( trace_ptr, j, "start", save( start[j] ) );
            mxSetProperty( trace_ptr, j, "stop", save( stop[j] ) );
            const auto active   = it->first;
            mxArray* active_ptr = mxCreateDoubleMatrix( 1, active.size(), mxREAL );
            double* active2     = mxGetPr( active_ptr );
            for ( size_t k = 0; k < active.size(); k++ )
                active2[k] = active[k] + 1;
            mxSetProperty( trace_ptr, j, "active", active_ptr );
        }
    }
    // Save the memory data
    const char* fields2[] = { "rank", "time", "bytes" };
    plhs[2]               = mxCreateStructMatrix( data.memory.size(), 1, 3, fields2 );
    for ( size_t i = 0; i < data.memory.size(); i++ ) {
        ASSERT( data.memory[i].time.size() == data.memory[i].bytes.size() );
        mxSetFieldByNumber( plhs[2], i, 0, save( data.memory[i].rank ) );
        mxSetFieldByNumber( plhs[2], i, 1, save( data.memory[i].time ) );
        mxSetFieldByNumber( plhs[2], i, 2, save( data.memory[i].bytes ) );
    }
}
