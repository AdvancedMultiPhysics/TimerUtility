#include <stdlib.h>
#include <mex.h>



void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    // Create a minor memory leak for valgrind to catch
    char *tmp = new char[517];
    if ( tmp == NULL )
        mexErrMsgTxt("Internal error");
    // Return an error message with hello world
    mexErrMsgTxt("Hello World");
}

