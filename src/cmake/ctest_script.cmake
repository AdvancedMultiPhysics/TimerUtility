# ctest script for building, running, and submitting the test results 
# Usage:  ctest -s script,build
#   build = debug / optimized / valgrind
# Note: this test will use use the number of processors defined in the variable N_PROCS,
#   the enviornmental variable N_PROCS, or the number of processors availible (if not specified)

# Set platform specific variables
SITE_NAME( HOSTNAME )
IF( ${HOSTNAME} STREQUAL "lap0086227" )
    SET( CC "mpicc" )
    SET( CXX "mpicxx" )
    SET( MPIEXEC "mpirun" )
    SET( MATLAB_DIRECTORY "/usr/local/matlab" )
    SET( COVERAGE_COMMAND /usr/bin/gcov )
    SET( VALGRIND_COMMAND /usr/bin/valgrind )
    SET( CTEST_CMAKE_GENERATOR "Unix Makefiles" )
    SET( LDLIBS "-lmpi" )
ELSEIF( ${HOSTNAME} MATCHES "oblivion.*" ) 
    SET( CC "mpicc" )
    SET( CXX "mpicxx" )
    SET( MPIEXEC "mpirun" )
    SET( MATLAB_DIRECTORY "/usr/MATHWORKS_R2008A" )
    SET( COVERAGE_COMMAND /usr/bin/gcov )
    SET( VALGRIND_COMMAND /usr/bin/valgrind )
    SET( LDLIBS "-lmpi" )
    SET( CTEST_CMAKE_GENERATOR "Unix Makefiles" )
ELSEIF( ${HOSTNAME} MATCHES "GAMING" ) 
    SET( MPIEXEC "mpiexec" )
    SET( MATLAB_DIRECTORY "C:/Program Files/MATLAB/R2013a" )
    SET( MAKE_CMD nmake )
    SET( BUILD_SERIAL TRUE )
    SET( CTEST_CMAKE_GENERATOR "NMake Makefiles" )
ELSE()
    MESSAGE( FATAL_ERROR "Unknown host: ${HOSTNAME}" )
ENDIF()


