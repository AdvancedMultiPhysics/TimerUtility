c Include profiler macros
#include "ProfilerAppMacros_Fortran.h"


      program hello_world
        use profiler

        integer i, N
        real :: start, finish
        N = 100000

        ! Enable the profiler including all level 0-1 timers
        PROFILE_ENABLE(1)

        ! Syncronize the timers so 
        PROFILE_SYNCHRONIZE()

        ! Test performance of timers without memory/trace data
        call cpu_time(start)
        do i = 1, N
          PROFILE_START( "subroutine 1", 0 )
          PROFILE_STOP( "subroutine 1", 0 )
        enddo
        call cpu_time(finish)
        print '("Timer overhead: ",f6.2," us")',1e6*(finish-start)/N

        ! Disable and re-enable the timers (to reset data)
        PROFILE_DISABLE()
        PROFILE_ENABLE(1)

        ! Enable the trace level data (incurs extra overhead/call)
        PROFILE_ENABLE_TRACE()

        ! Enable the trace level data (incurs extra overhead/call)
        PROFILE_ENABLE_MEMORY()

        ! Enable a timer
        PROFILE_START(  "Main", 0 )

        ! Call sub timers
        call cpu_time(start)
        do i = 1, N
          PROFILE_START( "subroutine 2", 0 )
          PROFILE_STOP( "subroutine 2", 0 )
        enddo
        call cpu_time(finish)
        print '("Trace overhead: ",f6.2," us")',1e6*(finish-start)/N

        ! Stop a timer
        PROFILE_STOP(  "Main", 0 )

        ! Save timer results
        PROFILE_SAVE(  "test_ProfilerApp_Fortran" )

        ! Disable timers (deletes timer data)
        PROFILE_DISABLE()

      end program


