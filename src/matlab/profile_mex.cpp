#include "ProfilerApp.h"
#include <mex.h>
#include <stdlib.h>
#include <string>


void mexFunction( int nlhs, mxArray *[], int nrhs, const mxArray *prhs[] )
{

    const char *msg =
        "Error, Incorrect number of inputs or outputs.\n"
        "profile_mex(args);\n"
        "This function provides an interface to access the global profiler.\n"
        "The possible commands are:\n"
        "   profile_mex('ENABLE',level);\n"
        "      Enable the profiler with the given level (default level is 0)\n"
        "   profile_mex('DISABLE');\n"
        "      Disable the profiler\n"
        "   profile_mex('ENABLE_TRACE',flag);\n"
        "      Enable/disable the trace data for the profiler (true: enable, false: disable)\n"
        "   profile_mex('ENABLE_MEMORY',flag);\n"
        "      Enable/disable the memory trace data for the profiler (true: enable, false: "
        "disable)\n"
        "   profile_mex('SAVE',filename);\n"
        "      Save the current results of the profiler to the given file\n"
        "   profile_mex('START',name);\n"
        "      Start a timer for the given name (file and line are automatically gathered, "
        "level=0)\n"
        "   profile_mex('START',name,level);\n"
        "      Start a timer for the given name and level (file and line are automatically "
        "gathered)\n"
        "   profile_mex('START',name,file,line,level);\n"
        "      Start a timer for the given name, file, line number, and level\n"
        "   profile_mex('STOP',name);\n"
        "      Stop a timer for the given name (file and line are automatically gathered, "
        "level=0)\n"
        "   profile_mex('STOP',name,level);\n"
        "      Stop a timer for the given name and level (file and line are automatically "
        "gathered)\n"
        "   profile_mex('STOP',name,file,line,level);\n"
        "      Stop a timer for the given name, file, line number, and level\n";

    //  Check that there are the proper number of arguments on each side
    if ( nrhs == 0 || nlhs != 0 )
        mexErrMsgTxt( msg );

    // Load the command
    if ( mxGetClassID( prhs[0] ) != mxCHAR_CLASS )
        mexErrMsgTxt( "command must be a string" );
    char *input_buf = mxArrayToString( prhs[0] );
    std::string command( input_buf );
    mxFree( input_buf );

    // Apply the given command
    if ( command == "ENABLE" || command == "DISABLE" ) {
        // Enable/disable the timers
        if ( nrhs < 1 || nlhs > 2 )
            mexErrMsgTxt( msg );
        int level = 0;
        if ( nrhs == 2 ) {
            if ( mxGetNumberOfElements( prhs[1] ) != 1 )
                mexErrMsgTxt( "level must be size 1" );
            level = static_cast<int>( mxGetScalar( prhs[1] ) );
            if ( level < 0 )
                mexErrMsgTxt( "Invalid value for level" );
        }
        if ( command == "ENABLE" )
            PROFILE_ENABLE( level );
        else
            PROFILE_DISABLE();
    } else if ( command == "ENABLE_TRACE" ) {
        // Enable/disable the trace data
        if ( nrhs != 2 )
            mexErrMsgTxt( msg );
        if ( mxGetNumberOfElements( prhs[1] ) != 1 )
            mexErrMsgTxt( "flag must be size 1" );
        if ( mxGetClassID( prhs[1] ) != mxLOGICAL_CLASS )
            mexErrMsgTxt( "flag must be a logical type" );
        bool flag = mxGetScalar( prhs[1] ) != 0;
        if ( flag )
            PROFILE_ENABLE_TRACE();
        else
            PROFILE_DISABLE_TRACE();
    } else if ( command == "ENABLE_MEMORY" ) {
        // Enable/disable the memory trace data
        if ( nrhs != 2 )
            mexErrMsgTxt( msg );
        if ( mxGetNumberOfElements( prhs[1] ) != 1 )
            mexErrMsgTxt( "flag must be size 1" );
        if ( mxGetClassID( prhs[1] ) != mxLOGICAL_CLASS )
            mexErrMsgTxt( "flag must be a logical type" );
        bool flag = mxGetScalar( prhs[1] ) != 0;
        if ( flag )
            PROFILE_ENABLE_MEMORY();
        else
            PROFILE_DISABLE_MEMORY();
    } else if ( command == "SAVE" ) {
        // Save the trace data
        if ( nrhs != 2 )
            mexErrMsgTxt( msg );
        if ( mxGetClassID( prhs[1] ) != mxCHAR_CLASS )
            mexErrMsgTxt( "command must be a string" );
        char *filename_buf = mxArrayToString( prhs[1] );
        std::string filename( filename_buf );
        mxFree( filename_buf );
        PROFILE_SAVE( filename );
    } else if ( command == "START" || command == "STOP" ) {
        // Start/stop a timer
        if ( nrhs != 2 && nrhs != 3 && nrhs != 5 )
            mexErrMsgTxt( msg );
        // Get the timer name
        if ( mxGetClassID( prhs[1] ) != mxCHAR_CLASS )
            mexErrMsgTxt( "name must be a string" );
        char *name_buf = mxArrayToString( prhs[1] );
        std::string name( name_buf );
        mxFree( name_buf );
        // Get the timer file, line, and level
        std::string file;
        int line = -1, level = 0;
        if ( nrhs == 5 ) {
            if ( mxGetClassID( prhs[2] ) != mxCHAR_CLASS )
                mexErrMsgTxt( "file must be a string" );
            char *file_buf = mxArrayToString( prhs[2] );
            file           = std::string( file_buf );
            mxFree( file_buf );
            if ( mxGetNumberOfElements( prhs[3] ) != 1 )
                mexErrMsgTxt( "line must be size 1" );
            line = static_cast<int>( mxGetScalar( prhs[3] ) );
            if ( mxGetNumberOfElements( prhs[4] ) != 1 )
                mexErrMsgTxt( "level must be size 1" );
            level = static_cast<int>( mxGetScalar( prhs[4] ) );
        } else {
            // Get the file and line number by calling DBSTACK
            mxArray *lhs[1];
            int error = mexCallMATLAB( 1, lhs, 0, NULL, "dbstack" );
            if ( error != 0 )
                mexErrMsgTxt( "Error calling dbstack" );
            if ( mxGetNumberOfElements( lhs[0] ) == 0 )
                mexErrMsgTxt( "No stack found from dbstack" );
            char *file_buf = mxArrayToString( mxGetField( lhs[0], 0, "file" ) );
            file           = std::string( file_buf );
            mxFree( file_buf );
            line = static_cast<int>( mxGetScalar( mxGetField( lhs[0], 0, "line" ) ) );
            mxDestroyArray( lhs[0] );
            // Get the level
            if ( nrhs == 3 ) {
                if ( mxGetNumberOfElements( prhs[2] ) != 1 )
                    mexErrMsgTxt( "level must be size 1" );
                level = static_cast<int>( mxGetScalar( prhs[2] ) );
            }
        }
        // Start/Stop the timer
        try {
            if ( command == "START" )
                global_profiler.start( name, file.c_str(), line, level );
            else
                global_profiler.stop( name, file.c_str(), line, level );
        } catch ( const std::exception &e ) {
            std::string msg2 = std::string( "Unable to start/stop timer:\n  " ) + e.what();
            mexErrMsgTxt( msg2.c_str() );
        } catch ( ... ) {
            mexErrMsgTxt( "Unable to start/stop timer (caught unknown exception)" );
        }
    } else {
        mexErrMsgTxt( "Unknown command\n" );
    }
}
