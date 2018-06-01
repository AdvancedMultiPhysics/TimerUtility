#include "ProfilerApp.h"
#include <string>
#include <vector>


int load_test( const std::string& name, size_t size, bool includes_trace, bool includes_memory )
{

    auto load_results = ProfilerApp::load( name );
    auto& timers      = load_results.timers;

    // Check the timers
    if ( timers.empty() ) {
        std::cout << "No timer data loaded\n";
        return 1;
    }
    bool error = false;
    std::vector<bool> rank_called( size, false );
    for ( auto& timer : timers ) {
        for ( const auto& trace : timer.trace ) {
            rank_called[trace.rank] = true;
            if ( trace.N == 0 || trace.rank >= size ) {
                error = true;
                break;
            }
        }
    }
    for ( size_t i = 0; i < size; i++ ) {
        if ( !rank_called[i] )
            error = true;
    }
    if ( error ) {
        std::cout << "Error with timers\n";
        return 1;
    }

    // Check the traces
    if ( includes_trace ) {
        error = false;
        for ( auto& timer : timers ) {
            for ( const auto& trace : timer.trace ) {
                if ( trace.N == 0 || trace.N_trace == 0 )
                    error = true;
            }
        }
        if ( error ) {
            std::cout << "Error with trace data\n";
            return 1;
        }
    }

    // Check the memory
    if ( includes_memory ) {
        error = false;
        if ( load_results.memory.size() != size ) {
            error = true;
        } else {
            for ( size_t i = 0; i < size; i++ ) {
                if ( load_results.memory[i].rank != (int) i ) {
                    error = true;
                    break;
                }
                MemoryResults& memory = load_results.memory[i];
                if ( memory.time.empty() )
                    error = true;
            }
        }
        if ( error ) {
            std::cout << "Error with memory data\n";
            return 1;
        }
    }

    return 0;
}


int main( int, char* [] )
{

    // Run the tests
    int N_errors = 0;
    N_errors += load_test( "set1", 4, false, false );
    N_errors += load_test( "set2", 4, true, true );

    // Finalize MPI and SAMRAI
    if ( N_errors == 0 )
        std::cout << "All tests passed" << std::endl;
    else
        std::cout << "Some tests failed" << std::endl;
    return N_errors;
}
