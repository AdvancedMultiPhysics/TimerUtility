#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <mex.h>
#include <math.h>


// Function to identify all points between start and stop
template<class TYPE> 
void search( size_t Nt, const TYPE* t, size_t Ns, const TYPE* start, const TYPE* stop, bool* active )
{
    for (size_t i=0; i<Nt; i++)
        active[i] = false;
    TYPE t_last = 1e100;
    size_t i_last = 0;
    for (size_t i=0; i<Ns; i++) {
        double t1 = start[i];
        double t2 = stop[i];
        if ( t1 < t_last )
            i_last = 0;
        t_last = t1;
        for (size_t j=i_last; j<Nt; j++) {
            if ( t[j]<t1 ) {
                i_last++;
                continue;
            }
            if ( t[j]>t2 )
                break;
            active[j] = true;
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
            "Function to return if the times that are between a start and stop.\n"
            "Usage:  active = get_active_times(t,start,stop);\n");
    }
    mwSize Nt = mxGetNumberOfElements(prhs[0]);
    mwSize Ns = mxGetNumberOfElements(prhs[1]);
    if ( Ns!=mxGetNumberOfElements(prhs[2]) )
        mexErrMsgTxt("start and stop must be the same length");
    if ( mxIsComplex(prhs[0]) || mxIsComplex(prhs[1]) || mxIsComplex(prhs[2]) )
        mexErrMsgTxt("all inputs must be real");

    // Get the class ids for all inputs
    mxClassID t_id = mxGetClassID(prhs[0]);
    mxClassID start_id = mxGetClassID(prhs[1]);
    mxClassID stop_id = mxGetClassID(prhs[2]);

    // Allocate output arguments
    plhs[0] = mxCreateLogicalMatrix(1,Nt);
    bool *data = reinterpret_cast<bool*>(mxGetPr(plhs[0]));

    // Run the problem, converting the input arguments if necessary
    if ( Ns==0 ) {
        // Special case for empty start/stop times
        for (size_t i=0; i<Nt; i++)
            data[i] = false;
    } else if ( t_id==mxDOUBLE_CLASS && start_id==mxDOUBLE_CLASS && stop_id==mxDOUBLE_CLASS ) {
        // All inputs are double precision
        const double *t     = mxGetPr(prhs[0]);
        const double *start = mxGetPr(prhs[1]);
        const double *stop  = mxGetPr(prhs[2]);
        search<double>( Nt, t, Ns, start, stop, data );
    } else if ( t_id==mxSINGLE_CLASS && start_id==mxSINGLE_CLASS && stop_id==mxSINGLE_CLASS ) {
        // All inputs are single precision
        const float *t     = reinterpret_cast<const float*>(mxGetData(prhs[0]));
        const float *start = reinterpret_cast<const float*>(mxGetData(prhs[1]));
        const float *stop  = reinterpret_cast<const float*>(mxGetData(prhs[2]));
        search<float>( Nt, t, Ns, start, stop, data );
    } else if ( t_id==mxDOUBLE_CLASS && start_id==mxSINGLE_CLASS && stop_id==mxSINGLE_CLASS ) {
        // Mix of double and single precision
        const double *t_in = mxGetPr(prhs[0]);
        const float *start = reinterpret_cast<const float*>(mxGetData(prhs[1]));
        const float *stop  = reinterpret_cast<const float*>(mxGetData(prhs[2]));
        float *t = new float[Nt];
        for (int i=0; i<Nt; i++)
            t[i] = static_cast<float>(t_in[i]);
        search<float>( Nt, t, Ns, start, stop, data );
        delete [] t;
    } else {
        mexErrMsgTxt("Unsupported input types");
    }
}


