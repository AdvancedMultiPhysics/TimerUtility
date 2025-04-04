# ctest script for building, running, and submitting the test results 
# Usage:  ctest -S script,build
#   build = debug / optimized / valgrind / valgrind-matlab / continuous
# Note: this test will use use the number of processors defined in the variable N_PROCS,
#   the enviornmental variable N_PROCS, or the number of processors availible (if not specified)


# Set the Project variables
SET( PROJ Profiler )


# Set platform specific variables
SITE_NAME( HOSTNAME )
STRING( REGEX REPLACE "-(ext|login)(..|.)" "" HOSTNAME "${HOSTNAME}" )
SET( USE_MPI                $ENV{USE_MPI}               )
SET( CC                     $ENV{CC}                    )
SET( CXX                    $ENV{CXX}                   )
SET( FORTRAN                $ENV{FORTRAN}               )
SET( CFLAGS                 $ENV{CFLAGS}                )
SET( CXXFLAGS               $ENV{CXXFLAGS}              )
SET( FFLAGS                 $ENV{FFLAGS}                )
SET( CXX_STD                $ENV{CXX_STD}               )
SET( MPIEXEC                $ENV{MPIEXEC}               )
SET( USE_MATLAB             $ENV{USE_MATLAB}            )
SET( MATLAB_DIRECTORY       $ENV{MATLAB_DIRECTORY}      )
SET( COVERAGE_COMMAND       $ENV{COVERAGE_COMMAND}      )
SET( VALGRIND_COMMAND       $ENV{VALGRIND_COMMAND}      )
SET( CMAKE_MAKE_PROGRAM     $ENV{CMAKE_MAKE_PROGRAM}    )
SET( CTEST_CMAKE_GENERATOR  $ENV{CTEST_CMAKE_GENERATOR} )
SET( LDLIBS                 $ENV{LDLIBS}                )
SET( LDFLAGS                $ENV{LDFLAGS}               )
SET( MPI_DIRECTORY          $ENV{MPI_DIRECTORY}         )
SET( MPI_INCLUDE            $ENV{MPI_INCLUDE}           )
SET( MPI_LINK_FLAGS        "$ENV{MPI_LINK_FLAGS}"       )
SET( MPI_LIBRARIES          $ENV{MPI_LIBRARIES}         )
SET( MPIEXEC                $ENV{MPIEXEC}               )
SET( BUILD_SERIAL           $ENV{BUILD_SERIAL}          )
SET( SKIP_TESTS             $ENV{SKIP_TESTS}            )
SET( BUILDNAME_POSTFIX     "$ENV{BUILDNAME_POSTFIX}"    )
SET( QWT_URL               "$ENV{QWT_URL}"              )
SET( QWT_SRC_DIR           "$ENV{QWT_SRC_DIR}"          )
SET( CTEST_SITE            "$ENV{CTEST_SITE}"           )
SET( CTEST_URL             "$ENV{CTEST_URL}"            )
SET( DISABLE_NEW_OVERLOAD   $ENV{DISABLE_NEW_OVERLOAD}  )
SET( DISABLE_GOLD           $ENV{DISABLE_GOLD}          )


# Add the C++ version to the project name
IF ( CXX_STD )
    SET( PROJ "${PROJ}-C++${CXX_STD}" )
ENDIF()
IF ( QWT_URL OR QWT_SRC_DIR )
    SET( PROJ "${PROJ}-GUI" )
ENDIF()


# Get the source directory based on the current directory
IF ( NOT ${PROJ}_SOURCE_DIR )
    SET( ${PROJ}_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." )
ENDIF()
IF ( NOT CMAKE_MAKE_PROGRAM )
    SET( CMAKE_MAKE_PROGRAM make )
ENDIF()


# Set default options
SET( CTEST_BUILD_NAME "${PROJ}" )
SET( CMAKE_BUILD_TYPE "Release" )
SET( CTEST_COVERAGE_COMMAND )
SET( ENABLE_GCOV "false" )
SET( USE_VALGRIND FALSE )
SET( USE_VALGRIND_MATLAB FALSE )
SET( CTEST_DASHBOARD "Nightly" )


# Check that we specified the build type to run
IF( NOT CTEST_SCRIPT_ARG )
    MESSAGE(FATAL_ERROR "No build specified: ctest -S /path/to/script,build (debug/optimized/valgrind/continuous")
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "continuous" )
    SET( CTEST_DASHBOARD "Continuous" )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "debug" )
    SET( CTEST_BUILD_NAME "${PROJ}-debug" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( CTEST_COVERAGE_COMMAND ${COVERAGE_COMMAND} )
    SET( ENABLE_GCOV "true" )
ELSEIF( (${CTEST_SCRIPT_ARG} STREQUAL "optimized") OR (${CTEST_SCRIPT_ARG} STREQUAL "opt") )
    SET( CTEST_BUILD_NAME "${PROJ}-opt" )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "valgrind" )
    SET( CTEST_BUILD_NAME "${PROJ}-valgrind" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( USE_VALGRIND TRUE )
    SET( USE_MATLAB 0 )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "valgrind-matlab" )
    SET( CTEST_BUILD_NAME "${PROJ}-valgrind-matlab" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( USE_VALGRIND FALSE )
    SET( USE_VALGRIND_MATLAB TRUE )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "doc" )
    SET( CTEST_BUILD_NAME "${PROJ}-doc" )
    SET( BUILD_ONLY_DOCS "true" )
