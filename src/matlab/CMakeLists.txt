INCLUDE ( macros )


# Copy MATLAB GUI files
SET( GUI_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/load_timer.m"
    "${CMAKE_CURRENT_SOURCE_DIR}/load_timer.fig"
    "${CMAKE_CURRENT_SOURCE_DIR}/plot_trace.m"
    "${CMAKE_CURRENT_SOURCE_DIR}/plot_trace.fig"
    "${CMAKE_CURRENT_SOURCE_DIR}/load_timer_file.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/load_timer_set.m"
    "${CMAKE_CURRENT_SOURCE_DIR}/get_memory_time.cpp" 
    "${CMAKE_CURRENT_SOURCE_DIR}/get_active_times.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/matlab_helpers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/TraceClass.m"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerApp.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerApp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerAppMacros.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MemoryApp.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../MemoryApp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ProfilerAtomicHelpers.h"
)
INSTALL(FILES ${GUI_FILES} DESTINATION "${TIMER_INSTALL_DIR}/gui" )


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


