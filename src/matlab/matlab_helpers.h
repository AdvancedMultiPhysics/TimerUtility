#ifndef included_TimerUtilitiy_matlab_helpers
#define included_TimerUtilitiy_matlab_helpers

#include "mex.h"
#include <complex>
#include <stdint.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <cstdarg>


// stringf
inline std::string stringf( const char *format, ... )
{
    va_list ap; 
    va_start(ap,format); 
    char tmp[1024];
    vsprintf(tmp,format,ap);
    va_end(ap);
    return std::string(tmp);
}


inline std::string loadMexString( const mxArray* ptr )
{
    if ( ptr==NULL )
        return std::string();
    if( !mxIsChar(ptr) )
        return std::string();
    char *tmp = mxArrayToString(ptr);
    std::string str(tmp);
    mxFree(tmp);
    return str;
}


/************************************************************************
* Functions to create a MATLAB class object                             *
************************************************************************/
mxArray *mxCreateClassArray( mwSize ndim, const mwSize *dims, const char *classname )
{
    PROFILE_START("mxCreateClassArray",1);
    // Create the arguments to feval
    size_t N = 1;
    for (size_t i=0; i<ndim; i++)
        N *= dims[i];
    std::string arg;
    if ( N > 1 ) {
        arg = stringf("x(%i",static_cast<int>(dims[0]));
        for (size_t i=1; i<ndim; i++)
            arg += stringf(",%i",static_cast<int>(dims[i]));
        arg += ") = " + std::string(classname) + ";";
    } else {
        arg = "x = " + std::string(classname) + ";";
    }
    mxArray *rhs = mxCreateString(arg.c_str());
    // Call MATLAB
    mxArray *mx = NULL;
    mxArray *mxtrap = mexCallMATLABWithTrap(1,&mx,1,&rhs,"mxCreateClassArrayHelper");
    mxDestroyArray(rhs);
    // Check for errors and return
    if( mxtrap !=NULL ) {
        std::string msg = stringf("Error creating class %s (1): %s\n",classname,arg.c_str());
        msg += "  identifier: " + loadMexString(mxGetProperty(mxtrap,0,"identifier")) + "\n";
        msg += "  message: " + loadMexString(mxGetProperty(mxtrap,0,"message")) + "\n";
        mxDestroyArray(mxtrap); mx = NULL;
        mexErrMsgTxt(msg.c_str());
    }
    const std::string className(mxGetClassName(mx));
    if ( className != classname )
        mexErrMsgTxt(stringf("Error creating class %s (2)",classname).c_str());
    PROFILE_STOP("mxCreateClassArray",1);
    return mx;
}
mxArray *mxCreateClassMatrix( mwSize n, mwSize m, const char *classname )
{
    mwSize dims[2] = {n,m};
    return mxCreateClassArray(2,dims,classname);
}


#endif

