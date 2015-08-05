INCLUDE ( macros )


# Copy MATLAB GUI files
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/load_timer.m"           "${TIMER_INSTALL_DIR}/gui/load_timer.m"         COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/load_timer.fig"         "${TIMER_INSTALL_DIR}/gui/load_timer.fig"       COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/plot_trace.m"           "${TIMER_INSTALL_DIR}/gui/plot_trace.m"         COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/plot_trace.fig"         "${TIMER_INSTALL_DIR}/gui/plot_trace.fig"       COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/load_timer_file.cpp"    "${TIMER_INSTALL_DIR}/gui/load_timer_file.cpp"  COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/load_timer_set.m"       "${TIMER_INSTALL_DIR}/gui/load_timer_set.m"     COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/get_memory_time.cpp"    "${TIMER_INSTALL_DIR}/gui/get_memory_time.cpp"  COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/get_active_times.cpp"   "${TIMER_INSTALL_DIR}/gui/get_active_times.cpp" COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/matlab_helpers.h"       "${TIMER_INSTALL_DIR}/gui/matlab_helpers.h"     COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/TraceClass.m"           "${TIMER_INSTALL_DIR}/gui/TraceClass.m"         COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerApp.h"       "${TIMER_INSTALL_DIR}/gui/ProfilerApp.h"        COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerApp.cpp"     "${TIMER_INSTALL_DIR}/gui/ProfilerApp.cpp"      COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerAppMacros.h" "${TIMER_INSTALL_DIR}/gui/ProfilerAppMacros.h"  COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/../MemoryApp.h"         "${TIMER_INSTALL_DIR}/gui/MemoryApp.h"          COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/../MemoryApp.cpp"       "${TIMER_INSTALL_DIR}/gui/MemoryApp.cpp"        COPYONLY )
CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerAtomicHelpers.h" "${TIMER_INSTALL_DIR}/gui/ProfilerAtomicHelpers.h" COPYONLY )

# Create a matlab file to create class arrays
SET( CreateClassArray "${TIMER_INSTALL_DIR}/gui/mxCreateClassArrayHelper.m" )
file(WRITE  ${CreateClassArray} "function x = mxCreateClassArrayHelper(cmd)\n" )
file(APPEND ${CreateClassArray} "% Helper file for mxCreateClassArray\n" )
file(APPEND ${CreateClassArray} "% cmd - Command to run, should be in format:\n" )
file(APPEND ${CreateClassArray} "%    x = something;\n" )
file(APPEND ${CreateClassArray} "eval(cmd);\n" )
file(APPEND ${CreateClassArray} "end\n" )


IF ( USE_MATLAB )
    # Add the mex target
    ADD_CUSTOM_TARGET ( mex ALL DEPENDS timerutility )
    ADD_MEXCLEAN()

    # Add simple MATLAB MEX test
    ADD_MATLAB_MEX( hello_world.cpp )

    # Add the MATLAB MEX files
    ADD_MATLAB_MEX( load_timer_file.cpp )
    ADD_MATLAB_MEX( get_memory_time.cpp )
    ADD_MATLAB_MEX( get_active_times.cpp )
    ADD_MATLAB_MEX( profile_mex.cpp )

    # Add subdirectory
    ADD_SUBDIRECTORY( test )
ENDIF()


