INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)


FUNCTION( CONFIGURE_LINE_COVERAGE )
    SET( COVERAGE_FLAGS )
    SET( COVERAGE_LIBS )
    IF ( ENABLE_GCOV )
        SET( COVERAGE_FLAGS -DUSE_GCOV )
        SET( CMAKE_REQUIRED_FLAGS ${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage )
        CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" profile-arcs )
        IF ( profile-arcs )
            SET( COVERAGE_FLAGS "${COVERAGE_FLAGS} -fprofile-arcs -ftest-coverage" )
            SET( COVERAGE_LIBS ${COVERAGE_LIBS} -fprofile-arcs )
        ENDIF()
        SET( CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS} -lgcov" )
        CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" lgcov )
        IF ( lgcov )
            SET( COVERAGE_LIBS -lgcov ${COVERAGE_LIBS} )
        ENDIF()
        MESSAGE("Enabling coverage:")
        MESSAGE("   COVERAGE_FLAGS = ${COVERAGE_FLAGS}")
        MESSAGE("   COVERAGE_LIBS = ${COVERAGE_LIBS}")
        ADD_DEFINITIONS( ${COVERAGE_FLAGS} )
        SET( COVERAGE_FLAGS ${COVERAGE_FLAGS} PARENT_SCOPE )
        SET( COVERAGE_LIBS  ${COVERAGE_LIBS}  PARENT_SCOPE )
    ENDIF()
ENDFUNCTION()


# Macro to find and configure the MPI libraries
MACRO( CONFIGURE_MPI )
    CHECK_ENABLE_FLAG( USE_MPI 1 )
    MESSAGE("MPIEXEC = ${MPIEXEC}")
    IF ( USE_MPI )
        MESSAGE( "Configuring MPI" )
        IF ( MPIEXEC )
            SET( MPIEXEC_EXECUTABLE ${MPIEXEC} )
        ENDIF()
        IF ( NOT MPI_SKIP_SEARCH )
            FIND_PACKAGE( MPI )
        ELSE()
            # Write mpi test
            SET( MPI_TEST_SRC "${CMAKE_CURRENT_BINARY_DIR}/test_mpi.cpp" )
            FILE(WRITE  ${MPI_TEST_SRC} "#include <mpi.h>\n" )
            FILE(APPEND ${MPI_TEST_SRC} "int main(int argc, char** argv) {\n" )
            FILE(APPEND ${MPI_TEST_SRC} "    MPI_Init(NULL, NULL);\n")
            FILE(APPEND ${MPI_TEST_SRC} "    MPI_Finalize();\n" )
            FILE(APPEND ${MPI_TEST_SRC} "}\n" )
            # Test the compile
            FOREACH ( tmp C CXX Fortran )
                IF ( CMAKE_${tmp}_COMPILER )
                    SET( TMP_FLAGS -DINCLUDE_DIRECTORIES=${MPI_CXX_INCLUDE_PATH} )
                    TRY_COMPILE( MPI_TEST_${tmp} ${CMAKE_CURRENT_BINARY_DIR} ${MPI_TEST_SRC}
                        CMAKE_FLAGS ${TMP_FLAGS}
                        LINK_OPTIONS ${MPI_CXX_LINK_FLAGS}
                        LINK_LIBRARIES ${MPI_CXX_LIBRARIES}
                        OUTPUT_VARIABLE OUT_TXT)
                    IF ( NOT ${MPI_TEST} )
                        MESSAGE( FATAL_ERROR "Skipping MPI search and default compile fails:\n${OUT_TXT}" )
                    ENDIF()
                    SET( MPI_C_FOUND TRUE )
                    SET( MPI_CXX_FOUND TRUE )
                    SET( MPI_Fortran_FOUND TRUE )
                ENDIF()
            ENDFOREACH()
        ENDIF()
        SET( MPI_LANG C )
        FOREACH( tmp ${MPI_LANG} )
            INCLUDE_DIRECTORIES( ${MPI_${tmp}_INCLUDE_PATH} )
            SET( SYSTEM_LIBS ${MPI_${tmp}_LIBRARIES} ${SYSTEM_LIBS} )
            STRING( STRIP "${MPI_${tmp}_COMPILE_FLAGS}" MPI_${tmp}_COMPILE_FLAGS )
            STRING( STRIP "${MPI_${tmp}_LINK_FLAGS}" MPI_${tmp}_LINK_FLAGS )
            STRING( STRIP "${MPI_${tmp}_LIBRARIES}" MPI_${tmp}_LIBRARIES )
            MESSAGE( "   MPI_${tmp}_FOUND = ${MPI_${tmp}_FOUND}" )
            MESSAGE( "   MPI_${tmp}_COMPILER = ${MPI_${tmp}_COMPILER}" )
            MESSAGE( "   MPI_${tmp}_COMPILE_FLAGS = ${MPI_${tmp}_COMPILE_FLAGS}" )
            MESSAGE( "   MPI_${tmp}_INCLUDE_PATH = ${MPI_${tmp}_INCLUDE_PATH}" )
            MESSAGE( "   MPI_${tmp}_LINK_FLAGS = ${MPI_${tmp}_LINK_FLAGS}" )
            MESSAGE( "   MPI_${tmp}_LIBRARIES = ${MPI_${tmp}_LIBRARIES}" )
        ENDFOREACH()
        MESSAGE( "   MPIEXEC = ${MPIEXEC}" )
        MESSAGE( "   MPIEXEC_NUMPROC_FLAG = ${MPIEXEC_NUMPROC_FLAG}" )
        MESSAGE( "   MPIEXEC_PREFLAGS = ${MPIEXEC_PREFLAGS}" )
        MESSAGE( "   MPIEXEC_POSTFLAGS = ${MPIEXEC_POSTFLAGS}" )
        IF ( NOT MPI_C_FOUND AND NOT MPI_CXX_FOUND AND NOT MPI_Fortran_FOUND )
            MESSAGE( FATAL_ERROR "MPI not found" )
        ENDIF()
        ADD_DEFINITIONS( -DUSE_MPI )
    ENDIF()
    IF ( USE_MPI AND NOT MPIEXEC )
        MESSAGE( FATAL_ERROR "Unable to find MPIEXEC, please set it before continuing" )
    ENDIF()
ENDMACRO()


# Macro to configure system-specific libraries and flags
MACRO( CONFIGURE_SYSTEM )
    # First check/set the compile mode
    IF( NOT CMAKE_BUILD_TYPE )
        MESSAGE(FATAL_ERROR "CMAKE_BUILD_TYPE is not set")
    ENDIF()
    # Remove extra library links
    # Get the compiler
    IDENTIFY_COMPILER()
    CHECK_ENABLE_FLAG( USE_STATIC 0 )
    # Add system dependent flags
    MESSAGE("System is: ${CMAKE_SYSTEM_NAME}")
    IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        # Windows specific system libraries
        SET( SYSTEM_PATHS "C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib/x64" 
                          "C:/Program Files (x86)/Microsoft Visual Studio 8/VC/PlatformSDK/Lib/AMD64" 
                          "C:/Program Files (x86)/Microsoft Visual Studio 12.0/Common7/Packages/Debugger/X64" )
        FIND_LIBRARY( PSAPI_LIB    NAMES Psapi    PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
        FIND_LIBRARY( DBGHELP_LIB  NAMES DbgHelp  PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
        FIND_LIBRARY( DBGHELP_LIB  NAMES DbgHelp )
        IF ( PSAPI_LIB ) 
            ADD_DEFINITIONS( -D PSAPI )
            SET( SYSTEM_LIBS ${PSAPI_LIB} )
        ENDIF()
        IF ( DBGHELP_LIB ) 
            ADD_DEFINITIONS( -D DBGHELP )
            SET( SYSTEM_LIBS ${DBGHELP_LIB} )
        ELSE()
            MESSAGE( WARNING "Did not find DbgHelp, stack trace will not be availible" )
        ENDIF()
        MESSAGE("System libs: ${SYSTEM_LIBS}")
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
        # Linux specific system libraries
        SET( SYSTEM_LIBS -lpthread -ldl )
        IF ( NOT USE_STATIC )
            # Try to add rdynamic so we have names in backtrace
            SET( CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS} -rdynamic" )
            CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" rdynamic )
            IF ( rdynamic )
                SET( SYSTEM_LDFLAGS ${SYSTEM_LDFLAGS} -rdynamic )
            ENDIF()
        ENDIF()
        IF ( USING_GCC )
            SET( SYSTEM_LIBS ${SYSTEM_LIBS} -lgfortran )
            SET(CMAKE_C_FLAGS   " ${CMAKE_C_FLAGS} -fPIC" )
            SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fPIC" )
        ENDIF()
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" )
        # Max specific system libraries
        SET( SYSTEM_LIBS -lpthread -ldl )
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Generic" )
        # Generic system libraries
    ELSE()
        MESSAGE( FATAL_ERROR "OS not detected" )
    ENDIF()
    # Set the compile flags based on the build
    SET_CXX_STD()
    SET_COMPILER_FLAGS()
    # Add the static flag if necessary
    IF ( USE_STATIC )
        SET_STATIC_FLAGS()
    ENDIF()
    # Print some flags
    MESSAGE( "LDLIBS = ${LDLIBS}" )
ENDMACRO ()


# Macro to configure matlab
MACRO( CONFIGURE_MATLAB )
    IF ( NOT DEFINED USE_MATLAB AND DEFINED MATLAB_DIRECTORY )
        SET( USE_MATLAB 1 )
    ENDIF()
    NULL_USE( MATLAB_DIRECTORY )
    CHECK_ENABLE_FLAG( USE_MATLAB 0 )
    IF ( NOT DEFINED USE_STATIC OR USE_STATIC )
        SET( LIB_TYPE STATIC )
    ENDIF()
    IF ( USE_MATLAB )
        SET( LIB_TYPE SHARED )  # If we are using MATLAB, we must build dynamic libraries for ProfilerApp to work properly
        IF ( MATLAB_DIRECTORY )
            VERIFY_PATH( ${MATLAB_DIRECTORY} )
        ELSE()
            MESSAGE( FATAL_ERROR "Default search for MATLAB is not supported, please specify MATLAB_DIRECTORY")
        ENDIF()
        SET( Matlab_ROOT_DIR ${MATLAB_DIRECTORY} )
        FIND_PACKAGE( Matlab REQUIRED MAIN_PROGRAM MX_LIBRARY ENG_LIBRARY MEX_COMPILER )
        SET( MATLABPATH ${${PROJ}_INSTALL_DIR}/mex )
        IF (CMAKE_SIZEOF_VOID_P MATCHES 8) 
            SET( SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_64" ) 
        ELSE()
            SET( SYSTEM_NAME "${CMAKE_SYSTEM_NAME}_32" ) 
        ENDIF() 
        IF ( ${SYSTEM_NAME} STREQUAL "Linux_32" ) 
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Linux_64" ) 
            SET( MATLAB_EXTERN "${Matlab_ROOT_DIR}/bin/glnxa64" )
            SET( MATLAB_OS "${Matlab_ROOT_DIR}/sys/os/glnxa64" )
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Darwin_32" ) 
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Darwin_64" ) 
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Windows_32" ) 
        ELSEIF ( ${SYSTEM_NAME} STREQUAL "Windows_64" ) 
            SET( MATLAB_EXTERN "${Matlab_ROOT_DIR}/extern/lib/win64/microsoft" 
                "${Matlab_ROOT_DIR}/bin/win64" )
        ELSE ( ) 
            MESSAGE( FATAL_ERROR "Unkown OS ${SYSTEM_NAME}" )
        ENDIF()
        # Check if we need to include MATLAB's libstdc++
        FIND_LIBRARY( MATLAB_BLAS_LIBRARY   NAMES mwblas          PATHS ${MATLAB_EXTERN}  NO_DEFAULT_PATH )
        FIND_LIBRARY( MATLAB_BLAS_LIBRARY   NAMES libmwblas.dll   PATHS ${MATLAB_EXTERN}  NO_DEFAULT_PATH )
        FIND_LIBRARY( MATLAB_LAPACK_LIBRARY NAMES mwlapack        PATHS ${MATLAB_EXTERN}  NO_DEFAULT_PATH )
        FIND_LIBRARY( MATLAB_LAPACK_LIBRARY NAMES libmwlapack.dll PATHS ${MATLAB_EXTERN}  NO_DEFAULT_PATH )
        SET( CMAKE_REQUIRED_FLAGS ${CMAKE_CXX_FLAGS} )
        SET( CMAKE_REQUIRED_LIBRARIES ${MATLAB_LAPACK_LIBRARY} ${MATLAB_BLAS_LIBRARY} )
        CHECK_CXX_SOURCE_COMPILES(
            "#include \"${Matlab_ROOT_DIR}/extern/include/tmwtypes.h\"
             #include \"${Matlab_ROOT_DIR}/extern/include/lapack.h\"
             int main() {
                dgesv( nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                return 0;
             }" MATLAB_LAPACK_LINK )
        IF ( MATLAB_LAPACK_LINK )
            # Use system libc++
            FIND_LIBRARY( MEX_LIBCXX  NAMES stdc++ )
            FIND_LIBRARY( MEX_LIBCXX  NAMES libstdc++.so.6 )
            SET( MEX_LIBCXX ${MEX_LIBCXX} )
        ELSE()
            # Try to link MATLAB's libc++
            FIND_LIBRARY( MEX_LIBCXX  NAMES stdc++          PATHS ${MATLAB_OS}      NO_DEFAULT_PATH )
            FIND_LIBRARY( MEX_LIBCXX  NAMES libstdc++.so.6  PATHS ${MATLAB_OS}      NO_DEFAULT_PATH )
            SET( CMAKE_REQUIRED_LIBRARIES ${MATLAB_LAPACK_LIBRARY} ${MATLAB_BLAS_LIBRARY} ${MEX_LIBCXX} )
            CHECK_CXX_SOURCE_COMPILES(
                "#include \"${Matlab_ROOT_DIR}/extern/include/tmwtypes.h\"
                 #include \"${Matlab_ROOT_DIR}/extern/include/lapack.h\"
                 int main() {
                    dgesv( nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                    return 0;
                 }" MATLAB_LAPACK_LINK2 )
            IF ( NOT MATLAB_LAPACK_LINK2 )
                MESSAGE( FATAL_ERROR "Unable to link with MATLAB's LAPACK" )
            ENDIF()
            SET( MATLAB_LAPACK ${MATLAB_LAPACK_LIBRARY} ${MATLAB_BLAS_LIBRARY} ${MEX_LIBCXX} )
        ENDIF()
        MESSAGE( "Using MATLAB" )
        MESSAGE( "   Matlab_ROOT_DIR = ${Matlab_ROOT_DIR}" )
        MESSAGE( "   Matlab_LIBRARIES = ${Matlab_LIBRARIES}" )
        MESSAGE( "   Matlab_INCLUDE_DIRS = ${Matlab_INCLUDE_DIRS}" )
    ENDIF()
ENDMACRO()


# Macro to add user c++ std 
MACRO( SET_CXX_STD )
    # Set the C++ standard
    SET( CMAKE_CXX_EXTENSIONS OFF )
    IF ( NOT CMAKE_CXX_STANDARD )
        IF ( CXX_STD )
            IF ( ${CXX_STD} STREQUAL "NONE" )
                # Do nothing
            ELSEIF ( "${CXX_STD}" MATCHES "^(98|11|14|17|20|23|26)$" )
                SET( CMAKE_CXX_STANDARD ${CXX_STD} )
                SET( CXX_STD_FLAG ${CMAKE_CXX${CXX_STD}_STANDARD_COMPILE_OPTION} )
            ELSE()
                MESSAGE( FATAL_ERROR "Unknown C++ standard ${CXX_STD} (98,11,14,17,20,23,26,NONE)" )
            ENDIF()
        ELSE()
            MESSAGE( FATAL_ERROR "C++ standard is not set" )
        ENDIF()
    ENDIF()
    # Set the C standard
    IF ( NOT CMAKE_C_STANDARD )
        IF ( C_STD )
            IF ( ${C_STD} STREQUAL "NONE" )
                # Do nothing
            ELSEIF ( "${CXX_STD}" MATCHES "^(90|99|11|17|23)$" )
                SET( CMAKE_C_STANDARD ${C_STD} )
            ELSE()
                MESSAGE( FATAL_ERROR "Unknown C standard ${C_STD} (90,99,11,17,23,NONE)" )
            ENDIF()
        ELSEIF ( CMAKE_CXX_STANDARD )
            IF ( "${CMAKE_CXX_STANDARD}" STREQUAL "98" )
                SET( CMAKE_C_STANDARD 99 )
            ELSEIF ( "${CMAKE_CXX_STANDARD}" MATCHES "^(11|14)$" )
                SET( CMAKE_C_STANDARD 11 )
            ELSEIF ( "${CMAKE_CXX_STANDARD}" MATCHES "^(17|20)$" )
                SET( CMAKE_C_STANDARD 17 )
            ELSEIF ( "${CMAKE_CXX_STANDARD}" MATCHES "^(23|26)$" )
                SET( CMAKE_C_STANDARD 26 )
            ELSE()
                MESSAGE( FATAL_ERROR "Unknown C++ standard" )
            ENDIF()
        ELSE()
            MESSAGE( FATAL_ERROR "C standard is not set" )
        ENDIF()
    ENDIF()
    ADD_DEFINITIONS( -DCXX_STD=${CXX_STD} )
ENDMACRO()


# Macro to configure TimerUtility-specific options
MACRO( CONFIGURE_TIMER )
    SET( CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE )
    SET( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
    SET(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} "${TIMER_INSTALL_DIR}/lib" )
    # Set the maximum number of processors for the tests
    IF ( NOT TEST_MAX_PROCS )
        SET( TEST_MAX_PROCS 32 )
    ENDIF()
    # Check if we want to enable the traps for new/delete
    CHECK_ENABLE_FLAG( OVERLOAD_NEW 1 )
    IF ( NOT OVERLOAD_NEW OR DISABLE_NEW_OVERLOAD )
        FILE(WRITE "${TIMER_INSTALL_DIR}/include/ProfilerDefinitions.h" "#define TIMER_DISABLE_NEW_OVERLOAD\n" )
    ELSE()
        FILE(WRITE "${TIMER_INSTALL_DIR}/include/ProfilerDefinitions.h" "#define TIMER_ENABLE_NEW_OVERLOAD\n" )
    ENDIF()
    # Add flags for MATLAB
    IF ( USE_MATLAB )
        SET( MEX_FLAGS ${MEX_FLAGS} -DUSE_TIMER )
        SET( MEX_INCLUDE ${MEX_INCLUDE} -I${TIMER_INSTALL_DIR}/include )
        SET( MEX_LIBS  -ltimerutility ${MEX_LIBS} )
        SET( MEX_LDFLAGS ${MEX_LDFLAGS} )
    ENDIF()
    # Disable CMake warning for unused flags
    NULL_USE(QWT_SRC_DIR)
    NULL_USE(QWT_URL)
ENDMACRO()



# Macro to find QT
MACRO( CONFIGURE_QT )
    IF ( NOT DEFINED QT_VERSION )
        SET( QT_VERSION "4" )
    ENDIF()
    IF ( ${QT_VERSION} EQUAL "4" )
        # Using Qt4
        SET( QT "QT4" )
        SET( Qt "Qt4" )
        SET( QT_COMPONENTS  QtCore QtGui QtOpenGL QtSvg QtSql )
    ELSEIF ( ${QT_VERSION} EQUAL "5" )
        # Using Qt5
        SET( QT "QT5" )
        SET( Qt "Qt5" )
        SET( QT_COMPONENTS  Core Gui OpenGL Svg Sql )
    ELSE()
        MESSAGE( FATAL_ERROR "Unknown Qt version")
    ENDIF()
    FIND_PACKAGE( ${Qt} COMPONENTS ${QT_COMPONENTS} REQUIRED )
    SET( QT_FOUND ${Qt}_FOUND )
    IF ( NOT ${Qt}_FOUND )
        RETURN()
    ENDIF()
    IF ( ${QT_VERSION} EQUAL "5" )
        GET_TARGET_PROPERTY( QT_QMAKE_EXECUTABLE ${Qt}::qmake IMPORTED_LOCATION )
    ENDIF()
ENDMACRO()


# Macro to configure QWT
MACRO( CONFIGURE_QWT )
    IF ( QWT_INSTALL_DIR )
        RETURN()
    ENDIF()
    INCLUDE(ProcessorCount)
    INCLUDE(ExternalProject)
    ProcessorCount( PROCS_INSTALL )
    SET( QWT_BUILD_DIR "${CMAKE_BINARY_DIR}/QWT-prefix/src/QWT-build" )
    SET( QWT_SRC_DIR "${CMAKE_BINARY_DIR}/QWT-prefix/src/QWT-src" )
    SET( QWT_INSTALL_DIR "${TIMER_INSTALL_DIR}/qwt" )
    ExternalProject_Add(
        QWT
        URL                     "${QWT_URL}"
        SOURCE_DIR          "${QWT_SRC_DIR}"
        PATCH_COMMAND       patch -p1 -i ${CMAKE_CURRENT_LIST_DIR}/cmake/QWT.patch
        CONFIGURE_COMMAND   ${QT_QMAKE_EXECUTABLE} QWT_INSTALL_PREFIX=${QWT_INSTALL_DIR} qwt.pro
        BUILD_COMMAND       make -j ${PROCS_INSTALL}
        INSTALL_COMMAND     make install
        BUILD_IN_SOURCE     1
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        DEPENDS             
        LOG_DOWNLOAD 1   LOG_UPDATE 1   LOG_CONFIGURE 1   LOG_BUILD 1   LOG_TEST 1   LOG_INSTALL 1
    )
ENDMACRO()



# Macro to configure GUI
MACRO( CONFIGURE_GUI )
    SET( TIMER_GUI_SOURCE_DIR "${${PROJ}_SOURCE_DIR}/gui" )
    SET( TIMER_GUI_INSTALL_DIR "${GUI_INSTALL_DIR}" )
    SET( TIMER_GUI_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/gui" )
    CONFIGURE_FILE( "${${PROJ}_SOURCE_DIR}/cmake/configure_GUI.cmake" "${TIMER_GUI_BUILD_DIR}/configure_GUI.cmake" @ONLY )
    INSTALL( SCRIPT "${TIMER_GUI_BUILD_DIR}/configure_GUI.cmake" )
ENDMACRO()



