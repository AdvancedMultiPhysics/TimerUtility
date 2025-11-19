# Set some CMake properties
CMAKE_MINIMUM_REQUIRED( VERSION 3.20.0 )
SET( DISABLE_LTO TRUE )

MESSAGE( "===========================" )
MESSAGE( "Configuring Timer Utility" )
MESSAGE( "===========================" )

# Set the project name
SET( PROJ Timer ) # Set the project name for CMake
SET( Timer_LIB timerutility ) # Set the final library name
SET( Timer_INC ) # Set an optional subfolder for includes ( e.g. include/name/... )

# Initialize the project ( should disable languages that the TPL builder will enable )
PROJECT( ${PROJ} LANGUAGES )

# Prevent users from building in place
IF ( "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()

# Set source/install paths
SET( ${PROJ}_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )
SET( ${PROJ}_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
IF ( ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${${PROJ}_INSTALL_DIR}" )
ELSEIF ( NOT ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()
INCLUDE_DIRECTORIES( "${${PROJ}_INSTALL_DIR}/include" )
SET( CMAKE_MODULE_PATH ${${PROJ}_SOURCE_DIR} ${${PROJ}_SOURCE_DIR}/cmake )

# Enable languages
ENABLE_LANGUAGE( C )
ENABLE_LANGUAGE( CXX )
IF ( CMAKE_Fortran_COMPILER )
    ENABLE_LANGUAGE( Fortran )
    SET( CMAKE_Fortran_MODULE_DIRECTORY ${${PROJ}_INSTALL_DIR}/include CACHE PATH "Fortran module install folder" )
ENDIF()

# Set the default C++ standard
IF ( NOT CXX_STD )
    MESSAGE( "CXX_STD not set, defaulting to c++17" )
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
    SET( DOXY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doxygen" )
    SET( DOXYFILE_IN "${${PROJ}_SOURCE_DIR}/doxygen/Doxyfile.in" )
    SET( DOXY_HEADER_FILE "${${PROJ}_SOURCE_DIR}/doxygen/html/header.html" )
    SET( DOXY_FOOTER_FILE "${${PROJ}_SOURCE_DIR}/doxygen/html/footer.html" )
    SET( DOXYFILE_OUTPUT_DIR "${${PROJ}_INSTALL_DIR}/doc" )
    SET( DOXYFILE_SRC_HTML_DIR "${${PROJ}_SOURCE_DIR}/doxygen/html" )
    SET( DOXYFILE_SOURCE_DIR "${${PROJ}_SOURCE_DIR}" )
    SET( REL_PACKAGE_HTML "" )
    SET( GENERATE_LATEX NO )
    SET( DOXYGEN_GENERATE_LATEX NO )
    CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxyfile.in" "${CMAKE_CURRENT_BINARY_DIR}/doxygen.config" @ONLY )
    FIND_PACKAGE( Doxygen COMPONENTS dot )
    FIND_PACKAGE( LATEX )
    IF ( Doxygen_FOUND )
        DOXYGEN_ADD_DOCS( doxygen "${CMAKE_CURRENT_SOURCE_DIR}" ALL CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/doxygen.config" )
        ADD_DEPENDENCIES( doxygen latex_docs )
        ADD_DEPENDENCIES( doc latex_docs doxygen )
        SET_PROPERTY( TARGET doc PROPERTY EXCLUDE_FROM_ALL TRUE )
        SET_PROPERTY( TARGET doxygen PROPERTY EXCLUDE_FROM_ALL TRUE )
        SET_PROPERTY( TARGET latex_docs PROPERTY EXCLUDE_FROM_ALL TRUE )
    ELSE()
        SET( USE_DOXYGEN 0 )
    ENDIF()
ENDIF()

# Create custom targets for distclean
SET( EXCLUDE_TESTS_FROM_ALL 0 )
ADD_DISTCLEAN(
    libtimerutility.*
    timerutility.lib
    utilities
    gui
    QWT
    qwt
    Debug
    ${PROJ}.build
    ${PROJ}.xcodeproj
    features
    gmon.out
    GUI-prefix
    plugins
    QWT-prefix )

# Get the version information
INCLUDE( cmake/WriteRepoVersion.cmake )
WRITE_REPO_VERSION( "${${PROJ}_INSTALL_DIR}/include/TimerUtilityVersion.h" )

# Configure external packages
IF ( NOT ONLY_BUILD_DOCS )
    CONFIGURE_SYSTEM()
    CONFIGURE_MPI() # MPI must be before other libraries
    SET( EXTERNAL_LIBS ${MPI_CXX_LIBRARIES} )
    SET( MPI_LINK_FLAGS ${MPI_CXX_LINK_FLAGS} )
    CONFIGURE_MATLAB() # MATLAB must be second after MPI
    CONFIGURE_TIMER()
    CONFIGURE_LINE_COVERAGE()
    CONFIGURE_VALGRIND()
    # Currently, windows doesn't link shared libraries
    IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        SET( LIB_TYPE STATIC )
    ENDIF()
ENDIF()

# Check for mallinfo2
INCLUDE( CheckSymbolExists )
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
    IF ( USE_MATLAB )
        ADD_SUBDIRECTORY( matlab )
    ENDIF()
ENDIF()
IF ( USE_LATEX )
    ADD_SUBDIRECTORY( latex_docs )
ENDIF()

# Generate a Package Configuration File
INCLUDE( CMakePackageConfigHelpers )
SET( INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Location of header files" )
SET( SYSCONFIG_INSTALL_DIR ${${PROJ}_INSTALL_DIR}/lib/cmake/Timer CACHE PATH "Location of configuration files" )
CONFIGURE_PACKAGE_CONFIG_FILE( cmake/Timer.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/tmp/TimerConfig.cmake INSTALL_DESTINATION "${${PROJ}_INSTALL_DIR}/lib/cmake/Timer"
                              PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR )
WRITE_BASIC_PACKAGE_VERSION_FILE( ${CMAKE_CURRENT_BINARY_DIR}/tmp/TimerVersion.cmake VERSION ${${PROJ}_VERSION} COMPATIBILITY SameMajorVersion )
INSTALL( FILES ${CMAKE_CURRENT_BINARY_DIR}/tmp/TimerConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/tmp/TimerVersion.cmake DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/Timer )

# Generate a Dummy Configuration File
INCLUDE( CMakePackageConfigHelpers )
SET( INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Location of header files" )
SET( SYSCONFIG_INSTALL_DIR ${${PROJ}_INSTALL_DIR}/lib/cmake/dummy CACHE PATH "Location of configuration files" )
CONFIGURE_PACKAGE_CONFIG_FILE( cmake/DummyTimer.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/tmp/dummy/TimerConfig.cmake INSTALL_DESTINATION "${${PROJ}_INSTALL_DIR}/lib/cmake/dummy"
                              PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR )
WRITE_BASIC_PACKAGE_VERSION_FILE( ${CMAKE_CURRENT_BINARY_DIR}/tmp/dummy/TimerVersion.cmake VERSION ${${PROJ}_VERSION} COMPATIBILITY SameMajorVersion )
INSTALL( FILES ${CMAKE_CURRENT_BINARY_DIR}/tmp/dummy/TimerConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/tmp/dummy/TimerVersion.cmake DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/dummy )

# Add the GUI
IF ( NOT DEFINED ENABLE_GUI )
    IF ( NOT ( "${QWT_INSTALL_DIR}" STREQUAL "" ) OR NOT ( "${QWT_URL}" STREQUAL "" ))
        SET( ENABLE_GUI 1 )
    ELSE()
        SET( ENABLE_GUI 0 )
    ENDIF()
ENDIF()
IF ( ENABLE_GUI )
    # Check for QT
    CONFIGURE_QT()
    IF ( NOT QT_FOUND )
        MESSAGE( "${Qt} not found, gui will be disabled" )
    ELSE()
        # Configure QWT
        CONFIGURE_QWT()
        # Configure GUI
        CONFIGURE_GUI()
        # Add the tests
        ADD_TEST( NAME load_timer COMMAND ${Timer_INSTALL_DIR}/bin/load_timer set1.1.timer set2.1.timer WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test )
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
