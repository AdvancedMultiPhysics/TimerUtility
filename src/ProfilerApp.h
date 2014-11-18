#ifndef included_ProfilerApp
#define included_ProfilerApp


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <vector>


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    #define USE_WINDOWS
#elif defined(__APPLE__)
    #define USE_MAC
#else
    #define USE_LINUX
#endif


#ifdef USE_WINDOWS
    // Windows
    #define NOMINMAX                    // Supress min max from being defined
    #include <windows.h>
    #include <string>
    #define TIME_TYPE LARGE_INTEGER
#elif defined(USE_MAC)
    // Mac
    #include <sys/time.h>
    #include <pthread.h>
    #include <string.h>
    #define TIME_TYPE timeval
#elif defined(USE_LINUX)
    // Linux
    #include <sys/time.h>
    #include <pthread.h>
    #include <string.h>
    #define TIME_TYPE timeval
#else
    #error Unknown OS
#endif


#define TRACE_SIZE 64                           // The maximum number of timers that will be checked for the trace logs
                                                // The actual number of timers is TRACE_SIZE * number of bits of size_t
                                                // Note: this only affects the trace logs, the number of timers is unlimited
#define MAX_TRACE_TRACE 1e6                     // The maximum number of stored start and stop times per trace
                                                // Note: this is only used if store_trace is set, and should be a power of 2
                                                // Note: the maximum ammount of memory used per trace is 16*MAX_TRACE_TRACE bytes (plus the trace itself)
#define MAX_TRACE_MEMORY 0x6000000              // The maximum number of times to store the memory usage
#define THREAD_HASH_SIZE 64                     // The size of the hash table to store the threads
#define TIMER_HASH_SIZE 1024                    // The size of the hash table to store the timers


/** \class id_struct
  *
  * Structure to store id string
  */
struct id_struct {
    id_struct( ) { data.u64=0; }
    id_struct(const id_struct& rhs) { data.u64=rhs.data.u64; }
    id_struct& operator=(const id_struct& rhs) { this->data.u64=rhs.data.u64; return *this; }
    id_struct(const std::string& rhs) { data.u64=0; rhs.copy(data.str,8); }
    id_struct(const char* rhs) { data.u64=0; for (int i=0; i<8&&rhs[i]>0; i++) data.str[i]=rhs[i]; }
    inline const char* c_str( ) const { return data.str; }
    inline const std::string string( ) const { return std::string(data.str,0,8); }
    inline bool operator==(const id_struct& rhs ) const { return data.u64==rhs.data.u64; }
    inline bool operator!=(const id_struct& rhs ) const { return data.u64!=rhs.data.u64; }
    inline bool operator>=(const id_struct& rhs ) const { return data.u64>=rhs.data.u64; }
    inline bool operator> (const id_struct& rhs ) const { return data.u64> rhs.data.u64; }
    inline bool operator< (const id_struct& rhs ) const { return data.u64< rhs.data.u64; }
    inline bool operator<=(const id_struct& rhs ) const { return data.u64<=rhs.data.u64; }
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
    id_struct id;                       //!<  ID of parent timer
    short unsigned int N_active;        //!<  Number of active timers
    short unsigned int thread;          //!<  Active thread
    unsigned int rank;                  //!<  Rank
    unsigned int N_trace;               //!<  Number of calls that we trace
    float min;                          //!<  Minimum call time
    float max;                          //!<  Maximum call time
    float tot;                          //!<  Total call time
    size_t N;                           //!<  Total number of calls
    id_struct* active();                //!<  List of active timers
    const id_struct* active() const;    //!<  List of active timers
    double* start();                    //!<  Start times for each call
    const double* start() const;        //!<  Start times for each call
    double* stop();                     //!<  Stop times for each call
    const double* stop() const;         //!<  Stop times for each call
    TraceResults( );                    //!<  Empty constructor
    ~TraceResults( );                   //!<  Destructor
    TraceResults(const TraceResults&);  //!<  Copy constructor
    TraceResults& operator=(const TraceResults&); //! Assignment operator
    void allocate();                    //!<  Allocate the data
    size_t size(bool store_trace=true) const; //!< The number of bytes needed to pack the trace
    void pack( void* data ) const;      //!<  Pack the data to a buffer
    void unpack( const void* data );    //!<  Unpack the data from a buffer
private:
    void *mem;
};


