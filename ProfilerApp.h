#ifndef included_ProfilerApp
#define included_ProfilerApp


#include <array>
#include <atomic>
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
#include <string_view>
#include <vector>

#include "ProfilerDefinitions.h"


/** \class uint16f
 *
 * Class to store an unsigned integer as a half precision floating type.
 */
class uint16f
{
public:
    // Constructors
    constexpr uint16f() : data( 0 ) {}
    explicit inline constexpr uint16f( uint64_t x ) : data( getData( x ) ) {}
    // Comparison operators
    inline constexpr bool operator==( const uint16f& rhs ) const { return data == rhs.data; }
    inline constexpr bool operator!=( const uint16f& rhs ) const { return data != rhs.data; }
    inline constexpr bool operator>=( const uint16f& rhs ) const { return data >= rhs.data; }
    inline constexpr bool operator<=( const uint16f& rhs ) const { return data <= rhs.data; }
    inline constexpr bool operator>( const uint16f& rhs ) const { return data > rhs.data; }
    inline constexpr bool operator<( const uint16f& rhs ) const { return data < rhs.data; }
    // Overload typecast
    inline constexpr operator uint64_t() const;

private:
    uint16_t data;
    static inline constexpr uint16_t getData( uint64_t x );
};


/** \class id_struct
 *
 * Structure to store id string
 */
class id_struct
{
public:
    // Constructors
    constexpr id_struct() : data( 0 ) {}
    constexpr explicit id_struct( uint64_t id ) : data( id ) {}
    explicit id_struct( const std::string& rhs );
    explicit id_struct( std::string_view rhs );
    explicit id_struct( const char* rhs );
    // Comparison operators
    constexpr inline bool operator==( const id_struct& rhs ) const { return data == rhs.data; }
    constexpr inline bool operator!=( const id_struct& rhs ) const { return data != rhs.data; }
    constexpr inline bool operator>=( const id_struct& rhs ) const { return data >= rhs.data; }
    constexpr inline bool operator<=( const id_struct& rhs ) const { return data <= rhs.data; }
    constexpr inline bool operator>( const id_struct& rhs ) const { return data > rhs.data; }
    constexpr inline bool operator<( const id_struct& rhs ) const { return data < rhs.data; }
    // Return null terminated string stored in a std::array
    std::array<char, 12> str() const;
    // Return a std::string
    inline std::string string() const { return std::string( str().data() ); }
    // Overload typecast
    inline constexpr operator uint64_t() const { return data; }

private:
    uint64_t data;
};


/** \class TraceResults
 *
 * Structure to store results of a single trace from the profiler application.
 * Note: field types and sizes are set to minimize the storage requirements
 *    of this structure (currently 48 bytes without active timers or trace profiling).
 */
class TraceResults
{
public:
    id_struct id;     //!<  ID of parent timer
    uint16_t thread;  //!<  Active thread
    uint32_t rank;    //!<  Rank
    uint32_t N_trace; //!<  Number of calls that we trace
    float min;        //!<  Minimum call time (ns)
    float max;        //!<  Maximum call time (ns)
    float tot;        //!<  Total call time (ns)
    uint64_t N;       //!<  Total number of calls
    uint64_t stack;   //!<  Hash value of the stack trace
    uint64_t stack2;  //!<  Hash value of the stack trace (including this call)
    uint16f* times;   //!<  Start/stop times for each call (N_trace)
public:
    // Constructors/destructor
    TraceResults();
    ~TraceResults();
    TraceResults( const TraceResults& ) = delete;
    TraceResults( TraceResults&& );
    TraceResults& operator=( const TraceResults& ) = delete;
    TraceResults& operator=( TraceResults&& );
    // Helper functions
    size_t size( bool store_trace = true ) const; //!< The number of bytes needed to pack the data
    size_t pack( char* data, bool store_trace = true ) const; //!<  Pack the data to a buffer
    size_t unpack( const char* data );                        //!<  Unpack the data from a buffer
    bool operator==( const TraceResults& rhs ) const;         //! Comparison operator
    inline bool operator!=( const TraceResults& rhs ) const { return !( this->operator==( rhs ) ); }
};


