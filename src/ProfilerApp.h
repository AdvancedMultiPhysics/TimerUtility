#ifndef included_ProfilerApp
#define included_ProfilerApp


#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "ProfilerAtomicHelpers.h"
#include "ProfilerDefinitions.h"


class ScopedTimer;


/** \class id_struct
 *
 * Structure to store id string
 */
struct id_struct {
    id_struct() { data.u64 = 0; }
    id_struct( const id_struct& rhs ) { data.u64 = rhs.data.u64; }
    id_struct& operator=( const id_struct& rhs )
    {
        this->data.u64 = rhs.data.u64;
        return *this;
    }
    explicit id_struct( const std::string& rhs )
    {
        data.u64 = 0;
        rhs.copy( data.str, 8 );
    }
    explicit id_struct( const char* rhs )
    {
        data.u64 = 0;
        for ( int i = 0; i < 8 && rhs[i] > 0; i++ )
            data.str[i] = rhs[i];
    }
    inline const char* c_str() const { return data.str; }
    inline const std::string string() const { return std::string( data.str, 0, 8 ); }
    inline bool operator==( const id_struct& rhs ) const { return data.u64 == rhs.data.u64; }
    inline bool operator!=( const id_struct& rhs ) const { return data.u64 != rhs.data.u64; }
    inline bool operator>=( const id_struct& rhs ) const { return data.u64 >= rhs.data.u64; }
    inline bool operator>( const id_struct& rhs ) const { return data.u64 > rhs.data.u64; }
    inline bool operator<( const id_struct& rhs ) const { return data.u64 < rhs.data.u64; }
    inline bool operator<=( const id_struct& rhs ) const { return data.u64 <= rhs.data.u64; }
    static id_struct create_id( uint64_t id );

private:
    union {
        uint64_t u64;
        char str[8];
    } data;
};


/** \class TraceResults
 *
 * Structure to store results of a single trace from the profiler application.
 * Note: field types and sizes are set to minimize the storage requirements
 *    of this structure (currently 48 bytes without active timers or traces).
 */
struct TraceResults {
    id_struct id;                                   //!<  ID of parent timer
    uint16_t N_active;                              //!<  Number of active timers
    uint16_t thread;                                //!<  Active thread
    uint32_t rank;                                  //!<  Rank
    uint32_t N_trace;                               //!<  Number of calls that we trace
    float min;                                      //!<  Minimum call time
    float max;                                      //!<  Maximum call time
    float tot;                                      //!<  Total call time
    uint64_t N;                                     //!<  Total number of calls
    id_struct* active();                            //!<  List of active timers
    const id_struct* active() const;                //!<  List of active timers
    double* start();                                //!<  Start times for each call
    const double* start() const;                    //!<  Start times for each call
    double* stop();                                 //!<  Stop times for each call
    const double* stop() const;                     //!<  Stop times for each call
    TraceResults();                                 //!<  Empty constructor
    ~TraceResults();                                //!<  Destructor
    TraceResults( const TraceResults& );            //!<  Copy constructor
    TraceResults& operator=( const TraceResults& ); //! Assignment operator
    void allocate();                                //!<  Allocate the data
    size_t size( bool store_trace = true ) const; //!< The number of bytes needed to pack the trace
    size_t pack( char* data, bool store_trace = true ) const; //!<  Pack the data to a buffer
    size_t unpack( const char* data );                        //!<  Unpack the data from a buffer
    bool operator==( const TraceResults& rhs ) const;         //! Comparison operator
    inline bool operator!=( const TraceResults& rhs ) const { return !( this->operator==( rhs ) ); }

private:
    double* mem; // Internal memory
};


/** \class TimerResults
 *
 * Structure to store results of a single timer from the profiler application.
 */
struct TimerResults {
    id_struct id;                                 //!<  Timer ID
    std::string message;                          //!<  Timer message
    std::string file;                             //!<  Timer file
    std::string path;                             //!<  Timer file path
    int start;                                    //!<  Timer start line (-1: never defined)
    int stop;                                     //!<  Timer stop line (-1: never defined)
    std::vector<TraceResults> trace;              //!< Trace data
    size_t size( bool store_trace = true ) const; //!< The number of bytes needed to pack the trace
    size_t pack( char* data, bool store_trace = true ) const; //!<  Pack the data to a buffer
    size_t unpack( const char* data );                        //!<  Unpack the data from a buffer
    bool operator==( const TimerResults& rhs ) const;         //! Comparison operator
    inline bool operator!=( const TimerResults& rhs ) const { return !( this->operator==( rhs ) ); }
};


