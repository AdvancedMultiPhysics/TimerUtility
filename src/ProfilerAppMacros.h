#ifndef included_ProfilerAppMacros
#define included_ProfilerAppMacros

// Define some helper functions
#define PROFILE_START_LEVEL( FILE, LINE, NAME, LEVEL, ... )                                    \
    do {                                                                                       \
        if ( LEVEL <= global_profiler.getLevel() ) {                                           \
            constexpr uint64_t v1 = ProfilerApp::hashString( ProfilerApp::stripPath( FILE ) ); \
            static_assert( v1 != 0, "Invalid FILE hash" );                                     \
            uint64_t v2 = ProfilerApp::hashString( NAME );                                     \
            uint64_t id = ( v2 << 32 ) + ( v1 ^ v2 );                                          \
            global_profiler.start( id, ProfilerApp::getString( NAME ), FILE, LINE, LEVEL );    \
        }                                                                                      \
    } while ( 0 )
#define PROFILE_STOP_LEVEL( FILE, LINE, NAME, LEVEL, ... )                                     \
    do {                                                                                       \
        if ( LEVEL <= global_profiler.getLevel() ) {                                           \
            constexpr uint64_t v1 = ProfilerApp::hashString( ProfilerApp::stripPath( FILE ) ); \
            static_assert( v1 != 0, "Invalid FILE hash" );                                     \
            uint64_t v2 = ProfilerApp::hashString( NAME );                                     \
            uint64_t id = ( v2 << 32 ) + ( v1 ^ v2 );                                          \
            global_profiler.stop( id, ProfilerApp::getString( NAME ), FILE, LINE, LEVEL );     \
        }                                                                                      \
    } while ( 0 )
#define PROFILE_SCOPED_LEVEL( FILE, LINE, OBJ, NAME, LEVEL, ... )                           \
    ScopedTimer OBJ( ProfilerApp::hashString( ProfilerApp::stripPath( FILE ) ),             \
        ProfilerApp::hashString( NAME ), ProfilerApp::getString( NAME ), FILE, LINE, LEVEL, \
        global_profiler )
#define PROFILE_SAVE_GLOBAL( NAME, GLOB, ... ) global_profiler.save( NAME, GLOB )


/*! \addtogroup Macros
 *  @{
 */


/*! \def PROFILE_START(NAME,LEVEL)
 *  \brief Start the profiler
 *  \details This is the primary call to start a timer.  Only one call within a file
 *      may call the timer.  Any other calls must use PROFILE_START2(X).
 *      This call will automatically add the file and line number to the timer.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME     Name of the timer
 *  \param LEVEL    Level at which to enable the timer
 */
#define PROFILE_START( ... ) PROFILE_START_LEVEL( __FILE__, __LINE__, __VA_ARGS__, 0, 0 )


/*! \def PROFILE_STOP(NAME,LEVEL)
 *  \brief Stop the profiler
 *  \details This is the primary call to stop a timer.  Only one call within a file
 *      may call the timer.  Any other calls must use PROFILE_STOP2(X).
 *      This call will automatically add the file and line number to the timer.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME     Name of the timer
 *  \param LEVEL    Level at which to enable the timer
 */
#define PROFILE_STOP( ... ) PROFILE_STOP_LEVEL( __FILE__, __LINE__, __VA_ARGS__, 0, 0 )


/*! \def PROFILE_START2(NAME,LEVEL)
 *  \brief Start the profiler
 *  \details This is a call to start a timer without the line number.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME     Name of the timer
 *  \param LEVEL    Level at which to enable the timer
 */
#define PROFILE_START2( ... ) PROFILE_START_LEVEL( __FILE__, -1, __VA_ARGS__, 0, 0 )


/*! \def PROFILE_STOP2(NAME,LEVEL)
 *  \brief Start the profiler
 *  \details This is a call to start a timer without the line number.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME     Name of the timer
 *  \param LEVEL    Level at which to enable the timer
 */
#define PROFILE_STOP2( ... ) PROFILE_STOP_LEVEL( __FILE__, -1, __VA_ARGS__, 0, 0 )


/*! \def PROFILE_SCOPED(OBJ,NAME,LEVEL)
 *  \brief Create and start a scoped timer
 *  \details This create and start a ScopedTimer
 *      See  \ref ProfilerApp "ProfilerApp" and
 *     \ref ProfilerApp "ProfilerApp" for more info.
 *  \param OBJ      Name of the object
 *  \param NAME     Name of the timer
 *  \param LEVEL    Level at which to enable the timer
 */
#define PROFILE_SCOPED( ... ) PROFILE_SCOPED_LEVEL( __FILE__, __LINE__, __VA_ARGS__, 0, 0 )


/*! \def PROFILE_SYNCHRONIZE()
 *  \brief Synchronize the time across multiple processors
 *  \details This will synchronize time zero across all processors.
 *      In a MPI program, the different processes may start a slightly
 *      different times.  Since we often want to examine the timers
 *      on different processors we need to synchronize time zero so it is
 *      consistent.
 *      Note:  This program should be called once after MPI has been initialized.
 *        it does not need to be called in a serial program and there is no benefit
 *        to multiple calls.
 *      Note: this is blocking call.
 */
#define PROFILE_SYNCHRONIZE() global_profiler.synchronize()


/*! \def PROFILE_SAVE(FILE,GLOBAL)
 *  \brief Save the profile results
 *  \details This will save the results of the timers the file provided
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param FILE     Name of the file to save
 *  \param GLOBAL   Optional variable to save all ranks in a single file (default is false)
 */
#define PROFILE_SAVE( ... ) PROFILE_SAVE_GLOBAL( __VA_ARGS__, false, 0 )


/*! \def PROFILE_STORE_TRACE(X)
 *  \brief Enable/Disable the trace data
 *  \details This will enable or disable trace timers.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 *  \param X  Flag to indicate if we want to enable/disable the trace timers
 */
#define PROFILE_STORE_TRACE( X ) global_profiler.setStoreTrace( X )


/*! \def PROFILE_ENABLE(LEVEL)
 *  \brief Enable the timers
 *  \details This will enable the timers.
 *      An optional argument specifying the level to enable may be included.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_ENABLE global_profiler.enable


/*! \def PROFILE_DISABLE()
 *  \brief Disable the timers
 *  \details This will disable the timers.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE() global_profiler.disable()


/*! \def PROFILE_ENABLE_TRACE()
 *  \brief Enable the trace level timers
 *  \details This will enable the trace capabilites within the timers.
 *      It does not affect the which timers are enabled or disabled.
 *      By default trace cabailities are disabled and may affect the
 *      performance if enabled.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_ENABLE_TRACE() global_profiler.setStoreTrace( true )


/*! \def PROFILE_DISABLE_TRACE()
 *  \brief Disable the trace level timers
 *  \details This will disable the trace capabilites within the timers.
 *      It does not affect the which timers are enabled or disabled.
 *      By default trace cabailities are disabled and may affect the
 *      performance if enabled.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE_TRACE() global_profiler.setStoreTrace( false )


/*! \def PROFILE_ENABLE_MEMORY()
 *  \brief Enable the memory trace
 *  \details This will enable the monitoring of the memory usage within
 *      the application.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_ENABLE_MEMORY() global_profiler.setStoreMemory( true )


/*! \def PROFILE_DISABLE_MEMORY()
 *  \brief Disable the memory trace
 *  \details This will disable the monitoring of the memory usage within

 *      the application.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE_MEMORY() global_profiler.setStoreMemory( false )


/*! @} */

#endif
