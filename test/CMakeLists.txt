# Add tests
ADD_TIMER_TEST( test_MemoryApp )
ADD_TIMER_TEST_1_2_4( test_ProfilerApp )
ADD_TIMER_TEST( test_ProfilerApp_C )
ADD_TIMER_TEST( test_Load set1.1.timer )
IF ( (${CXX_STD} STREQUAL "11" ) OR ( ${CXX_STD} STREQUAL "14" ))
    ADD_TIMER_PROVISIONAL_TEST( test_Callgrind )
ENDIF()
IF ( CMAKE_Fortran_COMPILER )
    ADD_TIMER_TEST( test_ProfilerApp_Fortran )
    SET_TARGET_PROPERTIES( test_ProfilerApp_Fortran PROPERTIES LINKER_LANGUAGE Fortran )
    INCLUDE( CheckLinkerFlag )
    CHECK_LINKER_FLAG( Fortran -lstdc++ test_link_std )
    IF ( test_link_std )
        TARGET_LINK_LIBRARIES( test_ProfilerApp_Fortran stdc++ )
    ENDIF()
ENDIF()

# Regression test for ProfilerApp to ensure we can continue to load the data from older versions
SET( COPY_LIST
    set1.1.timer
    set1.2.timer
    set1.3.timer
    set1.4.timer
    set2.1.timer
    set2.2.timer
    set2.3.timer
    set2.4.timer
    set2.1.trace
    set2.2.trace
    set2.3.trace
    set2.4.trace
    set2.1.memory
    set2.2.memory
    set2.3.memory
    set2.4.memory
    ASE.0.timer )
FOREACH( tmp ${COPY_LIST} )
    COPY_TEST_FILE( ${tmp} )
ENDFOREACH()
ADD_TIMER_TEST( test_ProfilerAppRegression )