/** \class MemoryResults
 *
 * Structure to store the memory results of a single rank.
 */
struct MemoryResults {
    int rank;                          //!<  Rank
    std::vector<double> time;          //!<  Time
    std::vector<uint64_t> bytes;       //!<  Memory in use
    size_t size() const;               //!< The number of bytes needed to pack the trace
    size_t pack( char* data ) const;   //!<  Pack the data to a buffer
    size_t unpack( const char* data ); //!<  Unpack the data from a buffer
    bool operator==( const MemoryResults& rhs ) const; //! Comparison operator
    inline bool operator!=( const MemoryResults& rhs ) const
    {
        return !( this->operator==( rhs ) );
    }
};


/** \class TimerMemoryResults
 *
 * Structure to store results of timers and memory
 */
struct TimerMemoryResults {
    int N_procs;
    std::vector<TimerResults> timers;
    std::vector<MemoryResults> memory;
    size_t size() const;               //!< The number of bytes needed to pack the trace
    size_t pack( char* data ) const;   //!<  Pack the data to a buffer
    size_t unpack( const char* data ); //!<  Unpack the data from a buffer
    TimerMemoryResults() : N_procs( 0 ) {}
    bool operator==( const TimerMemoryResults& rhs ) const; //! Comparison operator
    inline bool operator!=( const TimerMemoryResults& rhs ) const
    {
        return !( this->operator==( rhs ) );
    }
};


// clang-format off
/** \class ProfilerApp
 *
 * This class provides some basic timing and profiling capabilities.
 * It works be providing start, stop, and save functions that allow the user to record
 * the total time required for blocks of code.  The results are written to an ASCII data
 * file that can be viewed directly or processed.  It is compatible with MPI and SAMRAI.
 * The overhead of the function is ~1us per call for start and stop.
 * The time required to save depends of the amount of data to be written and the speed of the writes.
 * Additional details can be provided with set_store_trace which records the actual time (from startup)
 * for each start and stop call.  This method significantly increases the memory requirements.
 * Several preprocessor define statement are given for easy incorporation: \verbatim
 *    PROFILE_START(NAME) - Start profiling a block of code with the given name.
 *                          The name must be unique to the file and there must be a corresponding stop statement.
 *    PROFILE_STOP(NAME)  - Stop profiling a block of code with the given name.
 *                          The name must match the given start block, and there must only be one
 *                          PROFILE_STOP for each PROFILE_START. This records the current line
 *                          number as the final line number for the block of code.
 *    PROFILE_STOP2(NAME) - Stop profiling a block of code with the given name.
 *                          The name must match the given start block.  This is a special stop
 *                          that does not use the current line as the final line of the block,
 *                          but is provided for cases where there are multiple exit paths for
 *                          a given function or block of code.
 *    PROFILE_SAVE(FILE)  - Save the results of profiling to a file.
 *    PROFILE_ENABLE(0)   - Enable the profiler with a default level of 0
 *    PROFILE_ENABLE(ln)  - Enable the profiler with the given level
 *    PROFILE_DISABLE()   - Disable the profiler
 *    PROFILE_ENABLE_TRACE()  - Enable the trace-level data
 *    PROFILE_DISABLE_TRACE() - Disable the trace-level data
 *    PROFILE_ENABLE_MEMORY()  - Enable tracing the memory usage over time
 *    PROFILE_DISABLE_MEMORY() - Disable tracing the memory usage over time
 * \endverbatim
 * Note that these commands are global and will create a global profiler.  It is possible
 * for a user to create multiple profilers and this should not create any problems, but the
 * class interface should be used. <BR>
 * All start/stop and enable support an optional argument level (>=0) that specifies the level of detail
 * for the timers.  All timers with a number greater than the current level in the profiler will be
 * ignored. The macros PROFILE_START and PROFILE_STOP automatically check the level for performance
 * and calling an unused timer adds 2-5ns per call. <BR>
 * For repeated calls the timer adds ~100ns per call with without trace info, and ~1us per call
 * with full trace info. Most of this overhead is not in the time returned by the timer.
 * The resolution is ~ 1us for a single timer call. <BR>
 * Note: PROFILE_START and PROFILE_STOP require compile time string constants for the
 * message. Calling start/stop directly eliminates this requirement, but add ~200ns to the cost. <BR>
 * Note: When a timer is created the initial cost may be significantly higher, but this only
 * occurs once pertimer. <BR>
 * Note: The scoped timer (PROFILE_SCOPED) has very similar performance to START/STOP. <BR>
 * Example usage: \verbatim 
 *    void my_function(void *arg) {
 *       PROFILE_START("my function");
 *       int k;
 *       for (int i=0; i<10; i++) {
 *          PROFILE_START("sub call");
 *          // Do some work
 *          if ( special_case1 ) {
 *             PROFILE_STOP2("sub call");
 *             break;
 *          }
 *          if ( special_case2 ) {
 *             PROFILE_STOP2("sub call");
 *             PROFILE_STOP2("my function");
 *             return;
 *          }
 *          // Some more work
 *          PROFILE_STOP2("sub call");
 *       }
 *       PROFILE_STOP("my function");
 *    }
 *    // Some where at the end of the calculation
 *    PROFILE_SAVE("filename");
 * \endverbatim
 *
 * Special notes:<BR>
 * When using the memory tracing capabilities, the profiler application will attempt
 * to keep track of it's own memory usage and subtract that from the total application
 * memory.  The results will contain the memory used by the program without the overhead
 * of the profiler app.  Consequently the actual memory usage will be higher than reported
 * by this amount.  Users can query the profiler memory usage through the getMemoryUsed()
 * function.  Additionally, the profiler uses a number of small and large allocations
 * which may be rounded to boundaries by the system.  This overhead is NOT accounted for,
 * and may cause a slight variation in the reported memory usage.
 */
