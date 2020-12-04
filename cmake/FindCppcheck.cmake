# FindCppcheck
# ---------
#
# Find cppcheck
#
# Use this module by invoking find_package with the form:
#
#   find_package( Cppcheck
#     [REQUIRED]             # Fail with error if the cppcheck is not found
#   )
#
# This module finds cppcheck and configures a test using the provided options
#
# This program reconizes the following options
#   CPPCHECK_INCLUDE      - List of include folders
#   CPPCHECK_OPTIONS      - List of cppcheck options
#   CPPCHECK_SOURCE       - Source path to check
#
# The following variables are set by find_package( Cppcheck )
#
#   CPPCHECK_FOUND        - True if cppcheck was found


# Find cppcheck if availible
FIND_PROGRAM( CPPCHECK 
    NAMES cppcheck cppcheck.exe 
    PATHS "${CPPCHECK_DIRECTORY}" "C:/Program Files/Cppcheck" "C:/Program Files (x86)/Cppcheck" 
)
IF ( CPPCHECK )
    SET( CPPCHECK_FOUND TRUE )
ELSE()
    SET( CPPCHECK_FOUND FALSE )
ENDIF()
IF ( CPPCHECK_FOUND )
    EXECUTE_PROCESS( COMMAND ${CPPCHECK} --version OUTPUT_VARIABLE CPPCHECK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE )
    MESSAGE( STATUS "Using cppcheck: ${CPPCHECK_VERSION}")
ELSEIF ( CPPCHECK_FIND_REQUIRED )
    MESSAGE( FATAL_ERROR "cppcheck not found")
ELSE()
    MESSAGE( STATUS "cppcheck not found")
ENDIF()


# Set the options for cppcheck
IF ( NOT DEFINED CPPCHECK_OPTIONS )
    SET( CPPCHECK_OPTIONS -q --enable=all )
ENDIF()
IF( NOT DEFINED CPPCHECK_INCLUDE )
    SET( CPPCHECK_INCLUDE )
    GET_PROPERTY( dirs DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY INCLUDE_DIRECTORIES )
    LIST( REMOVE_DUPLICATES dirs )
    FOREACH(dir ${dirs})
        SET( CPPCHECK_INCLUDE ${CPPCHECK_INCLUDE} "-I${dir}" )
    ENDFOREACH()
ENDIF()


# Add the test
IF ( CPPCHECK )
    ADD_TEST( cppcheck ${CPPCHECK} ${CPPCHECK_OPTIONS} --error-exitcode=1  ${CPPCHECK_INCLUDE} "${CMAKE_CURRENT_SOURCE_DIR}" )
    IF( ${CPPCHECK_DIR} )
        SET_TESTS_PROPERTIES( ${TEST_NAME} PROPERTIES WORKING_DIRECTORY "${CPPCHECK_DIR}" PROCESSORS 1 )
    ENDIF()
ENDIF()

