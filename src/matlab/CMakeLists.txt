# Create copy target to copy class and include files for mex files
SET( COPY_TARGET copy-mex-include )
ADD_CUSTOM_TARGET( ${COPY_TARGET} ALL )


# Copy header files from the matlab directory
FILE( GLOB HFILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp )
FOREACH (HFILE ${HFILES})
    SET( SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${HFILE}" )
    SET( DST_FILE "${${PROJ}_INSTALL_DIR}/include/${${PROJ}_INC}/matlab/${HFILE}" )
    ADD_CUSTOM_COMMAND(TARGET ${COPY_TARGET} 
        PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_FILE}" "${DST_FILE}"
        DEPENDS "${SRC_FILE}"
    )
ENDFOREACH ()


# Copy matlab classes to the mex folder
ADD_CUSTOM_COMMAND(TARGET ${COPY_TARGET} 
    PRE_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/TraceClass.m" "${${PROJ}_INSTALL_DIR}/mex/TraceClass.m"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/TraceClass.m"
)


# Create a matlab file to create class arrays
SET( CreateClassArray "${TIMER_INSTALL_DIR}/mex/mxCreateClassArrayHelper.m" )
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

    # Create the wrappers for Matlab
    CREATE_MATLAB_WRAPPER()

    # Add subdirectory
    ADD_SUBDIRECTORY( test )
ENDIF()