// clang-format on
class ProfilerApp final
{
public:
    //! Constructor
    ProfilerApp();

    // Copy constructor
    ProfilerApp( const ProfilerApp& ) = delete;

    //! Destructor
    ~ProfilerApp();

    /*!
     * \brief  Function to start profiling a block of code
     * \details  This function starts profiling a block of code until a corresponding stop
     *   is called.  It is recommended to use PROFILE_START(message) to call this routine.
     *   It will automatically fill in the file name and the line number.
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param[in] filename  Name of the file containing the code
     * @param[in] line      Line number containing the start command
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     */
    inline void start( const std::string& message, const char* filename, int line, int level = 0 )
    {
        if ( level < 0 || level >= 128 )
            throw std::logic_error( "level must be in the range 0-127" );
        if ( level <= d_level ) {
            uint32_t v1 = hashString( stripPath( filename ) );
            uint32_t v2 = hashString( message.c_str() );
            uint64_t id = ( static_cast<uint64_t>( v2 ) << 32 ) + static_cast<uint64_t>( v1 ^ v2 );
            start( id, message.c_str(), filename, line, level );
        }
    }

    /*!
     * \brief  Function to stop profiling a block of code
     * \details  This function stop profiling a block of code until a corresponding stop is called.
     *   It is recommended to use PROFILE_STOP(message) to call this routine.  It will
     *   automatically fill in the file name and the line number.
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param[in] filename  Name of the file containing the code
     * @param[in] line      Line number containing the start command
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     */
    inline void stop( const std::string& message, const char* filename, int line, int level = 0 )
    {
        if ( level < 0 || level >= 128 )
            throw std::logic_error( "level must be in the range 0-127" );
        if ( level <= d_level ) {
            uint32_t v1 = hashString( stripPath( filename ) );
            uint32_t v2 = hashString( message.c_str() );
            uint64_t id = ( static_cast<uint64_t>( v2 ) << 32 ) + static_cast<uint64_t>( v1 ^ v2 );
            stop( id, message.c_str(), filename, line, level );
        }
    }

    /*!
     * \brief  Function to check if a timer is active
     * \details  This function checks if a given timer is active on the current thread.
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must match a start call.
     * @param[in] filename  Name of the file containing the code
     */
    inline bool active( const std::string& message, const char* filename )
    {
        uint64_t id = getTimerId( message.c_str(), filename );
        auto timer  = getBlock( getThreadData(), id );
        return !timer ? false : timer->is_active;
    }