/** \class TimerResults
  *
  * Structure to store results of a single timer from the profiler application.
  */
struct TimerResults {
    id_struct id;                       //!<  Timer ID
    std::string message;                //!<  Timer message
    std::string file;                   //!<  Timer file
    std::string path;                   //!<  Timer file path
    int start;                          //!<  Timer start line (-1: never defined)
    int stop;                           //!<  Timer stop line (-1: never defined)
    std::vector<TraceResults> trace;    //!< Trace data
    size_t size(bool store_trace=true) const; //!< The number of bytes needed to pack the trace
    void pack( void* data ) const;      //!<  Pack the data to a buffer
    void unpack( const void* data );    //!<  Unpack the data from a buffer
};


/** \class MemoryResults
  *
  * Structure to store the memory results of a single rank.
  */
struct MemoryResults{
    int rank;                           //!<  Rank
    std::vector<double> time;           //!<  Time
    std::vector<size_t> bytes;          //!<  Memory in use
    size_t size() const;                //!< The number of bytes needed to pack the trace
    void pack( void* data ) const;      //!<  Pack the data to a buffer
    void unpack( const void* data );    //!<  Unpack the data from a buffer
};


/** \class TimerMemoryResults
  *
  * Structure to store results of timers and memory
  */
struct TimerMemoryResults {
    int N_procs;
    std::vector<TimerResults> timers;
    std::vector<MemoryResults> memory;
};


