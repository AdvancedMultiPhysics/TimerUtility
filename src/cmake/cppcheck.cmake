# Find cppcheck if availible
FIND_PROGRAM( CPPCHECK 
    NAMES cppcheck cppcheck.exe 
    PATHS "${CPPCHECK_DIRECTORY}" "C:/Program Files/Cppcheck" "C:/Program Files (x86)/Cppcheck" 
)
IF ( CPPCHECK )
    MESSAGE("Using cppcheck")
ELSE()
    MESSAGE("cppcheck not found")
ENDIF()

# Set the options for cppcheck
IF ( NOT DEFINED CPPCHECK_OPTIONS )
    SET( CPPCHECK_OPTIONS -q --enable=all )
ENDIF()
IF( NOT DEFINED CPPCHECK_INCLUDE )
    SET( CPPCHECK_INCLUDE )
    GET_PROPERTY( dirs DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY INCLUDE_DIRECTORIES )
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

