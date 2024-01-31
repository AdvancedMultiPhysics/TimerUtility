#include "ProfilerApp.h"

#include <algorithm>
#include <cstdarg>
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


// stringf
inline std::string stringf( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    char tmp[1024];
    vsprintf( tmp, format, ap );
    va_end( ap );
    return std::string( tmp );
}


// Load string from MATLAB
inline std::string loadMexString( const mxArray* ptr )
{
    if ( ptr == nullptr )
        return std::string();
    if ( !mxIsChar( ptr ) )
        return std::string();
    char* tmp = mxArrayToString( ptr );
    std::string str( tmp );
    mxFree( tmp );
    return str;
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
inline mxArray* save( const std::string& str ) { return mxCreateString( str.data() ); }
inline mxArray* save( const id_struct& id ) { return mxCreateString( id.str().data() ); }
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


/************************************************************************
 * Functions to create a MATLAB class object                             *
 ************************************************************************/
mxArray* mxCreateClassArray( mwSize ndim, const mwSize* dims, const char* classname )
{
    PROFILE( "mxCreateClassArray", 1 );
    // Create the arguments to feval
    size_t N = 1;
    for ( size_t i = 0; i < ndim; i++ )
        N *= dims[i];
    std::string arg;
    if ( N > 1 ) {
        arg = stringf( "x(%i", static_cast<int>( dims[0] ) );
        for ( size_t i = 1; i < ndim; i++ )
            arg += stringf( ",%i", static_cast<int>( dims[i] ) );
        arg += ") = " + std::string( classname ) + ";";
    } else {
        arg = "x = " + std::string( classname ) + ";";
    }
    mxArray* rhs = mxCreateString( arg.c_str() );
    // Call MATLAB
    mxArray* mx     = nullptr;
    mxArray* mxtrap = mexCallMATLABWithTrap( 1, &mx, 1, &rhs, "mxCreateClassArrayHelper" );
    mxDestroyArray( rhs );
    // Check for errors and return
    if ( mxtrap ) {
        std::string msg = stringf( "Error creating class %s (1): %s\n", classname, arg.c_str() );
        msg += "  identifier: " + loadMexString( mxGetProperty( mxtrap, 0, "identifier" ) ) + "\n";
        msg += "  message: " + loadMexString( mxGetProperty( mxtrap, 0, "message" ) ) + "\n";
        mxDestroyArray( mxtrap );
        mx = nullptr;
        mexErrMsgTxt( msg.c_str() );
    }
    const std::string className( mxGetClassName( mx ) );
    if ( className != classname )
        mexErrMsgTxt( stringf( "Error creating class %s (2)", classname ).c_str() );
    return mx;
}
mxArray* mxCreateClassMatrix( mwSize n, mwSize m, const char* classname )
{
    mwSize dims[2] = { n, m };
    return mxCreateClassArray( 2, dims, classname );
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

    // Save N_procs
    plhs[0] = save( data.N_procs );

    // Save the timer data
    const char* fields[] = { "id", "message", "file", "path", "line", "trace" };
    plhs[1]              = mxCreateStructMatrix( data.timers.size(), 1, 6, fields );
    for ( size_t i = 0; i < data.timers.size(); i++ ) {
        auto& timer = data.timers[i];
        mxSetFieldByNumber( plhs[1], i, 0, save( timer.id ) );
        mxSetFieldByNumber( plhs[1], i, 1, save( timer.message ) );
        mxSetFieldByNumber( plhs[1], i, 2, save( timer.file ) );
        mxSetFieldByNumber( plhs[1], i, 3, save( timer.path ) );
        mxSetFieldByNumber( plhs[1], i, 4, save( timer.line ) );
        if ( timer.trace.empty() ) {
            mxSetFieldByNumber( plhs[1], i, 5, mxCreateStructMatrix( 0, 1, 0, nullptr ) );
            continue;
        }
        // Create the TraceClass
        auto trace_ptr = mxCreateClassMatrix( timer.trace.size(), 1, "TraceClass" );
        mxSetFieldByNumber( plhs[1], i, 6, trace_ptr );
        for ( size_t j = 0; j < timer.trace.size(); ++j ) {
            auto& trace    = timer.trace[i];
            size_t N_trace = trace.N_trace;
            std::vector<uint64_t> start( N_trace, 0 ), stop( N_trace, 0 );
            for ( size_t k = 0; k < N_trace; k++ ) {
                start[k] = static_cast<uint64_t>( trace.times[2 * k + 0] );
                stop[k]  = static_cast<uint64_t>( trace.times[2 * k + 1] );
            }
            mxSetProperty( trace_ptr, j, "id", save( timer.id ) );
            mxSetProperty( trace_ptr, j, "N", save( trace.N ) );
            mxSetProperty( trace_ptr, j, "min", save( trace.min ) );
            mxSetProperty( trace_ptr, j, "max", save( trace.max ) );
            mxSetProperty( trace_ptr, j, "tot", save( trace.tot ) );
            mxSetProperty( trace_ptr, j, "stack", save( trace.stack ) );
            mxSetProperty( trace_ptr, j, "stack2", save( trace.stack2 ) );
            mxSetProperty( trace_ptr, j, "start", save( start ) );
            mxSetProperty( trace_ptr, j, "stop", save( stop ) );
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