ELSE()
    MESSAGE(FATAL_ERROR "Invalid build (${CTEST_SCRIPT_ARG}): ctest -S /path/to/script,build (debug/opt/valgrind/continuous")
ENDIF()
IF ( BUILDNAME_POSTFIX )
    SET( CTEST_BUILD_NAME "${CTEST_BUILD_NAME}-${BUILDNAME_POSTFIX}" )
ENDIF()
IF ( NOT COVERAGE_COMMAND )
    SET( ENABLE_GCOV "false" )
ENDIF()


# Set the number of processors
IF( NOT DEFINED N_PROCS )
    SET( N_PROCS $ENV{N_PROCS} )
ENDIF()
IF ( NOT DEFINED N_PROCS )
    SET(N_PROCS 1)
    # Linux:
    SET(cpuinfo_file "/proc/cpuinfo")
    IF(EXISTS "${cpuinfo_file}")
        FILE(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
        list(LENGTH procs N_PROCS)
    ENDIF()
    # Mac:
    IF(APPLE)
        find_program(cmd_sys_pro "sysctl")
        if(cmd_sys_pro)
            execute_process(COMMAND ${cmd_sys_pro} hw.physicalcpu OUTPUT_VARIABLE info)
            STRING(REGEX REPLACE "^.*hw.physicalcpu: ([0-9]+).*$" "\\1" N_PROCS "${info}")
        ENDIF()
    ENDIF()
    # Windows:
    IF(WIN32)
        SET(N_PROCS "$ENV{NUMBER_OF_PROCESSORS}")
    ENDIF()
ENDIF()


# Set the nightly start time
# This controls the version of a checkout from cvs/svn (ignored for mecurial/git)
# This does not control the start of the day displayed on CDash, that is controled by the CDash project settings
SET( NIGHTLY_START_TIME "$ENV{NIGHTLY_START_TIME}" )
IF ( NOT NIGHTLY_START_TIME )
    SET( NIGHTLY_START_TIME "18:00:00 EST" )
ENDIF()
SET( CTEST_NIGHTLY_START_TIME ${NIGHTLY_START_TIME} )


# Set basic variables
SET( CTEST_PROJECT_NAME "${PROJ}" )
SET( CTEST_SOURCE_DIRECTORY "${${PROJ}_SOURCE_DIR}" )
SET( CTEST_BINARY_DIRECTORY "." )
SET( CTEST_CUSTOM_MAXIMUM_NUMBER_OF_ERRORS 500 )
SET( CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 500 )
SET( CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 10000 )
SET( CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 10000 )
SET( CTEST_COMMAND "\"${CTEST_EXECUTABLE_NAME}\" -D ${CTEST_DASHBOARD}" )
IF ( BUILD_SERIAL )
    SET( CTEST_BUILD_COMMAND "${CMAKE_MAKE_PROGRAM} -i install" )
ELSEIF ( "${CMAKE_MAKE_PROGRAM}" STREQUAL "nmake" )
    SET( CTEST_BUILD_COMMAND "${CMAKE_MAKE_PROGRAM} -i install" )
ELSE()
    SET( CTEST_BUILD_COMMAND "${CMAKE_MAKE_PROGRAM} -i -j ${N_PROCS} install" )
ENDIF()
SET( CTEST_CUSTOM_WARNING_EXCEPTION 
    "has no symbols"
    "the table of contents is empty"
    "This object file does not define any previously undefined public symbols"
    "Note: No relevant classes found. No output generated."
    "The version of gcc is not supported. The version currently supported with MEX is"
)
SET( CTEST_CUSTOM_ERROR_EXCEPTION 
    "Note: No relevant classes found. No output generated."
)


# Set timeouts: 5 minutes for debug, 2 for opt, and 30 minutes for valgrind
IF ( USE_VALGRIND )
    SET( CTEST_TEST_TIMEOUT 1800 )
ELSEIF( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
    SET( CTEST_TEST_TIMEOUT 300 )
ELSE()
    SET( CTEST_TEST_TIMEOUT 120 )
ENDIF()


# Set valgrind options
#SET (VALGRIND_COMMAND_OPTIONS "--tool=memcheck --leak-check=yes --track-fds=yes --num-callers=50 --show-reachable=yes --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --suppressions=${${PROJ}_SOURCE_DIR}/ValgrindSuppresionFile" )
SET( VALGRIND_COMMAND_OPTIONS  "--tool=memcheck --leak-check=yes --track-fds=yes --num-callers=50 --show-reachable=yes --suppressions=${${PROJ}_SOURCE_DIR}/ValgrindSuppresionFile" )
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
EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E remove -f CMakeCache.txt )
EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E remove_directory CMakeFiles )
FILE(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "CTEST_TEST_CTEST:BOOL=1")


