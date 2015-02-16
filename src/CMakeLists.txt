CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
CMAKE_POLICY(SET CMP0014 OLD)
IF ( ${CMAKE_MAJOR_VERSION} EQUAL 3 )
    CMAKE_POLICY(SET CMP0026 OLD)
ENDIF()


MESSAGE( "===========================" )
MESSAGE( "Configuring Timer Utility"    )
MESSAGE( "===========================" )


# Prevent users from building in place
IF ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF ()


# Set the project name
PROJECT( TIMER )


# Set some common paths
SET( TIMER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
SET( TIMER_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR} )
IF( PREFIX )
    SET( TIMER_INSTALL_DIR ${PREFIX} )
ELSEIF( NOT TIMER_INSTALL_DIR )
    SET( TIMER_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR} )
ENDIF()
SET( CMAKE_MODULE_PATH ${TIMER_SOURCE_DIR} ${TIMER_SOURCE_DIR}/cmake )


# Check if we are only compiling docs
SET( PROJ TIMER )
INCLUDE( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/macros.cmake )
INCLUDE( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libraries.cmake )
CHECK_ENABLE_FLAG( ONLY_BUILD_DOCS 0 )


# Set testing paramaters
SET( DROP_METHOD "http" )
SET( DROP_SITE "oblivion.engr.colostate.edu" )
SET( DROP_LOCATION "/CDash/submit.php?project=AMR-MHD" )
SET( TRIGGER_SITE "" )
SET( DROP_SITE_CDASH TRUE )
ENABLE_TESTING()
INCLUDE( CTest )


# Create custom targets for build-test, check, and distclean
ADD_CUSTOM_TARGET( doc )
ADD_CUSTOM_TARGET( latex_docs )
ADD_CUSTOM_TARGET( build-test )
ADD_CUSTOM_TARGET( check COMMAND  make test  )
ADD_DISTCLEAN( libtimerutility.* utilities gui )


# Check the compile mode and compile flags
IF ( NOT ONLY_BUILD_DOCS )
    CONFIGURE_SYSTEM()
ENDIF()


# Add some directories to include
INCLUDE_DIRECTORIES( ${TIMER_INSTALL_DIR}/include )


# Set doxygen info
CHECK_ENABLE_FLAG( USE_DOXYGEN 1 )
CHECK_ENABLE_FLAG( USE_LATEX 1 )
FILE( MAKE_DIRECTORY "${TIMER_INSTALL_DIR}/doc" )
IF ( USE_DOXYGEN )
    SET( DOXYFILE_IN ${TIMER_SOURCE_DIR}/doxygen/Doxyfile.in)
    SET( DOXY_HEADER_FILE ${TIMER_SOURCE_DIR}/doxygen/html/header.html )
    SET( DOXY_FOOTER_FILE ${TIMER_SOURCE_DIR}/doxygen/html/footer.html )
    SET( DOXYFILE_OUTPUT_DIR ${TIMER_BUILD_DIR}/doc/ )
    SET( DOXYFILE_SRC_HTML_DIR ${TIMER_SOURCE_DIR}/doxygen/html )
    SET( DOXYFILE_SOURCE_DIR ${TIMER_SOURCE_DIR} )
    SET( REL_PACKAGE_HTML "" )
    SET( DOXYGEN_MACROS "" )
    MESSAGE("DOXYGEN_MACROS = ${DOXYGEN_MACROS}")
    INCLUDE( ${TIMER_SOURCE_DIR}/cmake/UseDoxygen.cmake )
    SET(DOXYFILE_LATEX "YES")
    IF ( DOXYGEN_FOUND )
        ADD_DEPENDENCIES( doxygen latex_docs )
        ADD_DEPENDENCIES( doc latex_docs doxygen )
    ELSE()
        SET( USE_DOXYGEN 0 )
    ENDIF()
ENDIF()


# Get the mercurial revision
EXECUTE_PROCESS(COMMAND hg id -i "${TIMER_SOURCE_DIR}/.." OUTPUT_VARIABLE TIMER_VERSION)
STRING(REGEX REPLACE "(\r?\n)+$" "" TIMER_VERSION "${TIMER_VERSION}")
FILE(WRITE  ${TIMER_INSTALL_DIR}/include/TimerUtilityVersion.h "#define TIMER_VERSION \"${TIMER_VERSION}\"\n" )
MESSAGE("ProfilerApp Version = ${TIMER_VERSION}")


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


# Configure internal libraries (order matters)
SET( TIMER_LIBS  timerutility )


# Macro to create 1,2,4 processor tests
MACRO( ADD_TIMER_TEST_1_2_4 EXENAME ${ARGN} )
    ADD_TIMER_TEST( ${EXENAME} ${ARGN} )
    ADD_TIMER_TEST_PARALLEL( ${EXENAME} 2 ${ARGN} )
    ADD_TIMER_TEST_PARALLEL( ${EXENAME} 4 ${ARGN} )
ENDMACRO()


# Add the src directories
IF ( NOT ONLY_BUILD_DOCS )
    BEGIN_PACKAGE_CONFIG( timerutility )
    INSTALL_TIMER_TARGET( timerutility )
    ADD_SUBDIRECTORY( test )
    ADD_SUBDIRECTORY( matlab )
ENDIF()
IF ( USE_LATEX )
    ADD_SUBDIRECTORY(latex_docs)
ENDIF()


# Write a cmake file with the configure info
#IF ( NOT ONLY_BUILD_DOCS )
#    SAVE_CMAKE_FLAGS()
#ENDIF()