/** \class ProfilerApp
  *
  * This class provides some basic timing and profiling capabilities.
  * It works be providing start, stop, and save functions that allow the user to record 
  * the total time required for blocks of code.  The results are written to an ASCII data 
  * file that can be viewed directly or processed.  It is compatible with MPI and SAMRAI.
  * The overhead of the function is ~1us per call for start and stop.  the time for save 
  * depends of the amount of data to be written and the speed of the writes.  Additional 
  * details can be provided with set_store_trace which records the actual time (from startup)
  * for each start and stop call.  This method significantly increases the memory requirements.
  * Several preprocessor define statement are given for easy incorporation: \verbatim
  *    PROFILE_START(NAME) - Start profiling a block of code with the given name
  *                          The name must be unique to the file and there must be a corresponding stop statement.
  *    PROFILE_STOP(NAME)  - Stop profiling a block of code with the given name
  *                          The name must match the given start block, and there must only be one PROFILE_STOP for each 
  *                          PROFILE_START.  This records the current line number as the final line number for the block of code.
  *    PROFILE_STOP2(NAME) - Stop profiling a block of code with the given name
  *                          The name must match the given start block.  This is a special stop that does not use the current 
  *                          line as the final line of the block, but is provided for cases where there are multiple exit
  *                          paths for a given function or block of code.
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
  * All start/stop and enable support an optional argument level that specifies the level of detail 
  * for the timers.  All timers with a number greater than the current level in the profiler will be ignored.
  * The macros PROFILE_START and PROFILE_STOP automatically check the level for performance and calling an
  * unused timer adds ~10ns per call. <BR>
  * For repeated calls the timer adds ~ 1us per call with without trace info, and ~1-10us per call with full trace info. 
  * Most of this overhead is not in the time returned by the timer.  The resolution is ~ 1us for a single timer call.
  * Note that when a timer is created the cost may be significantly higher, but this only occurs once per timer.  <BR>
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
class ProfilerApp {
public:
    //! Constructor
    ProfilerApp( );

    //! Destructor
    ~ProfilerApp();

    /*!
     * \brief  Function to start profiling a block of code
     * \details  This function starts profiling a block of code until a corresponding stop is called.
     *   It is recommended to use PROFILE_START(message) to call this routine.  It will 
     *   automatically fill in the file name and the line number.  
     * @param message       Message to uniquely identify the block of code being profiled.
     *                      It must be a unique message to all start called within the same file.
     * @param filename      Name of the file containing the code
     * @param line          Line number containing the start command
     * @param level         Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level of the specified by enable will be included.
     * @param id            Optional id for the timer (helps improve performance).  See get_timer_id for more info.
     */
    void start( const std::string& message, const char* filename, const int line, const int level=0, const size_t id=0 );

    /*!
     * \brief  Function to stop profiling a block of code
     * \details  This function stop profiling a block of code until a corresponding stop is called.
     *   It is recommended to use PROFILE_STOP(message) to call this routine.  It will 
     *   automatically fill in the file name and the line number.  
     * @param message       Message to uniquely identify the block of code being profiled.
     *                      It must match a start call.
     * @param filename      Name of the file containing the code
     * @param line          Line number containing the stop command
     * @param level         Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level of the specified by enable will be included.
     *                      Note: this must match the level in start
     * @param id            Optional id for the timer (helps improve performance).  See get_timer_id for more info.
     */
    void stop( const std::string& message, const char* filename, const int line, const int level=0, const size_t id=0 );

    /*!
     * \brief  Function to check if a timer is active
     * \details  This function checks if a given timer is active on the current thread.
     * @param message       Message to uniquely identify the block of code being profiled.
     *                      It must match a start call.
     * @param filename      Name of the file containing the code
     * @param id            Optional id for the timer (helps improve performance).  See get_timer_id for more info.
     */
    bool active( const std::string& message, const char* filename, const size_t id=0 );

    /*!
     * \brief  Function to save the profiling info
     * \details  This will save the current timer info.  This is a non-blocking function.
     * Note: .x.timer will automatically be appended to the filename, where x is the rank+1 of the process.
     * Note: .x.trace will automatically be appended to the filename when detailed traces are used.
     * @param filename      File name for saving the results
     * @param global        Save a global file (true) or individual files (false)
     */
    void save( const std::string& filename, bool global=false ) const;

    /*!
     * \brief  Function to load the profiling info
     * \details  This will load the timing and trace info from a file
     * @param filename      File name for loading the results
     *                      Note: .x.timer will be automatically appended to the filename
     * @param rank          Rank to load (-1: all ranks)
     */
    static TimerMemoryResults load( const std::string& filename, int rank=-1 );

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
     * \details  This function will enable the current timer clase.  It supports an optional level argument
     * that specifies the level of detail to use for the timers. 
     * @param level         Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level of the specified by enable will be included.
     */
    void enable( int level=0 );

    //! Function to enable the timers (all current timers will be deleted)
    void disable( );

    /*!
     * \brief  Function to change if we are storing detailed trace information
     * \details  This function will change if we are storing detailed trace information (must be called before any start)
     *  Note: Enabling this option will store the starting and ending time for each call.
     *  This will allow the user to look at the detailed results to get trace information.
     *  However this will significantly increase the memory requirements for any traces
     *  that get called repeatedly and may negitivly impact the performance.
     * @param profile       Do we want to store detailed profiling data
     */
    void set_store_trace(bool profile=false);

    /*!
     * \brief  Function to change if we are storing memory information
     * \details  This function will change if we are storing information about the memory usage
     *  as a function of time (must be called before any start).
     *  Note: Enabling this option will check the memory usage evergy time we enter or leave
     *  timer.  This data will be combined from all timers/threads to get the memory usage
     *  of the application over time.  Combined with the trace level data, we can determine
     *  when memory is allocated and which timers are active.
     * @param memory        Do we want to store detailed profiling data
     */
    void set_store_memory(bool memory=false);

    //! Return the current timer level
    inline int get_level( ) const { return d_level; }

    /*!
     * \brief  Function to change the behavior of timer errors
     * \details  This function controls the behavior of the profiler when we encounter a timer
     *   error.  The default behavior is to abort.  Timer errors include starting a timer
     *   that is already started, or stopping a timer that is not running.
     *   The user should only disable theses checks if they understand the behavior.  
     * @param flag        Do we want to ignore timer errors
     */
    void ignore_timer_errors(bool flag=false) { d_check_timer_error = flag; }

    /*!
     * \brief  Function to get the timer id
     * \details  This function returns the timer id given the message and filename
     * @param message     The timer message
     * @param filename    The filename
     */
    static size_t get_timer_id( const char* message, const char* filename );

    /*!
     * \brief  Function to return the current timer results
     * \details  This function will return a vector containing the 
     *   current timing results for all threads.
     */
    std::vector<TimerResults> getTimerResults() const;

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
    size_t getMemoryUsed() const { return static_cast<size_t>(d_bytes); }

