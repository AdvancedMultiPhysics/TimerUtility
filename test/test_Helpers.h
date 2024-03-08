#include "ProfilerApp.h"

#include <sstream>
#include <stdexcept>
#include <stdio.h>


// Define NULL_USE
#define NULL_USE( variable )                       \
    do {                                           \
        if ( 0 ) {                                 \
            auto static temp = (char *) &variable; \
            temp++;                                \
        }                                          \
    } while ( 0 )


/***********************************************************************
 * Subroutine to perform a quicksort                                    *
 ***********************************************************************/
template<class T1, class T2>
static inline void quicksort( int64_t n, T1 *arr, T2 *brr )
{
    if ( n <= 1 )
        return;
    int64_t jstack = 0;
    int64_t l      = 0;
    int64_t ir     = n - 1;
    int64_t istack[100];
    while ( 1 ) {
        if ( ir - l < 7 ) { // Insertion sort when subarray small enough
            T1 a;
            T2 b;
            for ( int64_t j = l + 1; j <= ir; j++ ) {
                std::swap( a, arr[j] );
                std::swap( b, brr[j] );
                bool test = true;
                for ( int64_t i = j - 1; i >= 0; i-- ) {
                    if ( arr[i] < a ) {
                        std::swap( a, arr[i + 1] );
                        std::swap( b, brr[i + 1] );
                        test = false;
                        break;
                    }
                    std::swap( arr[i], arr[i + 1] );
                    std::swap( brr[i], brr[i + 1] );
                }
                if ( test ) {
                    int64_t i = l - 1;
                    std::swap( a, arr[i + 1] );
                    std::swap( b, brr[i + 1] );
                }
            }
            if ( jstack == 0 )
                return;
            ir = istack[jstack]; // Pop stack and begin a new round of partitioning
            l  = istack[jstack - 1];
            jstack -= 2;
        } else {
            // Choose median of left, center and right elements as partitioning element a
            // Also rearrange so that a(l) ? a(l+1) ? a(ir)
            int64_t k = ( l + ir ) / 2;
            std::swap( arr[k], arr[l + 1] );
            std::swap( brr[k], brr[l + 1] );
            if ( arr[l] > arr[ir] ) {
                std::swap( arr[l], arr[ir] );
                std::swap( brr[l], brr[ir] );
            }
            if ( arr[l + 1] > arr[ir] ) {
                std::swap( arr[l + 1], arr[ir] );
                std::swap( brr[l + 1], brr[ir] );
            }
            if ( arr[l] > arr[l + 1] ) {
                std::swap( arr[l], arr[l + 1] );
                std::swap( brr[l], brr[l + 1] );
            }
            // Scan up to find element > a
            int64_t j = ir;
            // Partitioning element
            T1 a;
            T2 b;
            std::swap( a, arr[l + 1] );
            std::swap( b, brr[l + 1] );
            int64_t i;
            for ( i = l + 2; i <= ir; i++ ) {
                if ( arr[i] < a )
                    continue;
                while ( arr[j] > a ) // Scan down to find element < a
                    j--;
                if ( j < i )
                    break;                   // Pointers crossed. Exit with partitioning complete
                std::swap( arr[i], arr[j] ); // Exchange elements of both arrays
                std::swap( brr[i], brr[j] );
            }
            // Insert partitioning element in both arrays
            std::swap( arr[l + 1], arr[j] );
            std::swap( brr[l + 1], brr[j] );
            std::swap( arr[j], a );
            std::swap( brr[j], b );
            jstack += 2;
            // Push pointers to larger subarray on stack, process smaller subarray immediately
            if ( ir - i + 1 >= j - l ) {
                istack[jstack]     = ir;
                istack[jstack - 1] = i;
                ir                 = j - 1;
            } else {
                istack[jstack]     = j - 1;
                istack[jstack - 1] = l;
                l                  = i;
            }
        }
    }
}
template<class type_a, class type_b>
static inline void quicksort( std::vector<type_a> &arr, std::vector<type_b> &brr )
{
    if ( arr.size() != brr.size() )
        throw std::logic_error( "Vector sizes do not match" );
    quicksort( arr.size(), arr.data(), brr.data() );
}


/***********************************************************************
 * Sort the profile results                                             *
 ***********************************************************************/
void sort( std::vector<TimerResults> &x )
{
    std::vector<id_struct> id( x.size() );
    for ( size_t i = 0; i < x.size(); i++ )
        id[i] = x[i].id;
    quicksort( id, x );
}


/***********************************************************************
 * Find a timer by message                                              *
 ***********************************************************************/
int find( const std::vector<TimerResults> &x, const std::string &message )
{
    for ( size_t i = 0; i < x.size(); i++ ) {
        if ( std::string( x[i].message ) == message )
            return i;
    }
    return -1;
}


/***********************************************************************
 * Find an entry in the profile results (assumes they are sorted)       *
 ***********************************************************************/
int find( const std::vector<TimerResults> &x, const id_struct &id )
{
    if ( x.empty() )
        return -1;
    // Check if value is within the range of x
    if ( id < x[0].id || id > x.back().id )
        return -1;
    if ( id == x[0].id )
        return 0;
    // Perform the search
    size_t lower = 0;
    size_t upper = x.size() - 1;
    while ( ( upper - lower ) != 1 ) {
        size_t index = ( upper + lower ) / 2;
        if ( x[index].id >= id )
            upper = index;
        else
            lower = index;
    }
    if ( x[upper].id == id )
        return upper;
    return -1;
}


/***********************************************************************
 * Macros to enable/disable warnings                                    *
 ***********************************************************************/
// clang-format off
#ifdef DISABLE_WARNINGS
    // Macros previously defined
#elif defined( USING_MSVC )
    #define DISABLE_WARNINGS __pragma( warning( push, 0 ) )
    #define ENABLE_WARNINGS __pragma( warning( pop ) )
#elif defined( USING_CLANG )
    #define DISABLE_WARNINGS                                                \
        _Pragma( "clang diagnostic push" ) _Pragma( "clang diagnostic ignored \"-Wall\"" ) \
        _Pragma( "clang diagnostic ignored \"-Wextra\"" )                   \
        _Pragma( "clang diagnostic ignored \"-Wunused-private-field\"" )    \
        _Pragma( "clang diagnostic ignored \"-Wmismatched-new-delete\"" )
    #define ENABLE_WARNINGS _Pragma( "clang diagnostic pop" )
#elif defined( USING_GCC )
    #define DISABLE_WARNINGS                                                \
        _Pragma( "GCC diagnostic push" ) _Pragma( "GCC diagnostic ignored \"-Wall\"" ) \
        _Pragma( "GCC diagnostic ignored \"-Wextra\"" )                     \
        _Pragma( "GCC diagnostic ignored \"-Wpragmas\"" )                   \
        _Pragma( "GCC diagnostic ignored \"-Wunused-local-typedefs\"" )     \
        _Pragma( "GCC diagnostic ignored \"-Woverloaded-virtual\"" )        \
        _Pragma( "GCC diagnostic ignored \"-Wunused-parameter\"" )          \
        _Pragma( "GCC diagnostic ignored \"-Wsized-deallocation\"" )
#define ENABLE_WARNINGS _Pragma( "GCC diagnostic pop" )
#else
#define DISABLE_WARNINGS
#define ENABLE_WARNINGS
#endif
// clang-format on
