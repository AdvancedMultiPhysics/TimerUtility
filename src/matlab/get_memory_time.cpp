#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <mex.h>
#include <math.h>


template<class TYPE1, class TYPE2>
void fillData( size_t N1, const TYPE1 *x1, const TYPE2 *y1, size_t N2, const TYPE1 *x2, TYPE2 *y2 )
{
    memset(y2,0,N2*sizeof(TYPE2));
    if ( N1==0 || N2==0 )
        return;
    for (size_t i=1; i<N1; i++) {
        if ( x1[i] < x1[i-1] )
            mexErrMsgTxt("time is not in sorted order");
    }
    for (size_t i=1; i<N2; i++) {
        if ( x2[i] < x2[i-1] )
            mexErrMsgTxt("t is not in sorted order");
    }
    if ( x2[N2-1]<x1[0] || x2[0]>x1[N1-1] )
        return;
    size_t i=0;    // Index to loop through x1
    size_t j=0;    // Index to loop through x2
    while ( x2[j] < x1[0] )
        j++;
    while ( i<N1-1 && j<N2 ) {
        if ( x1[i+1] <= x2[j] ) {
            i++;
        } else {
            y2[j] = y1[i];
            j++;
        }
    }
}


/******************************************************************
* This is the MATLAB interface                                    *
******************************************************************/
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{

    // Check that there are the proper number of arguments
    if ( nrhs!=3 || nlhs!=1 ) {
        mexErrMsgTxt("Error, Incorrect number of inputs or outputs.\n"
            "Function to return the memory used at a given time\n"
            "Usage:  bytes2 = get_memory_time(t,time,bytes);\n"
            "Note: t and time are assumed to be in sorted order\n");
    }
    mwSize Nt = mxGetNumberOfElements(prhs[0]);
    mwSize Ns = mxGetNumberOfElements(prhs[1]);
    if ( Ns!=mxGetNumberOfElements(prhs[2]) )
        mexErrMsgTxt("time and bytes must be the same length");
    if ( mxIsComplex(prhs[0]) || mxIsComplex(prhs[1]) || mxIsComplex(prhs[2]) )
        mexErrMsgTxt("all inputs must be real");

    // Get the class ids for all inputs
    mxClassID t_id = mxGetClassID(prhs[0]);
    mxClassID time_id = mxGetClassID(prhs[1]);
    mxClassID bytes_id = mxGetClassID(prhs[2]);

    // Allocate output arguments
    plhs[0] = mxCreateNumericMatrix(1,Nt,bytes_id,mxREAL);

    // Run the problem, converting the input arguments if necessary
    if ( t_id==mxDOUBLE_CLASS && time_id==mxDOUBLE_CLASS && bytes_id==mxDOUBLE_CLASS ) {
        // All inputs are double precision
        const double *t     = mxGetPr(prhs[0]);
        const double *time  = mxGetPr(prhs[1]);
        const double *bytes = mxGetPr(prhs[2]);
        double *bytes_out = mxGetPr(plhs[0]);
        fillData<double,double>(Ns,time,bytes,Nt,t,bytes_out);
    } else if ( t_id==mxSINGLE_CLASS && time_id==mxSINGLE_CLASS && bytes_id==mxSINGLE_CLASS ) {
        // All inputs are single precision
        const float *t     = reinterpret_cast<const float*>(mxGetPr(prhs[0]));
        const float *time  = reinterpret_cast<const float*>(mxGetPr(prhs[1]));
        const float *bytes = reinterpret_cast<const float*>(mxGetPr(prhs[2]));
        float *bytes_out   = reinterpret_cast<float*>(mxGetPr(plhs[0]));
        fillData<float,float>(Ns,time,bytes,Nt,t,bytes_out);
    } else if ( t_id==mxDOUBLE_CLASS && time_id==mxSINGLE_CLASS && bytes_id==mxUINT32_CLASS ) {
        // Mix of types
        const double *t     = mxGetPr(prhs[0]);
        const float *time   = reinterpret_cast<const float*>(mxGetPr(prhs[1]));
        const uint32_t *bytes = reinterpret_cast<const uint32_t*>(mxGetPr(prhs[2]));
        uint32_t *bytes_out   = reinterpret_cast<uint32_t*>(mxGetPr(plhs[0]));
        float *t2 = new float[Nt];
        for (size_t i=0; i<Nt; i++)
            t2[i] = static_cast<float>(t[i]);
        fillData<float,uint32_t>(Ns,time,bytes,Nt,t2,bytes_out);
        delete [] t2;
    } else {
        mexErrMsgTxt("Unsupported input types");
    }
}