    /*!
     * \brief  Function to check if a timer is active
     * \details  This function checks if a given timer is active on the current thread.
     * @param[in] id        ID of the timer we want
     */
    inline bool active( uint64_t id )
    {
        auto timer = getBlock( getThreadData(), id );
        return !timer ? false : timer->is_active;
    }

    /*!
     * \brief  Function to save the profiling info
     * \details  This will save the current timer info.  This is a non-blocking function.
     * Note: .x.timer will automatically be appended to the filename, where x is the rank+1 of the
     * process. Note: .x.trace will automatically be appended to the filename when detailed traces
     * are used.
     * @param[in] filename  File name for saving the results
     * @param[in] global    Save a global file (true) or individual files (false)
     */
    void save( const std::string& filename, bool global = false ) const;

    /*!
     * \brief  Function to load the profiling info
     * \details  This will load the timing and trace info from a file
     * @param[in] filename  File name for loading the results
     *                      Note: .x.timer will be automatically appended to the filename
     * @param[in] rank      Rank to load (-1: all ranks)
     * @param[in] global    Save the time results in a global file (default is false)
     */
    static TimerMemoryResults load(
        const std::string& filename, int rank = -1, bool global = false );

    /*!
     * \brief  Function to synchronize the timers
     * \details  This function will synchronize the timers across multiple processors.
     *   If used, this function only needs to be called once.  This is only needed if
     *   the trace level data is being stored and the user wants the times synchronized.
     *   Note: This is a blocking call for all processors and must be called after MPI_INIT.
     */
    void synchronize();

    /*!
     * \brief  Function to enable the timers
     * \details  This function will enable the current timer clase.  It supports an optional level
     * argument that specifies the level of detail to use for the timers.
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     */
    void enable( int level = 0 );

    //! Function to enable the timers (all current timers will be deleted)
    void disable();

    /*!
     * \brief  Function to change if we are storing detailed trace information
     * \details  This function will change if we are storing detailed trace information (must be
     * called before any start) Note: Enabling this option will store the starting and ending time
     * for each call. This will allow the user to look at the detailed results to get trace
     * information. However this will significantly increase the memory requirements for any traces
     *  that get called repeatedly and may negitivly impact the performance.
     * @param[in] profile   Do we want to store detailed profiling data
     */
    void setStoreTrace( bool profile );

    /*!
     * \brief  Function to change if we are storing memory information
     * \details  This function will change if we are storing information about the memory usage
     *  as a function of time (must be called before any start).
     *  Note: Enabling this option will check the memory usage evergy time we enter or leave
     *  timer.  This data will be combined from all timers/threads to get the memory usage
     *  of the application over time.  Combined with the trace level data, we can determine
     *  when memory is allocated and which timers are active.
     * @param[in] memory    Do we want to store detailed profiling data
     */
    void setStoreMemory( bool memory );

    //! Return the current timer level
    inline int getLevel() const { return d_level; }

    /*!
     * \brief  Function to change the behavior of timer errors
     * \details  This function controls the behavior of the profiler when we encounter a timer
     *   error.  The default behavior is to abort.  Timer errors include starting a timer
     *   that is already started, or stopping a timer that is not running.
     *   The user should only disable theses checks if they understand the behavior.
     * @param[in] flag      Do we want to ignore timer errors
     */
    void ignoreTimerErrors( bool flag ) { d_disable_timer_error = flag; }

    /*!
     * \brief  Function to get the timer id
     * \details  This function returns the timer id given the message and filename.
     *     Internally all timers are stored using this id for faster searching.
     *     Many routines can take the timer id directly to imrove performance by
     *     avoiding the hashing function.
     * @param[in] message   The timer message
     * @param[in] filename  The filename
     */
    constexpr static inline uint64_t getTimerId( const char* message, const char* filename );

    /*!
     * \brief  Function to return the current timer results
     * \details  This function will return a vector containing the
     *      current timing results for all threads.
     */
    std::vector<TimerResults> getTimerResults() const;

    /*!
     * \brief  Function to return the current timer results
     * \details  This function will return a vector containing the
     *      current timing results for all threads.
     * @param[in] id        ID of the timer we want
     */
    TimerResults getTimerResults( uint64_t id ) const;

    /*!
     * \brief  Function to return the memory usage as a function of time
     * \details  This function will return a vector containing the
     *   memory usage as a function of time
     */
    MemoryResults getMemoryResults() const;

