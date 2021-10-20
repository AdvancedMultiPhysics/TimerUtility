#include "ProfilerApp.h"
#include <string>
#include <vector>


bool load_test( const std::string& name, size_t size, bool includes_trace, bool includes_memory )
{

    auto load_results = ProfilerApp::load( name );
    auto& timers      = load_results.timers;

    // Check the timers
    bool pass = !timers.empty();
    if ( timers.empty() )
        std::cout << "No timer data loaded\n";
    std::vector<bool> rank_called( size, false );
    for ( auto& timer : timers ) {
        for ( const auto& trace : timer.trace ) {
            rank_called[trace.rank] = true;
            pass                    = pass && trace.N > 0 && trace.rank < size;
        }
    }
    for ( size_t i = 0; i < size; i++ )
        pass = pass && rank_called[i];
    if ( !pass )
        std::cout << "Error with timers\n";

    // Check the traces
    if ( includes_trace && pass ) {
        for ( auto& timer : timers ) {
            for ( const auto& trace : timer.trace )
                pass = pass && trace.N > 0 && trace.N_trace > 0;
        }
        if ( !pass )
            std::cout << "Error with trace data\n";
    }

    // Check the memory
    if ( includes_memory && pass ) {
        pass = pass && load_results.memory.size() == size;
        for ( size_t i = 0; i < size && pass; i++ ) {
            auto& memory = load_results.memory[i];
            pass         = pass && load_results.memory[i].rank == (int) i;
            pass         = pass && !memory.time.empty();
        }
        if ( !pass )
            std::cout << "Error with memory data\n";
    }

    return pass;
}


int main( int, char*[] )
{

    // Run the tests
    bool pass = true;
    pass      = pass && load_test( "set1", 4, false, false );
    pass      = pass && load_test( "set2", 4, true, true );

    // Finalize MPI and SAMRAI
    if ( pass )
        std::cout << "All tests passed" << std::endl;
    else
        std::cout << "Some tests failed" << std::endl;
    return pass ? 0 : 1;
}
