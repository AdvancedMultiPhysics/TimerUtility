#include "Callgrind.h"
#include "ProfilerApp.h"
#include <string>
#include <vector>
#include <stdio.h>
#include <iostream>


int main(int argc, char *argv[])
{
    // Check the input arguments
    if ( argc != 2 ) {
        std::cerr << "test_Callgrind called with the wrong number of arguments\n";
        return -1;
    }
    std::string filename(argv[1]);

    // Run the tests
    int N_errors = 0;
    callgrind_results results = loadCallgrind(filename);
    std::cout << "Callgrind results loaded\n";

    // Convert callgrind to timers
    std::vector<TimerResults> timers = convertCallgrind(results);
    std::cout << "Converted to timers\n";

    // Finalize MPI and SAMRAI
    if ( N_errors==0 ) 
        std::cout << "All tests passed" << std::endl;
    else
        std::cout << "Some tests failed" << std::endl;
    return N_errors;
}