    /*!
     * \brief  Get the memory used by the profiler
     * \details  Return the total memory usage of the profiler app
     */
    size_t getMemoryUsed() const { return static_cast<size_t>( d_bytes ); }


public: // Fast interface to start/stop
    /*!
     * \brief  Function to start profiling a block of code (advanced interface)
     * \details  This function starts profiling a block of code until a corresponding stop
     *   is called. It is recommended to use PROFILE_START(message) to call this routine.
     *   It will automatically fill in the file name and the line number.
     *   Note: this is the advanced interface, we disable some error checking.
     * @param[in] id        Timer id (see get_timer_id for more info)
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param[in] filename  Name of the file containing the code
     * @param[in] line      Line number containing the start command
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     */
    inline void start( uint64_t id, const char* message, const char* filename, int line, int level )
    {
        if ( level <= d_level && level >= 0 ) {
            auto thread_data = getThreadData();
            auto timer       = getBlock( thread_data, id, true, message, filename, line, -1 );
            start( thread_data, timer );
        }
    }

    /*!
     * \brief  Function to stop profiling a block of code (advanced interface)
     * \details  This function stop profiling a block of code until a corresponding stop is called.
     *   It is recommended to use PROFILE_STOP(message) to call this routine.  It will
     *   automatically fill in the file name and the line number.
     *   Note: this is the advanced interface, we disable some error checking.
     * @param[in] id        Timer id (see get_timer_id for more info)
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param[in] filename  Name of the file containing the code
     * @param[in] line      Line number containing the start command
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     */
    inline void stop( uint64_t id, const char* message, const char* filename, int line, int level )
    {
        if ( level <= d_level && level >= 0 ) {
            auto end_time    = std::chrono::steady_clock::now();
            auto thread_data = getThreadData();
            auto timer       = getBlock( thread_data, id, true, message, filename, -1, line );
            stop( thread_data, timer, end_time );
        }
    }


    // Helper functions
    constexpr static inline const char* stripPath( const char* filename_in );
    constexpr static inline uint32_t hashString( const char* str );
    static inline uint32_t hashString( const std::string& str )
    {
        return hashString( str.c_str() );
    }
    static inline const char* getString( const std::string& str ) { return str.c_str(); }
    constexpr static inline const char* getString( const char* str ) { return str; }


public: // Constants to determine parameters that affect performance/memory
    // The maximum number of stored start and stop times per trace
    // Note: this is only used if store_trace is set, and should be a power of 2
    // Note: the maximum ammount of memory used per trace is 16*MAX_TRACE_TRACE bytes (plus the
    // trace itself)
    constexpr static size_t MAX_TRACE_TRACE = 1e6;

    // The maximum number of times to store the memory usage
    constexpr static size_t MAX_TRACE_MEMORY = 0x6000000;

    // The maximum number of threads supported
    constexpr static size_t MAX_THREADS = 1024;

    // The size of the hash table to store the timers
    constexpr static size_t TIMER_HASH_SIZE = 1024;


private: // Member classes
    //! Convience typedef for storing a point in time
    typedef std::chrono::time_point<std::chrono::steady_clock> time_point;

    // Structure to store the active trace data
    class StoreActive
    {
    public:
        StoreActive() { memset( this, 0, sizeof( *this ) ); }
        inline void set( size_t index );
        inline void unset( size_t index );
        inline StoreActive( const StoreActive& rhs ) = default;
        inline StoreActive& operator                 =( const StoreActive& rhs );
        inline StoreActive& operator&=( const StoreActive& rhs );
        inline bool operator==( const StoreActive& rhs ) { return hash == rhs.hash; }
        inline uint64_t id() const { return hash; }
        std::vector<uint32_t> getSet() const;

    private:
        // The maximum number of timers that will be checked for the trace logs
        // The actual number of timers is 64*TRACE_SIZE
        // Note: this only affects the trace logs, the number of timers is unlimited
        constexpr static size_t TRACE_SIZE = 64;
        // Store a hash id for fast access
        uint64_t hash;
        // Store the active trace
        uint64_t trace[TRACE_SIZE];
    };

