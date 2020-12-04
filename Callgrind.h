#ifndef included_Callgrind
#define included_Callgrind

#include "ProfilerApp.h"

#include <map>
#include <string>
#include <vector>


// Structures used to store callgrind results
struct callgrind_subfunction_struct {
    id_struct id;
    int calls;
    long int cost;
    callgrind_subfunction_struct() : calls( 0 ), cost( 0 ) {}
    bool operator<( const callgrind_subfunction_struct& rhs ) const { return id < rhs.id; }
};
struct callgrind_function_struct {
    id_struct id;
    int obj;
    int file;
    int fun;
    long int inclusive_cost;
    long int exclusive_cost;
    std::vector<callgrind_subfunction_struct> subfunctions;
    callgrind_function_struct()
        : obj( -1 ), file( -1 ), fun( -1 ), inclusive_cost( 0 ), exclusive_cost( 0 )
    {
    }
    bool operator<( const callgrind_function_struct& rhs ) const { return id < rhs.id; }
};
struct callgrind_results {
    std::map<int, std::string> name_map;
    std::vector<callgrind_function_struct> functions;
};


// Load a callgrind file
callgrind_results loadCallgrind( const std::string& filename, double tol = 0 );


// Convert callgrind results into timers
std::vector<TimerResults> convertCallgrind( const callgrind_results& callgrind );


#endif
