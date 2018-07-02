#include "MemoryApp.h"
#include "ProfilerApp.h"
#include <string>
#include <vector>


int main( int argc, char *argv[] )
{
    // Check the input arguments
    if ( argc != 2 ) {
        std::cerr << "test_Load called with the wrong number of arguments\n";
        return -1;
    }
    std::string filename( argv[1] );

    // Run the tests
    int N_errors                    = 0;
    MemoryApp::MemoryStats m1       = MemoryApp::getMemoryStats();
    bool global                     = filename.rfind( ".0.timer" ) != std::string::npos;
    size_t i                        = filename.rfind( '.', filename.rfind( ".timer" ) - 1 );
    filename                        = filename.substr( 0, i );
    TimerMemoryResults load_results = ProfilerApp::load( filename, -1, global );
    MemoryApp::MemoryStats m2       = MemoryApp::getMemoryStats();
    std::cout << "Profile results loaded\n";
    std::cout << "   " << m2.tot_bytes_used - m1.tot_bytes_used << " bytes used\n";

    // Finalize MPI and SAMRAI
    if ( N_errors == 0 )
        std::cout << "All tests passed" << std::endl;
    else
        std::cout << "Some tests failed" << std::endl;
    return N_errors;
}
