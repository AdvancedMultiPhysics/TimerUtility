INCLUDE (macros)

# Add tests
ADD_TIMER_TEST_1_2_4( test_ProfilerApp )
ADD_TIMER_TEST( test_ProfilerApp_C )

# Regression test for ProfilerApp to ensure we can continue to load the data from older versions
COPY_TEST_DATA_FILE( set1.1.timer  set1.2.timer  set1.3.timer  set1.4.timer )
COPY_TEST_DATA_FILE( set2.1.timer  set2.2.timer  set2.3.timer  set2.4.timer 
                     set2.1.trace  set2.2.trace  set2.3.trace  set2.4.trace 
                     set2.1.memory set2.2.memory set2.3.memory set2.4.memory )
ADD_TIMER_TEST( test_ProfilerAppRegression )