    // Structure to store the info for a trace log
    struct store_trace {
        size_t N_calls;       // Number of calls to this block
        StoreActive trace;    // Store the trace
        store_trace* next;    // Pointer to the next entry in the list
        int64_t min_time;     // Store the minimum time spent in the given block (nano-seconds)
        int64_t max_time;     // Store the maximum time spent in the given block (nano-seconds)
        int64_t total_time;   // Store the total time spent in the given block (nano-seconds)
        size_t N_trace_alloc; // The size of the arrays for start_time and stop_time
        int64_t* start_time;  // Store when start was called for the given trace (nano-seconds from
                              // constructor call)
        int64_t* end_time;    // Store when stop was called for the given trace (nano-seconds from
                              // constructor call)
        // Constructor
        store_trace()
            : N_calls( 0 ),
              next( NULL ),
              min_time( std::numeric_limits<int64_t>::max() ),
              max_time( 0 ),
              total_time( 0 ),
              N_trace_alloc( 0 ),
              start_time( NULL ),
              end_time( NULL )
        {
        }
        // Destructor
        ~store_trace()
        {
            delete[] start_time;
            delete[] end_time;
            start_time = NULL;
            end_time   = NULL;
            delete next;
            next = NULL;
        }
        store_trace( const store_trace& rhs ) = delete;
        store_trace& operator=( const store_trace& rhs ) = delete;
    };

    // Structure to store the global timer information for a single block of code
    struct store_timer_data_info {
        int start_line;                       // The starting line for the timer
        int stop_line;                        // The ending line for the timer
        uint64_t id;                          // A unique id for each timer
        std::string message;                  // The message to identify the block of code
        std::string filename;                 // The file containing the block of code to be timed
        std::string path;                     // The path to the file (if availible)
        volatile store_timer_data_info* next; // Pointer to the next entry in the list
        // Constructor used to initialize key values
        store_timer_data_info() : start_line( -1 ), stop_line( -1 ), id( 0 ), next( NULL ) {}
        // Destructor
        ~store_timer_data_info()
        {
            delete next;
            next = NULL;
        }
        store_timer_data_info( const store_timer_data_info& rhs ) = delete;
        store_timer_data_info& operator=( const store_timer_data_info& rhs ) = delete;
    };

    // Structure to store the timing information for a single block of code
    struct store_timer {
        bool is_active;                    // Are we currently running a timer
        unsigned int trace_index;          // The index of the current timer in the trace
        int N_calls;                       // Number of calls to this block
        uint64_t id;                       // A unique id for each timer
        StoreActive trace;                 // Store the active trace
        int64_t min_time;                  // Store the minimum time spent in the given block (ns)
        int64_t max_time;                  // Store the maximum time spent in the given block (ns)
        int64_t total_time;                // Store the total time spent in the given block (ns)
        store_trace* trace_head;           // Head of the trace-log list
        store_timer* next;                 // Pointer to the next entry in the list
        store_timer_data_info* timer_data; // Pointer to the timer data
        time_point start_time;             // Store when start was called for the given block
        // Constructor used to initialize key values
        store_timer()
            : is_active( false ),
              trace_index( 0 ),
              N_calls( 0 ),
              id( 0 ),
              min_time( std::numeric_limits<int64_t>::max() ),
              max_time( 0 ),
              total_time( 0 ),
              trace_head( NULL ),
              next( NULL ),
              timer_data( NULL ),
              start_time( time_point() )
        {
        }
        // Destructor
        ~store_timer()
        {
            delete trace_head;
            delete next;
        }
        store_timer( const store_timer& rhs ) = delete;
        store_timer& operator=( const store_timer& rhs ) = delete;
    };

    // Structure to store thread specific information
    struct thread_info {
        int id;                             // The id of the thread
        unsigned int N_timers;              // The number of timers seen by the current thread
        StoreActive active;                 // Store the active trace
        store_timer* head[TIMER_HASH_SIZE]; // Store the timers in a hash table
        size_t N_memory_steps;              // The number of steps we have for the memory usage
        size_t N_memory_alloc; // The size of the arrays allocated for time_memory and size_memory
        int64_t* time_memory;  // The times at which we know the memory usage (ns from start)
        int64_t* size_memory;  // The memory usage at each time
        // Constructor used to initialize key values
        explicit thread_info( int id_ )
            : id( id_ ),
              N_timers( 0 ),
              N_memory_steps( 0 ),
              N_memory_alloc( 0 ),
              time_memory( nullptr ),
              size_memory( nullptr )
        {
            for ( size_t i = 0; i < TIMER_HASH_SIZE; i++ )
                head[i] = nullptr;
        }
        // Destructor
        ~thread_info()
        {
            for ( size_t i = 0; i < TIMER_HASH_SIZE; i++ ) {
                delete head[i];
                head[i] = nullptr;
            }
            delete[] time_memory;
            delete[] size_memory;
        }
        thread_info()                         = delete;
        thread_info( const thread_info& rhs ) = delete;
        thread_info& operator=( const thread_info& rhs ) = delete;
    };

private: // Member data
    typedef TimerUtility::atomic::int32_atomic int32_atomic;
    typedef TimerUtility::atomic::int64_atomic int64_atomic;

