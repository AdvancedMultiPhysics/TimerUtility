#ifndef included_ProfilerAppMacros
#define included_ProfilerAppMacros


// Define some C interfaces to the global profiler
extern void global_profiler_enable( int level );
extern void global_profiler_disable();
extern int global_profiler_get_level();
extern void global_profiler_synchronize();
extern void global_profiler_set_store_trace( int flag );
extern void global_profiler_set_store_memory( int flag );
extern void global_profiler_start( const char* name, const char* file, int line, int level );
extern void global_profiler_stop( const char* name, const char* file, int line, int level );
extern void global_profiler_save( const char* name, int global );


// Define some helper macros
#define GET_LEVEL( _0, N, ... ) N
#define PROFILE_START_LEVEL( NAME, FILE, LINE, LEVEL )        \
    do {                                                      \
        if ( ( (int) LEVEL ) <= global_profiler_get_level() ) \
            global_profiler_start( NAME, FILE, LINE, LEVEL ); \
    } while ( 0 )
#define PROFILE_STOP_LEVEL( NAME, FILE, LINE, LEVEL )        \
    do {                                                     \
        if ( LEVEL <= global_profiler_get_level() )          \
            global_profiler_stop( NAME, FILE, LINE, LEVEL ); \
    } while ( 0 )
#define PROFILE_SAVE_GLOBAL( NAME, GLOB ) global_profiler_save( NAME, GLOB )


/*! \addtogroup Macros
 *  @{
 */


/*! \def PROFILE_START(NAME,..)
 *  \brief Start the profiler
 *  \details This is the primary call to start a timer.  Only one call within a file
 *      may call the timer.  Any other calls must use PROFILE_START2(X).
 *      This call will automatically add the file and line number to the timer.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME  Name of the timer
 */
#define PROFILE_START( NAME, ... ) \
    PROFILE_START_LEVEL( NAME, __FILE__, __LINE__, GET_LEVEL( _0, ##__VA_ARGS__, 0 ) )


/*! \def PROFILE_STOP(NAME,..)
 *  \brief Stop the profiler
 *  \details This is the primary call to stop a timer.  Only one call within a file
 *      may call the timer.  Any other calls must use PROFILE_STOP2(X).
 *      This call will automatically add the file and line number to the timer.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME  Name of the timer
 */
#define PROFILE_STOP( NAME, ... ) \
    PROFILE_STOP_LEVEL( NAME, __FILE__, __LINE__, GET_LEVEL( _0, ##__VA_ARGS__, 0 ) )


/*! \def PROFILE_START2(NAME,..)
 *  \brief Start the profiler
 *  \details This is a call to start a timer without the line number.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME  Name of the timer
 */
#define PROFILE_START2( NAME, ... ) \
    PROFILE_START_LEVEL( NAME, __FILE__, -1, GET_LEVEL( _0, ##__VA_ARGS__, 0 ) )


/*! \def PROFILE_STOP2(NAME,..)
 *  \brief Start the profiler
 *  \details This is a call to start a timer without the line number.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME  Name of the timer
 */
#define PROFILE_STOP2( NAME, ... ) \
    PROFILE_STOP_LEVEL( NAME, __FILE__, -1, GET_LEVEL( _0, ##__VA_ARGS__, 0 ) )


/*! \def PROFILE_SYNCHRONIZE()
 *  \brief Synchronize the time across multiple processors
 *  \details This will synchronize time zero across all processors.
 *      In a MPI program, the different processes may start a slightly
 *      different times.  Since we often want to examine the timers
 *      on different processors we need to synchronize time zero so it is
 *      consistent.
 *      Note:  This program should be called once after MPI has been initialized.
 *        it does not need to be called in a serial program and there is no benifit
 *        to multiple calls.
 *      Note: this is blocking call.
 */
#define PROFILE_SYNCHRONIZE() global_profiler_synchronize()


/*! \def PROFILE_SAVE(FILE,...)
 *  \brief Save the profile results
 *  \details This will save the results of the timers the file provided
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param FILE     Name of the file to save
 *  \param GLOBAL   Optional variable to save all ranks in a single file (default is false)
 */
#define PROFILE_SAVE( FILE, ... ) PROFILE_SAVE_GLOBAL( FILE, GET_LEVEL( _0, ##__VA_ARGS__, false ) )


/*! \def PROFILE_STORE_TRACE(X)
 *  \brief Enable/Disable the trace data
 *  \details This will enable or disable trace timers.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param X  Flag to indicate if we want to enable/disable the trace timers
 */
#define PROFILE_STORE_TRACE( X ) global_profiler_set_store_trace( X )


/*! \def PROFILE_ENABLE(...)
 *  \brief Enable the timers
 *  \details This will enable the timers.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_ENABLE( LEVEL ) global_profiler_enable( LEVEL )


/*! \def PROFILE_DISABLE()
 *  \brief Disable the timers
 *  \details This will disable the timers.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE() global_profiler_disable()


/*! \def PROFILE_ENABLE_TRACE()
 *  \brief Enable the trace level timers
 *  \details This will enable the trace capabilites within the timers.
 *      It does not affect the which timers are enabled or disabled.
 *      By default trace cabailities are disabled and may affect the
 *      performance if enabled.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_ENABLE_TRACE() global_profiler_set_store_trace( 1 )


/*! \def PROFILE_DISABLE_TRACE()
 *  \brief Disable the trace level timers
 *  \details This will disable the trace capabilites within the timers.
 *      It does not affect the which timers are enabled or disabled.
 *      By default trace cabailities are disabled and may affect the
 *      performance if enabled.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE_TRACE() global_profiler_set_store_trace( 0 )


/*! \def PROFILE_ENABLE_MEMORY()
 *  \brief Enable the memory trace
 *  \details This will enable the monitoring of the memory usage within
 *      the application.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_ENABLE_MEMORY() global_profiler_set_store_memory( 1 )


/*! \def PROFILE_DISABLE_MEMORY()
 *  \brief Disable the memory trace
 *  \details This will disable the monitoring of the memory usage within
 *      the application.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE_MEMORY() global_profiler_set_store_memory( 0 )


/*! @} */

#endif
