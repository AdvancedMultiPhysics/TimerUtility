# Add the data files needed for the tests

# Add a simple test calling MATLAB
ADD_MATLAB_TEST( test_hello_world )
SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES PASS_REGULAR_EXPRESSION "Hello World" )

# Add the MATLAB tests   