    // Store thread specific info
    volatile int32_atomic d_N_threads;
    thread_info* thread_table[MAX_THREADS];

    // Store the global timer info in a hash table
    volatile int N_timers;
    volatile store_timer_data_info* timer_table[TIMER_HASH_SIZE];

    // Handle to a mutex lock
    mutable std::mutex d_lock;

    // Misc variables
    bool d_store_trace_data;               // Store trace information?
    bool d_store_memory_data;              // Store memory information?
    bool d_disable_timer_error;            // Disable the timer errors for start/stop?
    int8_t d_level;                        // Timer level (default is 0, -1 is disabled)
    time_point d_construct_time;           // Constructor time
    int64_t d_shift;                       // Offset to synchronize the trace data
    mutable size_t d_max_trace_remaining;  // The number of traces remaining
    mutable size_t d_N_memory_steps;       // The number of steps for memory usage
    mutable int64_t* d_time_memory;        // Times at which we know the memory usage (ns)
    mutable int64_t* d_size_memory;        // The memory usage at each time
    mutable volatile int64_atomic d_bytes; // The current memory used by the profiler


private: // Private member functions
    // Function to return a pointer to the thread info (or create it if necessary)
    // Note: this function does not require any blocking
    inline thread_info* getThreadData()
    {
        thread_local int id = TimerUtility::atomic::atomic_increment( &d_N_threads ) - 1;
        if ( !thread_table[id] )
            thread_table[id] = new thread_info( id );
        return thread_table[id];
    }

    // Function to return a pointer to the global timer info (or create it if necessary)
    // Note: this function may block for thread safety
    store_timer_data_info* getTimerData(
        uint64_t id, const char* message, const char* filename, int start, int stop );

    // Function to return the appropriate timer block
    inline store_timer* getBlock( thread_info* thread_data, uint64_t id, bool create = false,
        const char* message = nullptr, const char* filename = nullptr, const int start = -1,
        const int stop = -1 );

    // Function to get the timer results
    inline void getTimerResultsID( uint64_t id, std::vector<const thread_info*>& threads, int rank,
        const time_point& end_time, TimerResults& results ) const;

    // Start/stop the timer
    void start( thread_info* thread, store_timer* timer );
    void stop( thread_info* thread, store_timer* timer,
        time_point end_time = std::chrono::steady_clock::now() );
    void activeErrStart( thread_info*, store_timer* );
    void activeErrStop( thread_info*, store_timer* );

    // Function to return the string of active timers
    static std::vector<id_struct> getActiveList(
        const StoreActive& active, unsigned int myIndex, const thread_info* head );

    // Function to get the current memory usage
    static inline size_t getMemoryUsage();

    // Functions to load files
    static int loadFiles( const std::string& filename, int index, TimerMemoryResults& data );
    static void loadTimer( const std::string& filename, std::vector<TimerResults>& timers,
        int& N_procs, std::string& date, bool& load_trace, bool& load_memory );
    static void loadTrace( const std::string& filename, std::vector<TimerResults>& timers );
    static void loadMemory( const std::string& filename, std::vector<MemoryResults>& memory );

    // Functions to send all timers/memory to rank 0
    static void gatherTimers( std::vector<TimerResults>& timers );
    static void addTimers(
        std::vector<TimerResults>& timers, const std::vector<TimerResults>& add );
    static void gatherMemory( std::vector<MemoryResults>& memory );


protected: // Friends
    friend ScopedTimer;
};


// The global profiler
extern ProfilerApp global_profiler;


#include "ProfilerApp.hpp"
#include "ProfilerAppClasses.h"
#include "ProfilerAppMacros.h"


#endif
