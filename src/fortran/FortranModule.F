      module profiler
        use iso_c_binding, only: C_CHAR, C_NULL_CHAR
        contains
        ! Function to start the timer
        subroutine GLOBAL_PROFILE_START( timer_name, file_name, line, level )
          character(*), intent(in), optional :: timer_name, file_name
          integer, intent(in), optional :: line, level
          interface
            subroutine global_profiler_start( timer_name,
     &          file_name, line, level ) bind(C)
              use iso_c_binding, only: C_CHAR, C_INT
              implicit none
              integer(c_int), VALUE :: line, level
              character(kind=c_char) :: timer_name(*), file_name(*)
            end subroutine global_profiler_start
          end interface
          call global_profiler_start( timer_name//C_NULL_CHAR, 
     &        file_name//C_NULL_CHAR, line, level )
        end subroutine
        ! Function to stop the timer
        subroutine GLOBAL_PROFILE_STOP( timer_name, file_name, line, level )
          character(*), intent(in), optional :: timer_name, file_name
          integer, intent(in), optional :: line, level
          interface
            subroutine global_profiler_stop( timer_name,
     &          file_name, line, level ) bind(C)
              use iso_c_binding, only: C_CHAR, C_INT
              implicit none
              integer(c_int), VALUE :: line, level
              character(kind=c_char) :: timer_name(*), file_name(*)
            end subroutine global_profiler_stop
          end interface
          call global_profiler_stop( timer_name//C_NULL_CHAR, 
     &        file_name//C_NULL_CHAR, line, level )
        end subroutine
        ! Function to save the results
        subroutine GLOBAL_PROFILE_SAVE( save_name, global )
          character(*), intent(in), optional :: save_name
          integer, intent(in), optional :: global
          interface
            subroutine global_profiler_save( save_name, global ) bind(C)
              use iso_c_binding, only: C_CHAR, C_INT
              implicit none
              integer(c_int), VALUE :: global
              character(kind=c_char) :: save_name(*)
            end subroutine global_profiler_save
          end interface
          call global_profiler_save( save_name//C_NULL_CHAR, global )
        end subroutine
        ! Function to enable timers
        subroutine GLOBAL_PROFILE_ENABLE( level )
          integer, intent(in), optional :: level
          interface
            subroutine global_profiler_enable( level ) bind(C)
              use iso_c_binding, only: C_INT
              implicit none
              integer(c_int), VALUE :: level
            end subroutine global_profiler_enable
          end interface
          call global_profiler_enable( level )
        end subroutine
        ! Function to disable timers
        subroutine GLOBAL_PROFILE_DISABLE( )
          interface
            subroutine global_profiler_disable( ) bind(C)
            end subroutine global_profiler_disable
          end interface
          call global_profiler_disable( )
        end subroutine
        ! Function to syncronize the timers
        subroutine GLOBAL_PROFILE_SYNC( )
          interface
            subroutine global_profiler_synchronize( ) bind(C)
            end subroutine global_profiler_synchronize
          end interface
          call global_profiler_synchronize( )
        end subroutine
        ! Function to set trace status
        subroutine GLOBAL_PROFILE_SET_STORE_TRACE( flag )
          integer, intent(in), optional :: flag
          interface
            subroutine global_profiler_set_store_trace( flag ) bind(C)
              use iso_c_binding, only: C_INT
              implicit none
              integer(c_int), VALUE :: flag
            end subroutine global_profiler_set_store_trace
          end interface
          call global_profiler_set_store_trace( flag )
        end subroutine
        ! Function to set trace status
        subroutine GLOBAL_PROFILE_SET_STORE_MEMORY( flag )
          integer, intent(in), optional :: flag
          interface
            subroutine global_profiler_set_store_memory( flag ) bind(C)
              use iso_c_binding, only: C_INT
              implicit none
              integer(c_int), VALUE :: flag
            end subroutine global_profiler_set_store_memory
          end interface
          call global_profiler_set_store_memory( flag )
        end subroutine
      end module
