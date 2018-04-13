# Set some CMake properties    
CMAKE_MINIMUM_REQUIRED(VERSION 3.6)
SET( DISABLE_LTO TRUE )


MESSAGE( "===========================" )
MESSAGE( "Configuring Timer Utility"   )
MESSAGE( "===========================" )



# Set the project name
SET( PROJ TIMER )              # Set the project name for CMake
SET( TIMER_LIB timerutility )  # Set the final library name
SET( TIMER_INC )               # Set an optional subfolder for includes (e.g. include/name/...)


# Initialize the project (should disable langagues that the TPL builder will enable)
PROJECT( ${PROJ} LANGUAGES )


# Prevent users from building in place
IF ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()


# Set source/install paths
SET( ${PROJ}_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )
SET( ${PROJ}_BUILD_DIR  "${CMAKE_CURRENT_BINARY_DIR}" )
IF( ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${${PROJ}_INSTALL_DIR}" )
ELSEIF( NOT ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()
INCLUDE_DIRECTORIES( "${${PROJ}_INSTALL_DIR}/include" )
SET( CMAKE_MODULE_PATH ${TIMER_SOURCE_DIR} ${TIMER_SOURCE_DIR}/cmake )


# Enable languages
ENABLE_LANGUAGE( C )
ENABLE_LANGUAGE( CXX )
IF ( CMAKE_Fortran_COMPILER )
    ENABLE_LANGUAGE( Fortran )
    SET (CMAKE_Fortran_MODULE_DIRECTORY ${TIMER_INSTALL_DIR}/include CACHE PATH "Fortran module install folder" )
ENDIF()


# Set the default C++ standard
IF ( NOT CXX_STD )
    MESSAGE( WARNING "CXX_STD not set, defaulting to c++11" )
    SET( CXX_STD 11 )
ENDIF()
IF ( "${CXX_STD}" STREQUAL "98" )
    MESSAGE( FATAL_ERROR "c++11 or higher is required" )
ENDIF()


# Include CMake files
INCLUDE( cmake/macros.cmake )
INCLUDE( cmake/libraries.cmake )
INCLUDE( cmake/WriteCompilerFeatures.cmake TIMER )


# Check if we are only compiling docs
CHECK_ENABLE_FLAG( ONLY_BUILD_DOCS 0 )


# Set testing paramaters
SET( DROP_METHOD "http" )
SET( DROP_SITE "" )
SET( DROP_LOCATION "/CDash/submit.php?project=AMR-MHD" )
SET( TRIGGER_SITE "" )
SET( DROP_SITE_CDASH TRUE )
ENABLE_TESTING()
INCLUDE( CTest )


# Check the compile mode and compile flags
IF ( NOT ONLY_BUILD_DOCS )
    CONFIGURE_SYSTEM()
    WRITE_COMPILE_FEATURES( "${TIMER_INSTALL_DIR}/include/ProfilerDefinitions.h" TIMER )
ENDIF()


# Create the target for documentation
ADD_CUSTOM_TARGET( doc )
ADD_CUSTOM_TARGET( latex_docs )
CHECK_ENABLE_FLAG( USE_DOXYGEN 1 )
CHECK_ENABLE_FLAG( USE_LATEX 1 )
FILE( MAKE_DIRECTORY "${${PROJ}_INSTALL_DIR}/doc" )
IF ( USE_DOXYGEN )
    SET(DOXYFILE_LATEX "YES")
    SET( DOXYFILE_IN "${${PROJ}_SOURCE_DIR}/doxygen/Doxyfile.in" )
    SET( DOXY_HEADER_FILE "${${PROJ}_SOURCE_DIR}/doxygen/html/header.html" )
    SET( DOXY_FOOTER_FILE "${${PROJ}_SOURCE_DIR}/doxygen/html/footer.html" )
    SET( DOXYFILE_OUTPUT_DIR "${${PROJ}_INSTALL_DIR}/doc" )
    SET( DOXYFILE_SRC_HTML_DIR "${${PROJ}_SOURCE_DIR}/doxygen/html" )
    SET( DOXYFILE_SOURCE_DIR "${${PROJ}_SOURCE_DIR}" )
    SET( REL_PACKAGE_HTML "" )
    SET( DOXYGEN_MACROS "" )
    MESSAGE("DOXYGEN_MACROS = ${DOXYGEN_MACROS}")
    INCLUDE( "${${PROJ}_SOURCE_DIR}/cmake/UseDoxygen.cmake" )
    IF ( DOXYGEN_FOUND )
        ADD_DEPENDENCIES( doxygen latex_docs )
        ADD_DEPENDENCIES( doc latex_docs doxygen )
    ELSE()
        SET( USE_DOXYGEN 0 )
    ENDIF()
ENDIF()


# Create custom targets for build-test, check, and distclean
SET( EXCLUDE_TESTS_FROM_ALL 0 )
ADD_CUSTOM_TARGET( build-test )
ADD_CUSTOM_TARGET( build-examples )
ADD_CUSTOM_TARGET( check COMMAND  make test  )
ADD_DISTCLEAN( libtimerutility.* timerutility.lib utilities gui QWT Debug TIMER.build TIMER.xcodeproj )


# Get the version information
WRITE_REPO_VERSION( "${${PROJ}_INSTALL_DIR}/include/TimerUtilityVersion.h" )


# Configure external packages
IF ( NOT ONLY_BUILD_DOCS )
    CONFIGURE_MPI()     # MPI must be before other libraries
    CONFIGURE_MATLAB()  # MATLAB must be second after MPI
    CONFIGURE_TIMER()
    CONFIGURE_LINE_COVERAGE()
    # Currently, windows doesn't link shared libraries
    IF( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        SET( LIB_TYPE STATIC )
    ENDIF()
ENDIF()


# Macro to create 1,2,4 processor tests
MACRO( ADD_TIMER_TEST_1_2_4 EXENAME ${ARGN} )
    ADD_TIMER_TEST( ${EXENAME} ${ARGN} )
    ADD_TIMER_TEST_PARALLEL( ${EXENAME} 2 ${ARGN} )
    ADD_TIMER_TEST_PARALLEL( ${EXENAME} 4 ${ARGN} )
ENDMACRO()


# Add the src directories
IF ( NOT ONLY_BUILD_DOCS )
    BEGIN_PACKAGE_CONFIG( timerutility_library )
    IF ( CMAKE_Fortran_COMPILER )
        ADD_PACKAGE_SUBDIRECTORY( fortran )
    ENDIF()
    INSTALL_TIMER_TARGET( timerutility_library )
    INSTALL_PROJ_LIB()
    ADD_SUBDIRECTORY( test )
    ADD_SUBDIRECTORY( matlab )
    ADD_SUBDIRECTORY( gui )
ENDIF()
IF ( USE_LATEX )
    ADD_SUBDIRECTORY(latex_docs)
ENDIF()


# Add the cppcheck tests
SET( CPPCHECK_INCLUDE "" )
SET( CPPCHECK_OPTIONS -q --enable=all --force --suppress=missingIncludeSystem 
    "--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/cppcheckSuppressionFile" )
IF ( CXX_STD STREQUAL 98 )
    SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c99 --std=c++03 --std=posix )
ELSEIF ( CXX_STD STREQUAL 11 )
    SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c11 --std=c++11 --std=posix )
ELSEIF ( CXX_STD STREQUAL 14 )
    SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c11 --std=c++11 --std=posix )
ENDIF()
SET( CPPCHECK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
FIND_PACKAGE( Cppcheck )


# Add the cppclean tests
SET( CPPCLEAN_OPTIONS )
SET( CPPCLEAN_EXCLUDE AtomicModel.h interp.h )
SET( CPPCLEAN_SUPPRESSIONS )
SET( CPPCLEAN_FAIL_ON_WARNINGS 1 )
SET( CPPCLEAN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
FIND_PACKAGE( Cppclean )