private:

    // Protect against copy of the class
    ProfilerApp( const ProfilerApp& );


    // Structure to store the info for a trace log
    struct store_trace {
        size_t N_calls;             // Number of calls to this block
        size_t id;                  // This is a (hopefully) unique id that we can use for comparison
        size_t trace[TRACE_SIZE];   // Store the trace
        store_trace *next;          // Pointer to the next entry in the list
        double min_time;            // Store the minimum time spent in the given block (seconds)
        double max_time;            // Store the maximum time spent in the given block (seconds)
        double total_time;          // Store the total time spent in the given block (seconds)
        size_t N_trace_alloc;       // The size of the arrays for start_time and stop_time
        double *start_time;         // Store when start was called for the given trace (seconds from constructor call)
        double *end_time;           // Store when stop was called for the given trace (seconds from constructor call)
        // Constructor
        store_trace(): N_calls(0), id(0), next(NULL), min_time(1e100), max_time(0), 
            total_time(0), N_trace_alloc(0), start_time(NULL), end_time(NULL) {
            memset(trace,0,TRACE_SIZE*sizeof(size_t));
        }
        // Destructor
		~store_trace() {
            delete [] start_time;
            delete [] end_time;
            start_time = NULL;
            end_time = NULL;
            delete next;
            next = NULL;
		}
      private:
        store_trace( const store_trace& rhs );              // Private copy constructor
        store_trace& operator=( const store_trace& rhs );   // Private assignment operator
    };
    
    // Structure to store the global timer information for a single block of code
    struct store_timer_data_info {
        int start_line;                     // The starting line for the timer
        int stop_line;                      // The ending line for the timer
        size_t id;                          // A unique id for each timer
        std::string message;                // The message to identify the block of code
        std::string filename;               // The file containing the block of code to be timed
        std::string path;                   // The path to the file (if availible)
        volatile store_timer_data_info *next; // Pointer to the next entry in the list
        // Constructor used to initialize key values
		store_timer_data_info(): start_line(-1), stop_line(-1), id(0), next(NULL) {}
        // Destructor
		~store_timer_data_info() {
            delete next;
            next = NULL;
		}
      private:
        store_timer_data_info( const store_timer_data_info& rhs );              // Private copy constructor
        store_timer_data_info& operator=( const store_timer_data_info& rhs );   // Private assignment operator
    };

    // Structure to store the timing information for a single block of code
    struct store_timer {
        bool is_active;                     // Are we currently running a timer
        unsigned int trace_index;           // The index of the current timer in the trace
        int N_calls;                        // Number of calls to this block
        size_t id;                          // A unique id for each timer
        size_t trace[TRACE_SIZE];           // Store the current trace
        double min_time;                    // Store the minimum time spent in the given block (seconds)
        double max_time;                    // Store the maximum time spent in the given block (seconds)
        double total_time;                  // Store the total time spent in the given block (seconds)
        store_trace *trace_head;            // Head of the trace-log list
        store_timer *next;                  // Pointer to the next entry in the list
        store_timer_data_info *timer_data;  // Pointer to the timer data
        TIME_TYPE start_time;               // Store when start was called for the given block
        // Constructor used to initialize key values
		store_timer() {
            memset(this,0,sizeof(store_timer));
		}
        // Destructor 
		~store_timer() {
            delete trace_head;
            delete next;
            memset(this,0,sizeof(store_timer));
		}
      private:
        store_timer( const store_timer& rhs );              // Private copy constructor
        store_timer& operator=( const store_timer& rhs );   // Private assignment operator
    };
    
    // Structure to store thread specific information
    struct thread_info {
        size_t id;                          // The id of the calling thread
        int thread_num;                     // The internal id of the thread
        unsigned int N_timers;              // The number of timers seen by the current thread
        volatile thread_info *next;         // Pointer to the next entry in the head list
        size_t active[TRACE_SIZE];          // Store the current active traces
        store_timer *head[TIMER_HASH_SIZE]; // Store the timers in a hash table
        size_t N_memory_steps;              // The number of steps we have for the memory usage
        size_t N_memory_alloc;              // The size of the arrays allocated for time_memory and size_memory
        double* time_memory;                // The times at which we know the memory usage
        size_t* size_memory;                // The memory usage at each time
        // Constructor used to initialize key values
		thread_info() {
            memset(this,0,sizeof(thread_info));
		}
        // Destructor
		~thread_info() {
            delete next;
            next = NULL;
            for (int i=0; i<TIMER_HASH_SIZE; i++) {
                delete head[i];
                head[i] = NULL;
            }
            delete [] time_memory;
            delete [] size_memory;
            memset(this,0,sizeof(thread_info));
		}
      private:
        thread_info( const thread_info& rhs );              // Private copy constructor
        thread_info& operator=( const thread_info& rhs );   // Private assignment operator
    };
    
    // Store thread specific info (use a small hash table to make searching faster)
    volatile int N_threads;
    volatile thread_info *thread_head[THREAD_HASH_SIZE];

    // Store the global timer info in a hash table
    volatile int N_timers;
    volatile store_timer_data_info *timer_table[TIMER_HASH_SIZE];

    // Function to return a pointer to the thread info (or create it if necessary)
    thread_info* get_thread_data( );

    // Function to return a pointer to the global timer info (or create it if necessary)
    store_timer_data_info* get_timer_data( size_t id );

    // Function to return the appropriate timer block
    inline store_timer* get_block( thread_info *thread_data, const char* message, 
        const char* filename, size_t timer_id, const int start, const int stop );

    // Function to return a hopefully unique id based on the active bit array
    static inline size_t get_trace_id( const size_t *trace );

    // Function to return the string of active timers
    static std::vector<id_struct> get_active_list( size_t *active, unsigned int myIndex, thread_info *head );

    // Function to get the current memory usage
    static inline size_t get_memory_usage();

    // Function to load a single timer file (appending the data to the vector)
    static void load_timer( const std::string& filename, std::vector<TimerResults>& timers, 
        int& N_procs, std::string& date, bool& load_trace, bool& load_memory );

    // Function to load a single trace file (appending the data to the vector)
    static void load_trace( const std::string& filename, std::vector<TimerResults>& timers );

    // Function to load a single memory file
    static void load_memory( const std::string& filename, std::vector<MemoryResults>& memory );

    // Functions to send all timers/memory to rank 0
    static void gather_timers( std::vector<TimerResults>& timers );
    static void add_timers( std::vector<TimerResults>& timers, const std::vector<TimerResults> add );
    static void gather_memory( std::vector<MemoryResults>& memory );

    // Handle to a mutex lock
    #ifdef USE_WINDOWS
        HANDLE lock;                // Handle to a mutex lock
    #elif defined(USE_LINUX) || defined(USE_MAC)
        pthread_mutex_t lock;       // Handle to a mutex lock
    #else
        #error Unknown OS
    #endif
    
    // Misc variables
    bool d_store_trace_data;        // Do we want to store trace information
    bool d_store_memory_data;       // Do we want to store memory information
    bool d_check_timer_error;       // Do we want to store memory information
    char d_level;                   // Level of timing to use (default is 0, -1 is disabled)
    TIME_TYPE d_construct_time;     // Store when the constructor was called
    TIME_TYPE d_frequency;          // Clock frequency (only used for windows)
    double d_shift;                 // Offset to add to all trace times when saving (used to synchronize the trace data)
    mutable size_t d_max_trace_remaining; // The number of traces remaining to store for each thread
    mutable size_t d_N_memory_steps; // The number of steps we have for the memory usage
    mutable double* d_time_memory;  // The times at which we know the memory usage
    mutable size_t* d_size_memory;  // The memory usage at each time
    mutable volatile int64_t d_bytes; // The current memory used by the profiler
};