# Set the configure options
SET( CTEST_OPTIONS "-DUSE_MPI=${USE_MPI}")
IF ( CC )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_C_COMPILER=${CC}" )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_C_FLAGS='${CFLAGS}'" )
ENDIF()
IF ( CXX )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_CXX_COMPILER=${CXX}" )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_CXX_FLAGS='${CXXFLAGS}';-DCXX_STD='${CXX_STD}'" )
ENDIF()
IF ( CC AND CXX )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_Fortran_COMPILER=${FORTRAN}" )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_Fortran_FLAGS='${FFLAGS}'" )
ENDIF()
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DDISABLE_GOLD=${DISABLE_GOLD}" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DLDFLAGS:STRING='${FLAGS}';-DLDLIBS:STRING='${LDLIBS}'" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DENABLE_GCOV:BOOL=${ENABLE_GCOV}" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_DOXYGEN=0" )
IF ( DISABLE_NEW_OVERLOAD )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DDISABLE_NEW_OVERLOAD=1" )
ENDIF()
IF ( USE_MATLAB )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_MATLAB=1;-DMATLAB_DIRECTORY='${MATLAB_DIRECTORY}'" )
    IF ( MATLAB_DEBUGGER )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMATLAB_DEBUGGER:STRING='${MATLAB_DEBUGGER}'" )
    ENDIF()
ELSE()
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_MATLAB=0" )
ENDIF()
IF ( USE_MPI )
    IF ( MPI_DIRECTORY )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMPI_DIRECTORY=${MPI_DIRECTORY}" )
    ENDIF()
    IF ( MPI_INCLUDE )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMPI_INCLUDE='${MPI_INCLUDE}'" )
    ENDIF()
    IF ( MPI_LINK_FLAGS )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMPI_LINK_FLAGS='${MPI_LINK_FLAGS}'" )
    ENDIF()
    IF ( MPI_LIBRARIES )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMPI_LIBRARIES='${MPI_LIBRARIES}'" )
    ENDIF()
    IF ( MPIEXEC )
        SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMPIEXEC='${MPIEXEC}'" )
    ENDIF()
ENDIF()
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DQWT_URL='${QWT_URL}';-DQWT_SRC_DIR='${QWT_SRC_DIR}'" )
MESSAGE("Configure options:")
MESSAGE("   ${CTEST_OPTIONS}")


# Configure the drop site
IF ( NOT CTEST_SITE )
    SET( CTEST_SITE ${HOSTNAME} )
ENDIF()
IF ( NOT CTEST_URL )
    MESSAGE( FATAL_ERROR "CTEST_URL is not set" )
ENDIF()
STRING( REPLACE "PROJECT" "TimerUtility" CTEST_URL "${CTEST_URL}" )
SET( CTEST_SUBMIT_URL "${CTEST_URL}" )


# Configure update
IF ( NOT CTEST_GIT_COMMAND )
    SET( CTEST_GIT_COMMAND "$ENV{CTEST_GIT_COMMAND}" )
ENDIF()
IF ( NOT CTEST_GIT_COMMAND )
    FIND_PROGRAM( CTEST_GIT_COMMAND git )
ENDIF()
IF ( NOT CTEST_GIT_COMMAND )
    SET( CTEST_GIT_COMMAND git )
ENDIF()
SET( CTEST_UPDATE_COMMAND ${CTEST_GIT_COMMAND} )
SET( CTEST_UPDATE_OPTIONS )
MESSAGE( "CTEST_UPDATE_COMMAND=${CTEST_UPDATE_COMMAND}" )
MESSAGE( "CTEST_UPDATE_OPTIONS=${CTEST_UPDATE_OPTIONS}" )


# Configure and run the tests
CTEST_START( "${CTEST_DASHBOARD}" )
CTEST_UPDATE()
CTEST_CONFIGURE(
    BUILD   "${CTEST_BINARY_DIRECTORY}"
    SOURCE  "${CTEST_SOURCE_DIRECTORY}"
    OPTIONS "${CTEST_OPTIONS}"
)


# Run the configure/build/test
CTEST_BUILD()
IF ( SKIP_TESTS )
    # Do not run tests
    SET( CTEST_COVERAGE_COMMAND )
ELSEIF ( USE_VALGRIND_MATLAB )
    CTEST_TEST( INCLUDE MATLAB  EXCLUDE WEEKLY  PARALLEL_LEVEL ${N_PROCS} )
ELSEIF ( USE_VALGRIND )
    CTEST_MEMCHECK( EXCLUDE "(procs)"  PARALLEL_LEVEL ${N_PROCS} )
ELSE()
    CTEST_TEST( PARALLEL_LEVEL ${N_PROCS} )
ENDIF()
IF( CTEST_COVERAGE_COMMAND )
    CTEST_COVERAGE()
ENDIF()


# Submit the results to CDash
CTEST_SUBMIT()


# Write a message to test for success in the ctest-builder
MESSAGE( "ctest_script ran to completion" )


