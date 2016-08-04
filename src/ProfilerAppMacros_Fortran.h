#ifndef included_ProfilerAppMacros
#define included_ProfilerAppMacros

c PROFILE_START(NAME,LEVEL)
c This is the primary call to start a timer.  Only one call within a file 
c may call the timer.  Any other calls must use PROFILE_START2(X).
c This call will automatically add the file and line number to the timer.
#define PROFILE_START(NAME,LEVEL) \
     call GLOBAL_PROFILE_START( NAME, __FILE__, __LINE__, LEVEL )


c PROFILE_STOP(NAME,LEVEL)
c This is the primary call to stop a timer.  Only one call within a file 
c may call the timer.  Any other calls must use PROFILE_STOP2(X).
c This call will automatically add the file and line number to the timer.
c An optional argument specifying the level to enable may be included.
#define PROFILE_STOP(NAME,LEVEL) \
     call GLOBAL_PROFILE_STOP( NAME, __FILE__, __LINE__, LEVEL )


c PROFILE_START2(NAME,LEVEL)
c This is a call to start a timer without the line number.
c An optional argument specifying the level to enable may be included.
#define PROFILE_START2(NAME,LEVEL) \
     call GLOBAL_PROFILE_START( NAME, __FILE__, -1, LEVEL )


c PROFILE_STOP2(NAME,LEVEL)
c This is a call to start a timer without the line number.
c An optional argument specifying the level to enable may be included.
#define PROFILE_STOP2(NAME,LEVEL) \
    call GLOBAL_PROFILE_STOP( NAME, __FILE__, -1, LEVEL )


c PROFILE_SYNCHRONIZE()
c This will synchronize time zero across all processors.
c In a MPI program, the different processes may start a slightly
c different times.  Since we often want to examine the timers
c on different processors we need to synchronize time zero so it is 
c consistent.  
c Note:  This program should be called once after MPI has been initialized.
c   it does not need to be called in a serial program and there is no benifit
c   to multiple calls.
c Note: this is blocking call.
#define PROFILE_SYNCHRONIZE() \
    call GLOBAL_PROFILE_SYNC( )


c PROFILE_SAVE(FILE,GLOBAL)
c This will save the results of the timers the file provided
c An optional argument specifying the level to enable may be included.
#define PROFILE_SAVE(FILE) \
    call GLOBAL_PROFILE_SAVE( FILE, 1 ) 


c PROFILE_ENABLE(LEVEL)
c This will enable the timers.
c An optional argument specifying the level to enable may be included.
#define PROFILE_ENABLE(LEVEL) \
    call GLOBAL_PROFILE_ENABLE( LEVEL )


c PROFILE_DISABLE()
c This will disable the timers.
#define PROFILE_DISABLE() \
    call GLOBAL_PROFILE_DISABLE()


c PROFILE_ENABLE_TRACE()
c This will enable the trace capabilites within the timers.
c It does not affect the which timers are enabled or disabled.
c By default trace cabailities are disabled and may affect the
c performance if enabled.
#define PROFILE_ENABLE_TRACE() \
    call GLOBAL_PROFILE_SET_STORE_TRACE(1)


c PROFILE_DISABLE_TRACE()
c This will disable the trace capabilites within the timers.
c It does not affect the which timers are enabled or disabled.
c By default trace cabailities are disabled and may affect the
c performance if enabled.
#define PROFILE_DISABLE_TRACE() \
    call GLOBAL_PROFILE_SET_STORE_TRACE(0)


c PROFILE_ENABLE_MEMORY()
c This will enable the monitoring of the memory usage within the application. 
#define PROFILE_ENABLE_MEMORY() \
    call GLOBAL_PROFILE_SET_STORE_MEMORY(1)


c PROFILE_DISABLE_MEMORY()
c This will disable the monitoring of the memory usage within the application. 
#define PROFILE_DISABLE_MEMORY() \
    call GLOBAL_PROFILE_SET_STORE_MEMORY(0)


#endif

