MACRO( CONFIGURE_LINE_COVERAGE )
    SET( COVERAGE_LIBS )
    IF ( ENABLE_GCOV )
        ADD_DEFINITIONS( -fprofile-arcs -ftest-coverage )
        SET( COVERAGE_LIBS -lgcov -fprofile-arcs )
    ENDIF()
ENDMACRO()


# Macro to find and configure the MPI libraries
MACRO ( CONFIGURE_MPI )
    # Determine if we want to use MPI
    CHECK_ENABLE_FLAG(USE_MPI 1 )
    IF ( USE_MPI )
        # Check if we specified the MPI directory
        IF ( MPI_DIRECTORY )
            # Check the provided MPI directory for include files
            VERIFY_PATH( "${MPI_DIRECTORY}" )
            IF ( EXISTS "${MPI_DIRECTORY}/include/mpi.h" )
                SET( MPI_INCLUDE_PATH "${MPI_DIRECTORY}/include" )
            ELSEIF ( EXISTS "${MPI_DIRECTORY}/Inc/mpi.h" )
                SET( MPI_INCLUDE_PATH "${MPI_DIRECTORY}/Inc" )
            ELSE()
                MESSAGE( FATAL_ERROR "mpi.h not found in ${MPI_DIRECTORY}/include" )
            ENDIF ()
            INCLUDE_DIRECTORIES ( ${MPI_INCLUDE_PATH} )
            SET ( MPI_INCLUDE ${MPI_INCLUDE_PATH} )
            # Set MPI libraries
            IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
                FIND_LIBRARY( MSMPI_LIB     NAMES msmpi     PATHS "${MPI_DIRECTORY}/Lib/x64"    NO_DEFAULT_PATH )
                FIND_LIBRARY( MSMPI_LIB     NAMES msmpi     PATHS "${MPI_DIRECTORY}/Lib/amd64"  NO_DEFAULT_PATH )
                FIND_LIBRARY( MSMPIFEC_LIB  NAMES msmpifec  PATHS "${MPI_DIRECTORY}/Lib/x64"    NO_DEFAULT_PATH )
                FIND_LIBRARY( MSMPIFEC_LIB  NAMES msmpifec  PATHS "${MPI_DIRECTORY}/Lib/amd64"  NO_DEFAULT_PATH )
                FIND_LIBRARY( MSMPIFMC_LIB  NAMES msmpifmc  PATHS "${MPI_DIRECTORY}/Lib/x64"    NO_DEFAULT_PATH )
                FIND_LIBRARY( MSMPIFMC_LIB  NAMES msmpifmc  PATHS "${MPI_DIRECTORY}/Lib/amd64"  NO_DEFAULT_PATH )
                SET( MPI_LIBRARIES ${MSMPI_LIB} ${MSMPIFEC_LIB} ${MSMPIFMC_LIB} )
            ENDIF()
            # Set the mpi executable
            IF ( MPIEXEC ) 
                # User specified the MPI command directly, use as is
            ELSEIF ( MPIEXEC_CMD )
                # User specified the name of the MPI executable
                SET ( MPIEXEC ${MPI_DIRECTORY}/bin/${MPIEXEC_CMD} )
                IF ( NOT EXISTS ${MPIEXEC} )
                    MESSAGE( FATAL_ERROR "${MPIEXEC_CMD} not found in ${MPI_DIRECTORY}/bin" )
                ENDIF ()
            ELSE ()
                # Search for the MPI executable in the current directory
                FIND_PROGRAM ( MPIEXEC  NAMES mpiexec mpirun lamexec  PATHS ${MPI_DIRECTORY}/bin  NO_DEFAULT_PATH )
                IF ( NOT MPIEXEC )
                    MESSAGE( FATAL_ERROR "Could not locate mpi executable" )
                ENDIF()
            ENDIF ()
            # Set MPI flags
            IF ( NOT MPIEXEC_NUMPROC_FLAG )
                SET( MPIEXEC_NUMPROC_FLAG "-np" )
            ENDIF()
        ELSEIF ( MPI_COMPILER )
            # The mpi compiler should take care of everything
            IF ( MPI_INCLUDE )
                INCLUDE_DIRECTORIES( ${MPI_INCLUDE} )
            ENDIF()
        ELSE()
            # Perform the default search for MPI
            INCLUDE ( FindMPI )
            IF ( NOT MPI_FOUND )
                MESSAGE( "  MPI_INCLUDE = ${MPI_INCLUDE}" )
                MESSAGE( "  MPI_LINK_FLAGS = ${MPI_LINK_FLAGS}" )
                MESSAGE( "  MPI_LIBRARIES = ${MPI_LIBRARIES}" )
                MESSAGE( FATAL_ERROR "Did not find MPI" )
            ENDIF ()
            INCLUDE_DIRECTORIES( "${MPI_INCLUDE_PATH}" )
            SET( MPI_INCLUDE "${MPI_INCLUDE_PATH}" )
        ENDIF()
        # Check if we need to use MPI for serial tests
        CHECK_ENABLE_FLAG( USE_MPI_FOR_SERIAL_TESTS 0 )
        # Set defaults if they have not been set
        IF ( NOT MPIEXEC )
            SET( MPIEXEC mpirun )
        ENDIF()
        IF ( NOT MPIEXEC_NUMPROC_FLAG )
            SET( MPIEXEC_NUMPROC_FLAG "-np" )
        ENDIF()
        # Set the definitions
        ADD_DEFINITIONS( "-D USE_MPI" )  
        MESSAGE( "Using MPI" )
        MESSAGE( "  MPIEXEC = ${MPIEXEC}" )
        MESSAGE( "  MPIEXEC_NUMPROC_FLAG = ${MPIEXEC_NUMPROC_FLAG}" )
        MESSAGE( "  MPI_INCLUDE = ${MPI_INCLUDE}" )
        MESSAGE( "  MPI_LINK_FLAGS = ${MPI_LINK_FLAGS}" )
        MESSAGE( "  MPI_LIBRARIES = ${MPI_LIBRARIES}" )
    ELSE()
        SET( USE_MPI_FOR_SERIAL_TESTS 0 )
        SET( MPIEXEC "" )
        SET( MPIEXEC_NUMPROC_FLAG "" )
        SET( MPI_INCLUDE "" )
        SET( MPI_LINK_FLAGS "" )
        SET( MPI_LIBRARIES "" )
        MESSAGE( "Not using MPI, all parallel tests will be disabled" )
    ENDIF()
