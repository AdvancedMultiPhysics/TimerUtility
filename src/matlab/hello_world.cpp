#include <mex.h>
#include <stdlib.h>


void mexFunction( int, mxArray *[], int, const mxArray *[] )
{
    // Create a minor memory leak for valgrind to catch
    char *tmp = new char[517];
    if ( tmp == NULL )
        mexErrMsgTxt( "Internal error" );
    // Return an error message with hello world
    mexErrMsgTxt( "Hello World" );
}
