#include "MemoryApp.h"
#include <iostream>
#include <iostream>
#include <sstream>
#include <stdexcept>


#define NULL_USE(variable) do {                         \
    if(0) {char *temp = (char *)&variable; temp++;}     \
}while(0)


inline void check_ptr( double *tmp ) 
{
    std::cout << tmp << std::endl;
    tmp[0] = rand();
    if ( tmp[0]==0 )
        std::cout << "Error in test" << std::endl;
}

#define ASSERT(EXP) do {                                \
    if ( !(EXP) ) {                                     \
        std::stringstream tboxos;                       \
        tboxos << "Failed assertion: " << #EXP << std::ends; \
        delete [] tmp;                                  \
        throw std::logic_error(tboxos.str());           \
    }                                                   \
}while(0)


int main(int, char*[])
{
    int N_errors = 0;
    
    // Get the initial memory
    MemoryApp::MemoryStats m1 = MemoryApp::getMemoryStats();
    if ( m1.bytes_new!=0 || m1.bytes_delete!=0 || m1.N_new!=0 || m1.N_delete!=0 ) {
        std::cout << "Memory structure is not initialized to 0" << std::endl;
        N_errors++;
    }

    // Test new/delete
    #ifdef DISABLE_NEW_OVERLOAD
        std::cout << "Skipping new/delete tests (disabled)" << std::endl;
    #else
        double *tmp = new double();
        check_ptr(tmp);
        MemoryApp::MemoryStats m2 = MemoryApp::getMemoryStats();
        delete tmp;
        MemoryApp::MemoryStats m3 = MemoryApp::getMemoryStats();
        tmp = new double[1000];
        check_ptr(tmp);
        MemoryApp::MemoryStats m4 = MemoryApp::getMemoryStats();
        delete tmp;
        MemoryApp::MemoryStats m5 = MemoryApp::getMemoryStats();
        if ( m2.bytes_new<m1.bytes_new+8 || m2.N_new!=1 ) {
            std::cout << "Failed new test\n";
            N_errors++;
        }
        if ( m3.bytes_delete!=m2.bytes_new || m3.N_delete!=1 ) {
            std::cout << "Failed delete test\n";
            N_errors++;
        }
        if ( m4.bytes_new<m2.bytes_new+8000 || m4.N_new!=2 ) {
            std::cout << "Failed new[] test\n";
            N_errors++;
        }
        if ( m5.bytes_delete!=m4.bytes_new || m5.N_delete!=2 ) {
            std::cout << "Failed delete[] test\n";
            N_errors++;
        }
    #endif
    
    // Print the memory stats
    MemoryApp::print(std::cout);

    // Finalize MPI and SAMRAI
    if ( N_errors==0 ) 
        std::cout << "All tests passed" << std::endl;
    else
        std::cout << "Some tests failed" << std::endl;
    return(N_errors);
}