// The global profiler
extern ProfilerApp global_profiler;


/** \class ScopedTimer
  *
  * This class provides a scoped timer that automatically stops when it
  * leaves scope and is thread safe.
  * Example usage:
  *    void my_function(void *arg) {
  *       ScopedTimer timer = PROFILE_SCOPED(timer,"my function");
  *       ...
  *    }
  *    void my_function(void *arg) {
  *       PROFILE_SCOPED(timer,"my function");
  *       ...
  *    }
  */
class ScopedTimer {
public:
    /**                 
     * @brief Create and start a scoped profiler
     * @details This is constructor to create and start a timer that starts
     *    at the given line, and is automatically deleted.  
     *    The scoped timer is also recursive safe, in that it automatically
     *    appends "-x" to indicate the number of recursive calls of the given timer.  
     *    Note: We can only have one scoped timer in a given scope
     *    Note: the scoped timer is generally lower performance that PROFILE_START and PROFILE_STOP.
     * @param msg           Name of the timer
     * @param file          Name of the file containing the code (__FILE__)
     * @param line          Line number containing the start command (__LINE__)
     * @param level         Level of detail to include this timer (default is 0)
     *                      Only timers whos level is <= the level of the specified by enable will be included.
     * @param app           Profiler application to use.  Default is the global profiler
     */
    ScopedTimer( const std::string& msg, const char* file, const int line, 
        const int level=0, ProfilerApp& app=global_profiler ):
        d_app(app), d_filename(file), d_line(line), d_level(level)
    {
        d_id = 0;
        if ( level <= app.get_level( ) ) {
            int recursive_level = 0;
            char buffer[16];
            while ( d_id==0 ) {
                recursive_level++;
                sprintf(buffer,"-%i",recursive_level);
                d_message = msg + std::string(buffer);
                size_t id2 = ProfilerApp::get_timer_id(d_message.c_str(),d_filename);
                bool test = d_app.active(d_message,d_filename,id2);
                d_id = test ? 0:id2;
            }
            d_app.start(d_message,d_filename,d_line,d_level,d_id);
        }
    }
    ~ScopedTimer()
    {
        // Note: we do not require that d_filename is still in scope since we use the timer id
        if ( d_id != 0 ) 
            d_app.stop(d_message,d_filename,-1,d_level,d_id);
    }
protected:
    ScopedTimer(const ScopedTimer&);            // Private copy constructor
    ScopedTimer& operator=(const ScopedTimer&); // Private assignment operator
private:
    ProfilerApp& d_app;
    std::string d_message;
    const char* d_filename;
    const int d_line;
    const int d_level;
    size_t d_id;
};


#include "ProfilerAppMacros.h"


#endif