/** \class TimerResults
 *
 * Structure to store results of a single timer from the profiler application.
 * Note: The size of this class is currently 232 bytes (without any trace data).
 */
class TimerResults
{
public:
    id_struct id;                    //!<  Timer ID
    int line;                        //!<  Timer line
    char message[64];                //!<  Timer message (null terminated string)
    char file[64];                   //!<  Timer file (null terminated string)
    char path[64];                   //!<  Timer file path (null terminated string)
    std::vector<TraceResults> trace; //!<  Trace data
    // Constructors/destructor
    TimerResults();
    TimerResults( const TimerResults& )            = delete;
    TimerResults( TimerResults&& )                 = default;
    TimerResults& operator=( const TimerResults& ) = delete;
    TimerResults& operator=( TimerResults&& )      = default;
    // Helper functions
    size_t size( bool store_trace = true ) const; //!<  The number of bytes needed to pack the trace
    size_t pack( char* data, bool store_trace = true ) const; //!<  Pack the data to a buffer
    size_t unpack( const char* data );                        //!<  Unpack the data from a buffer
    bool operator==( const TimerResults& rhs ) const;         //!<  Comparison operator
    inline bool operator!=( const TimerResults& rhs ) const { return !( this->operator==( rhs ) ); }
};


/** \class MemoryResults
 *
 * Structure to store the memory results of a single rank.
 */
