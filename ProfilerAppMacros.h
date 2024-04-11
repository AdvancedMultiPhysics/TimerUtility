#ifndef included_ProfilerAppMacros
#define included_ProfilerAppMacros

#define OVERLOADED_MACRO( M, ... ) _OVR( M, _COUNT_ARGS( __VA_ARGS__ ) )( __VA_ARGS__ )
#define _OVR( macroName, number_of_args ) _OVR_EXPAND( macroName, number_of_args )
#define _OVR_EXPAND( macroName, number_of_args ) macroName##number_of_args
#define _COUNT_ARGS( ... ) _ARG_PATTERN_MATCH( __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1 )
#define _ARG_PATTERN_MATCH( _1, _2, _3, _4, _5, _6, _7, _8, _9, N, ... ) N

// Define some helper functions
#define PROFILE_ID( NAME ) ProfilerApp::getTimerId( NAME, __FILE__, __LINE__ )
#define CALL_STATIC_TIMER_VAR( VAR, ID, FIXED, NAME, LEVEL, TRACE ) \
    ProfilerAppTimer<ID, FIXED> VAR( NAME, __FILE__, __LINE__, LEVEL, TRACE )
#define CALL_STATIC_TIMER( ID_NAME, FIXED, NAME, LINE, LEVEL, TRACE ) \
    CALL_STATIC_TIMER_VAR( profile_##LINE, PROFILE_ID( ID_NAME ), FIXED, NAME, LEVEL, TRACE )
#define PROFILE_2( NAME, LINE ) CALL_STATIC_TIMER( NAME, true, NAME, LINE, 0, -1 )
#define PROFILE_3( NAME, LEVEL, LINE ) CALL_STATIC_TIMER( NAME, true, NAME, LINE, LEVEL, -1 )
#define PROFILE_4( NAME, LEVEL, TRACE, LINE ) \
    CALL_STATIC_TIMER( NAME, true, NAME, LINE, LEVEL, TRACE )
#define PROFILE2_2( NAME, LINE ) CALL_STATIC_TIMER( "", false, NAME, LINE, 0, -1 )
#define PROFILE2_3( NAME, LEVEL, LINE ) CALL_STATIC_TIMER( "", false, NAME, LINE, LEVEL, -1 )
#define PROFILE2_4( NAME, LEVEL, TRACE, LINE ) \
    CALL_STATIC_TIMER( "", false, NAME, LINE, LEVEL, TRACE )
#define PROFILE_SAVE_GLOBAL( NAME, GLOB, ... ) global_profiler.save( NAME, GLOB )


/*! \addtogroup Macros
 *  @{
 */


/*! \def PROFILE(NAME,LEVEL,TRACE)
 *  \brief Create and start a timer
 *  \details This create and start a Timer
 *      See  \ref ProfilerApp "ProfilerApp" and
 *     \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME     Name of the timer
 *  \param LEVEL    Optional level at which to enable the timer
 *  \param TRACE    Optional flag to indicate if we want to store trace level data
 */
#define PROFILE( ... ) OVERLOADED_MACRO( PROFILE_, __VA_ARGS__, __LINE__ )


/*! \def PROFILE2(OBJ,NAME,LEVEL,TRACE)
 *  \brief Create and start a timer
 *  \details This create and start a Timer
 *      See  \ref ProfilerApp "ProfilerApp" and
 *     \ref ProfilerApp "ProfilerApp" for more info.
 *  \param NAME     Name of the timer
 *  \param LEVEL    Optional level at which to enable the timer
 *  \param TRACE    Optional flag to indicate if we want to store trace level data
 */
#define PROFILE2( ... ) OVERLOADED_MACRO( PROFILE2_, __VA_ARGS__, __LINE__ )


/*! \def PROFILE_MEMORY()
 *  \brief Add the current memory usage to the trace
 *  \details dd the current memory usage to the trace
 *      See  \ref ProfilerApp "ProfilerApp" and
 *     \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_MEMORY() global_profiler.memory()


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
 *  \param GLOBAL   Optional variable to save all ranks in a single file (default is true)
 */
#define PROFILE_SAVE( ... ) PROFILE_SAVE_GLOBAL( __VA_ARGS__, true, 0 )


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
#define PROFILE_ENABLE_MEMORY( ... ) global_profiler.setStoreMemory( __VA_ARGS__ )


/*! \def PROFILE_DISABLE_MEMORY()
 *  \brief Disable the memory trace
 *  \details This will disable the monitoring of the memory usage within

 *      the application.
 *      See  \ref ProfilerApp "ProfilerApp" for more info.
 */
#define PROFILE_DISABLE_MEMORY() global_profiler.setStoreMemory( ProfilerApp::MemoryLevel::None )


/*! @} */


#endif