ENDMACRO()


# Macro to configure system-specific libraries and flags
MACRO ( CONFIGURE_SYSTEM )
    # First check/set the compile mode
    IF ( COMPILE_MODE )
        MESSAGE(FATAL_ERROR "COMPILE_MODE is obsolete, please set CMAKE_BUILD_TYPE")
    ENDIF()
    VERIFY_VARIABLE( "CMAKE_BUILD_TYPE" )
    IF ( COMPILE_MODE )
        IF ( ${COMPILE_MODE} STREQUAL "debug" )
            SET(CMAKE_BUILD_TYPE "Debug")
        ELSEIF ( ${COMPILE_MODE} STREQUAL "optimized" )
            SET(CMAKE_BUILD_TYPE "Release")
        ELSE()
            MESSAGE( FATAL_ERROR "COMPILE_MODE must be either debug or optimized" )
        ENDIF()
    ENDIF()
    # Remove extra library links
    # Get the compiler
    SET_COMPILER ()
    CHECK_ENABLE_FLAG( USE_STATIC 0 )
    # Add system dependent flags
    MESSAGE("System is: ${CMAKE_SYSTEM_NAME}")
    IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        # Windows specific system libraries
        SET( SYSTEM_PATHS "C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib/x64" 
                          "C:/Program Files (x86)/Microsoft Visual Studio 8/VC/PlatformSDK/Lib/AMD64" 
                          "C:/Program Files (x86)/Microsoft Visual Studio 12.0/Common7/Packages/Debugger/X64" )
        FIND_LIBRARY ( PSAPI_LIB    NAMES Psapi    PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
        FIND_LIBRARY ( DBGHELP_LIB  NAMES DbgHelp  PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
        IF ( PSAPI_LIB ) 
            ADD_DEFINITIONS( -D PSAPI )
            SET( SYSTEM_LIBS ${PSAPI_LIB} )
        ENDIF()
        IF ( DBGHELP_LIB ) 
            ADD_DEFINITIONS( -D DBGHELP )
            SET( SYSTEM_LIBS ${DBGHELP_LIB} )
        ENDIF()
        MESSAGE("System libs: ${SYSTEM_LIBS}")
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
        # Linux specific system libraries
        SET( SYSTEM_LIBS -lz -lpthread -ldl )
        IF ( NOT USE_STATIC )
            SET( SYSTEM_LDFLAGS ${SYSTEM_LDFLAGS} -rdynamic )   # Needed for backtrace to print function names
        ENDIF()
        IF ( USING_GCC )
            SET( SYSTEM_LIBS ${SYSTEM_LIBS} -lgfortran )
            SET(CMAKE_C_FLAGS   " ${CMAKE_C_FLAGS} -fPIC" )
            SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fPIC" )
        ENDIF()
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" )
        # Max specific system libraries
        SET( SYSTEM_LIBS -lz -lpthread -ldl )
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Generic" )
        # Generic system libraries
    ELSE()
        MESSAGE( FATAL_ERROR "OS not detected" )
    ENDIF()
    # Set the compile flags based on the build
    SET_COMPILER_FLAGS()
    # Add the static flag if necessary
    IF ( USE_STATIC )
        SET_STATIC_FLAGS()
    ENDIF()
ENDMACRO ()


# Macro to configure matlab
MACRO( CONFIGURE_MATLAB )
    CHECK_ENABLE_FLAG( USE_MATLAB 0 )
    SET( LIB_TYPE STATIC )      # By default we want to build static libraries
    IF ( USE_MATLAB )
        SET( LIB_TYPE SHARED )  # If we are using MATLAB, we must build dynamic libraries for ProfilerApp to work properly
        IF ( MATLAB_DIRECTORY )
            VERIFY_PATH ( ${MATLAB_DIRECTORY} )
        ELSE()
            MESSAGE( FATAL_ERROR "Default search for MATLAB is not supported, please specify MATLAB_DIRECTORY")
        ENDIF()
        VERIFY_PATH ( ${MATLAB_DIRECTORY}/extern )
        # Get the mex extension for the current architecture
        IF (CMAKE_SIZEOF_VOID_P MATCHES 8) 
            SET( SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_64" ) 
        ELSE()
            SET( SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_32" ) 
        ENDIF() 
        IF ( ${SYSTEM_NAME} STREQUAL "Linux_32" ) 
            SET( MEX_EXTENSION "mexglx" )
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Linux_64" ) 
            SET( MEX_EXTENSION "mexa64" )
            SET( MATLAB_EXTERN "${MATLAB_DIRECTORY}/bin/glnxa64" )
            SET( MATLAB_OS "${MATLAB_DIRECTORY}/sys/os/glnxa64" )
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Darwin_32" ) 
            SET( MEX_EXTENSION "mexmac" )
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Darwin_64" ) 
            SET( MEX_EXTENSION "mexmaci64" )
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Windows_32" ) 
            SET( MEX_EXTENSION "mexw32" )
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Windows_64" ) 
            SET( MEX_EXTENSION "mexw64" )
            SET( MATLAB_EXTERN "${MATLAB_DIRECTORY}/extern/lib/win64/microsoft" )
        ELSE ( ) 
            MESSAGE( FATAL_ERROR "Unkown OS ${SYSTEM_NAME}" )
        ENDIF()
        IF ( (NOT MEX_EXTENSION) OR (NOT MATLAB_EXTERN) )
            MESSAGE( FATAL_ERROR "Some MATLAB paths are not set" )
        ENDIF()
        VERIFY_PATH( ${MATLAB_EXTERN} )
        SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
        SET(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} "${MATLAB_EXTERN}" "${MATLAB_OS}" )
        SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 
        MESSAGE( "Using MATLAB" )
        MESSAGE( "   MATLAB_DIRECTORY = ${MATLAB_DIRECTORY}" )
    ENDIF()
ENDMACRO()


# Macro to configure TimerUtility-specific options
MACRO( CONFIGURE_TIMER )
    SET(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} ${TIMER_INSTALL_DIR}/lib )
    # Set the maximum number of processors for the tests
    IF ( NOT TEST_MAX_PROCS )
        SET( TEST_MAX_PROCS 32 )
    ENDIF()
    # Check if we want to enable the traps for new/delete
    CHECK_ENABLE_FLAG( OVERLOAD_NEW 1 )
    IF ( NOT OVERLOAD_NEW )
        ADD_DEFINITIONS( -DDISABLE_NEW_OVERLOAD )
    ENDIF()
ENDMACRO()