# Get the source directory based on the current directory
IF ( NOT TIMER_SOURCE_DIR )
    SET( TIMER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." )
ENDIF()
IF ( NOT MAKE_CMD )
    SET( MAKE_CMD make )
ENDIF()


# Check that we specified the build type to run
SET( USE_MATLAB 1 )
IF( NOT CTEST_SCRIPT_ARG )
    MESSAGE(FATAL_ERROR "No build specified: ctest -S /path/to/script,build (debug/optimized/valgrind")
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "debug" )
    SET( CTEST_BUILD_NAME "Profiler-debug" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( CTEST_COVERAGE_COMMAND ${COVERAGE_COMMAND} )
    SET( ENABLE_GCOV "true" )
    SET( USE_VALGRIND FALSE )
    SET( USE_VALGRIND_MATLAB FALSE )
ELSEIF( (${CTEST_SCRIPT_ARG} STREQUAL "optimized") OR (${CTEST_SCRIPT_ARG} STREQUAL "opt") )
    SET( CTEST_BUILD_NAME "Profiler-opt" )
    SET( CMAKE_BUILD_TYPE "Release" )
    SET( CTEST_COVERAGE_COMMAND )
    SET( ENABLE_GCOV "false" )
    SET( USE_VALGRIND FALSE )
    SET( USE_VALGRIND_MATLAB FALSE )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "valgrind" )
    SET( CTEST_BUILD_NAME "Profiler-valgrind" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( CTEST_COVERAGE_COMMAND )
    SET( ENABLE_GCOV "false" )
    SET( USE_VALGRIND TRUE )
    SET( USE_VALGRIND_MATLAB FALSE )
    SET( USE_MATLAB 0 )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "valgrind-matlab" )
    SET( CTEST_BUILD_NAME "Profiler-valgrind-matlab" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( CTEST_COVERAGE_COMMAND )
    SET( ENABLE_GCOV "false" )
    SET( USE_VALGRIND FALSE )
    SET( USE_VALGRIND_MATLAB TRUE )
ELSE()
    MESSAGE(FATAL_ERROR "Invalid build (${CTEST_SCRIPT_ARG}): ctest -S /path/to/script,build (debug/opt/valgrind")
ENDIF()
IF ( NOT COVERAGE_COMMAND )
    SET( ENABLE_GCOV "false" )
ENDIF()


# Set the number of processors
IF( NOT DEFINED N_PROCS )
    SET( N_PROCS $ENV{N_PROCS} )
ENDIF()
IF( NOT DEFINED N_PROCS )
    SET(N_PROCS 1)
    # Linux:
    SET(cpuinfo_file "/proc/cpuinfo")
    IF(EXISTS "${cpuinfo_file}")
        FILE(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
        list(LENGTH procs N_PROCS)
    ENDIF()
    # Mac:
    IF(APPLE)
        find_program(cmd_sys_pro "system_profiler")
        if(cmd_sys_pro)
            execute_process(COMMAND ${cmd_sys_pro} OUTPUT_VARIABLE info)
            STRING(REGEX REPLACE "^.*Total Number of Cores: ([0-9]+).*$" "\\1" N_PROCS "${info}")
        ENDIF()
    ENDIF()
    # Windows:
    IF(WIN32)
        SET(N_PROCS "$ENV{NUMBER_OF_PROCESSORS}")
    ENDIF()
ENDIF()


# Set basic variables
SET( CTEST_PROJECT_NAME "TIMER" )
SET( CTEST_SOURCE_DIRECTORY "${TIMER_SOURCE_DIR}" )
SET( CTEST_BINARY_DIRECTORY "." )
SET( CTEST_DASHBOARD "Nightly" )
SET( CTEST_TEST_TIMEOUT 900 )
SET( CTEST_CUSTOM_MAXIMUM_NUMBER_OF_ERRORS 500 )
SET( CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 500 )
SET( CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 10000 )
SET( CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 10000 )
SET( NIGHTLY_START_TIME "18:00:00 EST" )
SET( CTEST_NIGHTLY_START_TIME "22:00:00 EST" )
SET( CTEST_COMMAND "\"${CTEST_EXECUTABLE_NAME}\" -D ${CTEST_DASHBOARD}" )
IF ( BUILD_SERIAL )
    SET( CTEST_BUILD_COMMAND "${MAKE_CMD} -i install" )
ELSE()
    SET( CTEST_BUILD_COMMAND "${MAKE_CMD} -i -j ${N_PROCS} install" )
ENDIF()


# Set valgrind options
#SET (VALGRIND_COMMAND_OPTIONS "--tool=memcheck --leak-check=yes --track-fds=yes --num-callers=50 --show-reachable=yes --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --suppressions=${TIMER_SOURCE_DIR}/src/ValgrindSuppresionFile" )
SET( VALGRIND_COMMAND_OPTIONS  "--tool=memcheck --leak-check=yes --track-fds=yes --num-callers=50 --show-reachable=yes --suppressions=${TIMER_SOURCE_DIR}/src/ValgrindSuppresionFile" )
IF ( USE_VALGRIND )
    SET( MEMORYCHECK_COMMAND ${VALGRIND_COMMAND} )
    SET( MEMORYCHECKCOMMAND ${VALGRIND_COMMAND} )
    SET( CTEST_MEMORYCHECK_COMMAND ${VALGRIND_COMMAND} )
    SET( CTEST_MEMORYCHECKCOMMAND ${VALGRIND_COMMAND} )
    SET( CTEST_MEMORYCHECK_COMMAND_OPTIONS ${VALGRIND_COMMAND_OPTIONS} )
    SET( CTEST_MEMORYCHECKCOMMAND_OPTIONS  ${VALGRIND_COMMAND_OPTIONS} )
ENDIF()
IF ( USE_VALGRIND_MATLAB )
    STRING(REPLACE ";" " " MATLAB_DEBUGGER ${VALGRIND_COMMAND_OPTIONS})
    STRING(REPLACE ";" " " MATLAB_DEBUGGER ${MATLAB_DEBUGGER})
    STRING(REPLACE " " "\\ " MATLAB_DEBUGGER "valgrind ${MATLAB_DEBUGGER}")
    SET( MATLAB_DEBUGGER "${MATLAB_DEBUGGER}" )
ENDIF()


# Clear the binary directory and create an initial cache
CTEST_EMPTY_BINARY_DIRECTORY (${CTEST_BINARY_DIRECTORY})
FILE(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "CTEST_TEST_CTEST:BOOL=1")


# Set the configure options
SET( CTEST_OPTIONS )
IF ( CC AND CXX )
	SET( CTEST_OPTIONS "-DCMAKE_C_COMPILER=mpicc;-DCMAKE_CXX_COMPILER=mpicxx" )
	SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_C_FLAGS='${C_FLAGS}';-DCMAKE_CXX_FLAGS='${CXX_FLAGS}'" )
ENDIF()
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DLDLIBS:STRING=\"${LDLIBS}\";-DENABLE_GCOV:BOOL=${ENABLE_GCOV}" )
#SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_MPI=1;-DMPI_COMPILER:BOOL=true;-DMPIEXEC=${MPIEXEC}" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_MPI=1" )
#SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_EXT_MPI_FOR_SERIAL_TESTS:BOOL=false" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_DOXYGEN=0" )
IF ( USE_MATLAB )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_MATLAB:BOOL=true;-DMATLAB_DIRECTORY='${MATLAB_DIRECTORY}'" )
    # SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_MATLAB_LAPACK=${USE_MATLAB_LAPACK}" )
    IF ( MATLAB_DEBUGGER )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMATLAB_DEBUGGER:STRING='${MATLAB_DEBUGGER}'" )
    ENDIF()
ENDIF()
MESSAGE("Configure options:")
MESSAGE("   ${CTEST_OPTIONS}")


# Configure and run the tests
SET( CTEST_SITE ${HOSTNAME} )
CTEST_START("${CTEST_DASHBOARD}")
CTEST_UPDATE()
CTEST_CONFIGURE(
    BUILD   ${CTEST_BINARY_DIRECTORY}
    SOURCE  ${CTEST_SOURCE_DIRECTORY}
    OPTIONS "${CTEST_OPTIONS}"
)

# Run the configure, build and tests
CTEST_BUILD()
IF ( USE_VALGRIND_MATLAB )
    CTEST_TEST( INCLUDE MATLAB--test_hello_world  PARALLEL_LEVEL ${N_PROCS} )
ELSEIF ( USE_VALGRIND )
    CTEST_MEMCHECK( EXCLUDE procs   PARALLEL_LEVEL ${N_PROCS} )
ELSE()
    CTEST_TEST( EXCLUDE WEEKLY  PARALLEL_LEVEL ${N_PROCS} )
ENDIF()
IF( CTEST_COVERAGE_COMMAND )
    CTEST_COVERAGE()
ENDIF()


# Submit the results to oblivion
SET( CTEST_DROP_METHOD "http" )
SET( CTEST_DROP_SITE "oblivion.engr.colostate.edu" )
SET( CTEST_DROP_LOCATION "/CDash/submit.php?project=AMR-MHD" )
SET( CTEST_DROP_SITE_CDASH TRUE )
SET( DROP_SITE_CDASH TRUE )
CTEST_SUBMIT()


# Clean up
# exec_program("make distclean")