struct MemoryResults {
    int rank;                          //!<  Rank
    std::vector<uint64_t> time;        //!<  Time (ns)
    std::vector<uint64_t> bytes;       //!<  Memory in use
    size_t size() const;               //!<  The number of bytes needed to pack the trace
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
    double walltime;                   //!< The walltime elapsed during run
    std::vector<TimerResults> timers;  //!< The timer results
    std::vector<MemoryResults> memory; //!< The memory results
    size_t size() const;               //!< The number of bytes needed to pack the trace
    size_t pack( char* data ) const;   //!<  Pack the data to a buffer
    size_t unpack( const char* data ); //!<  Unpack the data from a buffer
    TimerMemoryResults() : N_procs( 0 ), walltime( 0 ) {}
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
 *    PROFILE(NAME) - Start profiling a block of code with the given name.
 *    PROFILE2(NAME) - Stop profiling a block of code with the given name.
 *                          The name must match the given start block.
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
 * For repeated calls the timer adds 100-200 ns per call.
 * The resolution is ~ 50 ns for a single timer call. <BR>
 * Example usage: \verbatim 
 *    void my_function(void *arg) {
 *       PROFILE("my function");
 *       int k;
 *       for (int i=0; i<10; i++) {
 *          PROFILE("loop");
 *          // Do some work
 *       }
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
    //! This is a singleton class
    ProfilerApp* getInstantance();

    //! Constructor
    ProfilerApp();

    //! Destructor
    ~ProfilerApp();

    /*!
     * \brief  Function to start profiling a block of code (advanced interface)
     * \details  This function starts profiling a block of code until a corresponding stop
     *   is called. It is recommended to use PROFILE_START(message) to call this routine.
     *   It will automatically fill in the file name and the line number.
     *   Note: this is the advanced interface, we disable some error checking.
     * @param[in] id        Timer id (see getTimerId for more info)
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param[in] filename  Name of the file containing the code
     * @param[in] line      Line number containing the start command
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whose level is <= the level will be included.
     */
    inline void start( uint64_t id, const char* message = "", const char* filename = "",
        int line = 0, int level = 0 )
    {
        if ( level <= d_level && level >= 0 ) {
            auto timer = getBlock( id, message, filename, line );
            start( timer );
        }
    }

    /*!
     * \brief  Function to stop profiling a block of code (advanced interface)
     * \details  This function stop profiling a block of code until a corresponding stop is called.
     *   It is recommended to use PROFILE_STOP(message) to call this routine.  It will
     *   automatically fill in the file name and the line number.
     *   Note: this is the advanced interface, we disable some error checking.
     * @param[in] id        Timer id (see getTimerId for more info)
     * @param[in] message   Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param[in] filename  Name of the file containing the code
     * @param[in] line      Line number containing the start command
     * @param[in] level     Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level will be included.
     * @param[in] trace     Store trace-level data for the timer:
     *                      -1: Default, use the global trace flag
     *                       0: Disable trace data for this timer
     *                       1: Enable trace data for this timer
     */
    inline void stop( uint64_t id, int level = 0, int trace = -1 )
    {
        if ( level <= d_level && level >= 0 ) {
            auto end_time = std::chrono::steady_clock::now();
            auto timer    = getBlock( id );
            stop( timer, end_time, trace );
        }
    }

    /*!
     * \brief  Function to add the current memory to the trace
     * \details  This function will get the current memory usage and add it to the profile
     *    assuming memory trace is enabled.
     */
    void memory();

    /*!
     * \brief  Function to save the profiling info
     * \details  This will save the current timer info.  This is a non-blocking function.
     * Note: .x.timer will automatically be appended to the filename, where x is the rank+1 of the
     * process. Note: .x.trace will automatically be appended to the filename when detailed traces
     * are used.
     * @param[in] filename  File name for saving the results
     * @param[in] global    Save a global file (true) or individual files (false)
     */
    void save( const std::string& filename, bool global = true ) const;

    /*!
     * \brief  Function to load the profiling info
     * \details  This will load the timing and trace info from a file
     * @param[in] filename  File name for loading the results
     *                      Note: .x.timer will be automatically appended to the filename
     * @param[in] rank      Rank to load (-1: all ranks)
     * @param[in] global    Save the time results in a global file (default is false)
     */
    static TimerMemoryResults load(
        const std::string& filename, int rank = -1, bool global = true );

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
     *                      Only timers whose level is <= the level will be included.
     */
    void enable( int level = 0 );

    //! Function to disable the timers (all current timers will be deleted)
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

    //! Enum defining the level of memory detail
    enum class MemoryLevel : int8_t { None = 0, Pause = 1, Fast = 2, Full = 3 };

    /*!
     * \brief  Function to change if we are storing memory information
     * \details  This function will change if we are storing information about the memory usage
     *    as a function of time (must be called before any start).
     *    If the level is set to Fast, it will only track new/delete calls
     *    (requires overloading new/delete).  If the level is set to Full, it will attempt
     *    to count all memory usage.
     *  Note: Enabling this option will check the memory usage every time we enter or leave
     *    timer.  This data will be combined from all timers/threads to get the memory usage
     *    of the application over time.  Combined with the trace level data, we can determine
     *    when memory is allocated and which timers are active.
     * @param[in] memory    Do we want to store detailed profiling data
     */
    void setStoreMemory( MemoryLevel level = MemoryLevel::Fast );


    //! Get the current memory level
    MemoryLevel getStoreMemory();

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
     *     Many routines can take the timer id directly to improve performance by
     *     avoiding the hashing function.
     * @param[in] message   The timer message
     * @param[in] filename  The filename
     * @param[in] line      The line number
     */
    constexpr static inline uint64_t getTimerId(
        const char* message, const char* filename, int line );

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
     * \brief  Function to return the current timer results
     * \details  This function will return a vector containing the
     *      current timing results for all threads.
     * @param[in] message   The timer message
     * @param[in] filename  The filename
     */
    TimerResults getTimerResults( std::string_view message, std::string_view file ) const;

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

    //! Build active stack map
    static std::tuple<std::vector<uint64_t>, std::vector<std::vector<uint64_t>>> buildStackMap(
        const std::vector<TimerResults>& timers );


public: // Helper functions
    constexpr static inline std::string_view stripPath( std::string_view filename );
    constexpr static inline uint64_t hashString( std::string_view str );

public: // Constants to determine parameters that affect performance/memory
    // The size of the hash table to store the timers/ (must be a power of 2)
    constexpr static uint64_t HASH_SIZE = 1024;


public: // Member classes
    //! Convience typedef for storing a point in time
    typedef std::chrono::time_point<std::chrono::steady_clock> time_point;

    // Structure to store a sorted list of times (future work: unsigned LEB128)
    class StoreTimes
    {
    public:
        StoreTimes();
        ~StoreTimes();
        StoreTimes( const StoreTimes& rhs )            = delete;
        StoreTimes& operator=( const StoreTimes& rhs ) = delete;
        explicit StoreTimes( const StoreTimes& rhs, uint64_t shift );
        inline void reserve( size_t N );
        inline size_t size() const { return d_size; }
        inline void add( uint64_t start, uint64_t stop );
        inline const uint16f* begin() const { return d_data; }
        inline const uint16f* end() const { return &d_data[2 * d_size]; }
        inline uint16f* take();

    private:
        // The maximum number of entries to store (we need 4 bytes/entry)
        constexpr static size_t MAX_TRACE = 1e6;
        // Internal data
        size_t d_capacity; // Capacity of d_data
        size_t d_size;     // Number of entries stored
        uint64_t d_offset; // Current offset
        uint16f* d_data;   // Data to store compressed data
    };

    // Structure to store memory usage
    class StoreMemory
    {
    public:
        StoreMemory();
        ~StoreMemory();
        inline StoreMemory( const StoreMemory& rhs )            = delete;
        inline StoreMemory& operator=( const StoreMemory& rhs ) = delete;
        inline void add(
            uint64_t time, MemoryLevel level, volatile std::atomic_int64_t& bytes_profiler );
        void reset() volatile;
        void swap( StoreMemory& rhs );
        void get( std::vector<uint64_t>& time, std::vector<uint64_t>& bytes ) const volatile;

    private:
        // The maximum number of memory traces allowed
        constexpr static size_t MAX_ENTRIES = 0x6000000;
        // Internal data
        size_t d_capacity;
        size_t d_size;
        uint64_t* d_time;  // The times at which we know the memory usage (ns from start)
        uint64_t* d_bytes; // The memory usage at each time
    };

    // Structure to store the info for a trace log
    struct store_trace {
        uint64_t start;      // Store when start was called for the given block
        uint64_t stack;      // Store the calling stack hash
        uint64_t stack2;     // Store stack including self
        uint64_t N_calls;    // Number of calls to this block
        uint64_t min_time;   // Store the minimum time spent in the given block (nano-seconds)
        uint64_t max_time;   // Store the maximum time spent in the given block (nano-seconds)
        uint64_t total_time; // Store the total time spent in the given block (nano-seconds)
        StoreTimes times;    // Store when start/stop was called (nano-seconds from constructor)
        store_trace* next;   // Store the next trace
        store_trace( uint64_t stack = 0 );
        ~store_trace();
        store_trace( const store_trace& rhs )            = delete;
        store_trace& operator=( const store_trace& rhs ) = delete;
    };

    // Structure to store the global timer information for a single block of code
    struct store_timer_data_info {
        int line;                             // The line number for the timer
        uint64_t id;                          // A unique id for each timer
        char message[64];                     // The message to identify the block of code
        char filename[64];                    // The file containing the block of code to be timed
        char path[64];                        // The path to the file (if availible)
        volatile store_timer_data_info* next; // Pointer to the next entry in the list
        store_timer_data_info();
        store_timer_data_info(
            std::string_view msg, std::string_view filepath, uint64_t id, int start );
        ~store_timer_data_info();
        store_timer_data_info( const store_timer_data_info& rhs )            = delete;
        store_timer_data_info& operator=( const store_timer_data_info& rhs ) = delete;
        bool compare(
            const store_timer_data_info& rhs ) const; // True if message and filename match
    };

    // Structure to store the timing information for a single block of code
    struct store_timer {
        uint64_t id;                       // A unique id for each timer
        store_trace* trace_head;           // Pointer to the first trace
        store_timer* next;                 // Pointer to the next entry in the list
        store_timer_data_info* timer_data; // Pointer to the timer data
        store_timer();
        ~store_timer();
        store_timer( const store_timer& rhs )            = delete;
        store_timer& operator=( const store_timer& rhs ) = delete;
    };

    // Structure to store thread specific data
    struct ThreadData {
        uint32_t id;               // A unique id for each thread
        uint32_t depth;            // Stack depth
        uint64_t stack;            // Current stack hash
        uint64_t hash;             // std::hash of std::thread::id
        volatile ThreadData* next; // Pointer to the next entry in the list
        store_timer* timers[HASH_SIZE];
        StoreMemory memory;
        ThreadData();
        ~ThreadData();
        ThreadData( const ThreadData& ) = delete;
        void reset() volatile;
    };

public: // Advanced interfaces, not intended for users
    store_trace* start( store_timer* timer );
    void stop( store_timer* timer, time_point end_time, int enableTrace );
    void stop( store_trace* trace, time_point end_time, int enableTrace );
    inline store_timer* getBlock( uint64_t id );
    inline store_timer* getBlock(
        uint64_t id, const char* message, const char* filename, const int line );

private: // Member data
    using ThreadDataPtr = volatile ThreadData*;
    using TimerDataPtr  = volatile store_timer_data_info*;
    ThreadDataPtr volatile d_threadData[HASH_SIZE]; // Store thread specific info
    TimerDataPtr d_timer_table[HASH_SIZE];          // Store the global timer info
    mutable std::mutex d_lock;                      // Handle to a mutex lock
    bool d_store_trace_data;                        // Store trace information (default value)?
    MemoryLevel d_store_memory_data;                // Store memory information?
    bool d_disable_timer_error;                     // Disable the timer errors for start/stop?
    int8_t d_level;                                 // Timer level (default is 0, -1 is disabled)
    time_point d_construct_time;                    // Constructor time
    uint64_t d_shift;                               // Offset to synchronize the trace data
    mutable volatile std::atomic_int64_t d_bytes;   // The current memory used by the profiler

private: // Private member functions
    ProfilerApp( const ProfilerApp& )            = delete;
    ProfilerApp& operator=( const ProfilerApp& ) = delete;

    // Function to get a global thread id
    static inline ThreadData* getThreadData()
    {
        static thread_local ThreadData* data = createThreadData();
        return data;
    }
    static ThreadData* createThreadData();

    // Function to return a pointer to the global timer info (or create it if necessary)
    // Note: this function may block for thread safety
    store_timer_data_info* getTimerData(
        uint64_t id, std::string_view message, std::string_view filename, int line );

    // Function to get the timer results
    inline void getTimerResultsID(
        uint64_t id, int rank, const time_point& end_time, TimerResults& results ) const;

    // Function to get the current memory usage
    static inline size_t getMemoryUsage();

    // Functions to load files
    static int loadFiles( const std::string& filename, int index, TimerMemoryResults& data );
    static void loadTimer( const std::string& filename, std::vector<TimerResults>& timers,
        int& N_procs, double& walltime, std::string& date, bool& load_trace, bool& load_memory );
    static void loadTrace( const std::string& filename, std::vector<TimerResults>& timers );
    static void loadMemory( const std::string& filename, std::vector<MemoryResults>& memory );

    // Functions to send all timers/memory to rank 0
    static void gatherTimers( std::vector<TimerResults>& timers );
    static void addTimers( std::vector<TimerResults>& timers, std::vector<TimerResults>&& add );
    static void gatherMemory( std::vector<MemoryResults>& memory );

    // Error processing
    void error( const std::string& message, const ThreadData* thread, const store_timer* timer );
};


// The global profiler
extern ProfilerApp global_profiler;


#include "ProfilerApp.hpp"
#include "ProfilerAppClasses.h"
#include "ProfilerAppMacros.h"


#endif
