# Set some CMake properties    
CMAKE_MINIMUM_REQUIRED(VERSION 3.9)
IF ( ${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.20.0" )
    CMAKE_POLICY( SET CMP0115 OLD )
ENDIF()
SET( DISABLE_LTO TRUE )


MESSAGE( "===========================" )
MESSAGE( "Configuring Timer Utility"   )
MESSAGE( "===========================" )



# Set the project name
SET( PROJ Timer )              # Set the project name for CMake
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
    SET( CMAKE_Fortran_MODULE_DIRECTORY ${TIMER_INSTALL_DIR}/include CACHE PATH "Fortran module install folder" )
ENDIF()


# Set the default C++ standard
IF ( NOT CXX_STD )
    MESSAGE( WARNING "CXX_STD not set, defaulting to c++17" )
    SET( CXX_STD 17 )
ENDIF()
IF ( "${CXX_STD}" STREQUAL "98" OR "${CXX_STD}" STRLESS "17" )
    MESSAGE( FATAL_ERROR "c++17 or higher is required" )
ENDIF()


# Include CMake files
INCLUDE( cmake/macros.cmake )
INCLUDE( cmake/libraries.cmake )


# Check if we are only compiling docs
CHECK_ENABLE_FLAG( ONLY_BUILD_DOCS 0 )


# Set testing paramaters
SET( DROP_METHOD "http" )
SET( DROP_SITE "" )
SET( DROP_LOCATION "/CDash/submit.php?project=AMP" )
SET( TRIGGER_SITE "" )
SET( DROP_SITE_CDASH TRUE )
SET( CMAKE_DIRECTORY_LABELS "Timer" ) # Set a label for subprojects
SET( CTEST_LABELS_FOR_SUBPROJECTS Timer )
ENABLE_TESTING()
INCLUDE( CTest )


# Create the target for documentation
ADD_CUSTOM_TARGET( doc )
ADD_CUSTOM_TARGET( latex_docs )
CHECK_ENABLE_FLAG( USE_DOXYGEN 1 )
CHECK_ENABLE_FLAG( USE_LATEX 1 )
FILE( MAKE_DIRECTORY "${${PROJ}_INSTALL_DIR}/doc" )
IF ( USE_DOXYGEN )
    SET( DOXYFILE_LATEX "YES" )
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


# Create custom targets for distclean
SET( EXCLUDE_TESTS_FROM_ALL 0 )
ADD_DISTCLEAN( libtimerutility.* timerutility.lib utilities gui QWT qwt Debug TIMER.build TIMER.xcodeproj features gmon.out GUI-prefix plugins QWT-prefix )


# Get the version information
INCLUDE( cmake/WriteRepoVersion.cmake )
WRITE_REPO_VERSION( "${${PROJ}_INSTALL_DIR}/include/TimerUtilityVersion.h" )


# Configure external packages
IF ( NOT ONLY_BUILD_DOCS )
    CONFIGURE_SYSTEM()
    CONFIGURE_MPI()     # MPI must be before other libraries
    CONFIGURE_MATLAB()  # MATLAB must be second after MPI
    CONFIGURE_TIMER()
    CONFIGURE_LINE_COVERAGE()
    # Currently, windows doesn't link shared libraries
    IF( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        SET( LIB_TYPE STATIC )
    ENDIF()
ENDIF()


# Set a macro to add serial, 2 processor, and 4 processor tests
MACRO( ADD_LaserCrystal_TEST_1_2_4 EXENAME ${ARGN} )
    ADD_TIMER_TEST( ${EXENAME} ${ARGN} )
    ADD_TIMER_TEST( ${EXENAME} ${ARGN} PROCS 2 )
    ADD_TIMER_TEST( ${EXENAME} ${ARGN} PROCS 4 )
ENDMACRO()


# Check for mallinfo2
INCLUDE(CheckSymbolExists)
CHECK_SYMBOL_EXISTS( mallinfo2 malloc.h HAVE_MALLINFO2 )
IF ( HAVE_MALLINFO2 )
    ADD_DEFINITIONS( -DHAVE_MALLINFO2 )
ENDIF()


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
ENDIF()
IF ( USE_LATEX )
    ADD_SUBDIRECTORY(latex_docs)
ENDIF()


# Generate a Package Configuration File
INCLUDE( CMakePackageConfigHelpers )
SET( INCLUDE_INSTALL_DIR  "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Location of header files" )
SET( SYSCONFIG_INSTALL_DIR ${${PROJ}_INSTALL_DIR}/lib/cmake/Timer CACHE PATH "Location of configuration files" )
CONFIGURE_PACKAGE_CONFIG_FILE( cmake/Timer.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/TimerConfig.cmake
    INSTALL_DESTINATION "${${PROJ}_INSTALL_DIR}/lib/cmake/Timer"
    PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR )
WRITE_BASIC_PACKAGE_VERSION_FILE(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/TimerVersion.cmake
    VERSION ${${PROJ}_VERSION}
    COMPATIBILITY SameMajorVersion )
INSTALL( FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/TimerConfig.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/cmake/TimerVersion.cmake
         DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/Timer )


# Generate a Dummy Configuration File
INCLUDE( CMakePackageConfigHelpers )
SET( INCLUDE_INSTALL_DIR  "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Location of header files" )
SET( SYSCONFIG_INSTALL_DIR ${${PROJ}_INSTALL_DIR}/lib/cmake/dummy CACHE PATH "Location of configuration files" )
CONFIGURE_PACKAGE_CONFIG_FILE( cmake/DummyTimer.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/dummy/TimerConfig.cmake
    INSTALL_DESTINATION "${${PROJ}_INSTALL_DIR}/lib/cmake/dummy"
    PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR )
WRITE_BASIC_PACKAGE_VERSION_FILE(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/dummy/TimerVersion.cmake
    VERSION ${${PROJ}_VERSION}
    COMPATIBILITY SameMajorVersion )
INSTALL( FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/dummy/TimerConfig.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/cmake/dummy/TimerVersion.cmake
         DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/dummy )


# Add the GUI
IF ( NOT DEFINED ENABLE_GUI )
    IF ( NOT ( "${QWT_INSTALL_DIR}" STREQUAL "" ) OR NOT ( "${QWT_URL}" STREQUAL "" ) )
        SET( ENABLE_GUI 1 )
    ELSE()
        SET( ENABLE_GUI 0 )
    ENDIF()
ENDIF()
IF ( ENABLE_GUI )
    # Check for QT
    CONFIGURE_QT()
    IF( NOT QT_FOUND )
        MESSAGE("${Qt} not found, gui will be disabled")
    ELSE()
        # Configure QWT
        CONFIGURE_QWT()
        # Configure GUI
        CONFIGURE_GUI()
        # Add the tests
        ADD_TEST( NAME load_timer COMMAND ${TIMER_INSTALL_DIR}/bin/load_timer set1.1.timer set2.1.timer WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test )
    ENDIF()
ENDIF()


# Add the cppcheck tests
SET( CPPCHECK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
SET( CPPCHECK_USE_JSON TRUE )
FIND_PACKAGE( Cppcheck )


# Add the cppclean tests
SET( CPPCLEAN_OPTIONS )
SET( CPPCLEAN_EXCLUDE )
SET( CPPCLEAN_SUPPRESSIONS )
SET( CPPCLEAN_FAIL_ON_WARNINGS 1 )
SET( CPPCLEAN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
FIND_PACKAGE( Cppclean )
