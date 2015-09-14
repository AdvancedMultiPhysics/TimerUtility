INCLUDE( ../cmake/Find_QT.cmake )
FindQt()
IF ( CXX_STD STREQUAL "98" )
    MESSAGE("gui requires c++11 or higher")
ELSEIF( NOT Qt_FOUND )
    MESSAGE("Qt not found, gui will be disabled")
ELSEIF ( (NOT QWT_URL) AND (NOT QWT_SRC_DIR) )
    MESSAGE("Qwt source is not found, gui will be disabled."
            " Please set either QWT_URL or QWT_SRC_DIR" )
ELSE()
    # Set Qt include paths
    FOREACH( tmp ${QT_INCLUDES} )
        INCLUDE_DIRECTORIES( ${tmp} )
    ENDFOREACH()

    # Create Qwt library
    IF ( QWT_URL )
        SET( QWT_SOURCE_DIR "${CMAKE_BINARY_DIR}/QWT" )
        EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E make_directory ${QWT_SOURCE_DIR} )
        EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E tar xf ${QWT_URL} 
            WORKING_DIRECTORY "${QWT_SOURCE_DIR}" )
        FILE(GLOB QWT_SRC_DIR "${QWT_SOURCE_DIR}/qwt-*/src" )
        IF ( NOT QWT_SRC_DIR )
            MESSAGE(FATAL_ERROR "QWT_SRC_DIR not set, problem extracting?")
        ENDIF()
        MESSAGE("QWT_SRC_DIR = ${QWT_SRC_DIR}")
    ELSEIF( QWT_SRC_DIR )
        MESSAGE("QWT_SRC_DIR = ${QWT_SRC_DIR}")
    ELSE()
        MESSAGE(FATAL_ERROR "QWT_URL or QWT_SRC_DIR must be set")
    ENDIF()
    VERIFY_PATH( "${QWT_SRC_DIR}" )
    INCLUDE_DIRECTORIES( "${QWT_SRC_DIR}" )
    FILE( GLOB QWT_CPP_SOURCES "${QWT_SRC_DIR}/*.cpp" )
    FILE( GLOB QWT_HEADERS "${QWT_SRC_DIR}/*.h" )
    QT_WRAP_CPP(QWT_H_SOURCES ${QWT_HEADERS})
    ADD_LIBRARY( QWT ${QWT_CPP_SOURCES} ${QWT_H_SOURCES} )
    FOREACH( tmp ${QT_INCLUDES} )
        target_include_directories( QWT PUBLIC ${tmp} )
    ENDFOREACH()
    INSTALL( TARGETS QWT DESTINATION "${TIMER_INSTALL_DIR}/lib" )
    
    # Create the application
    SET(QtApp_SRCS load_timer.cpp timerwindow.cpp tracewindow.cpp LoadBalance.cpp MemoryPlot.cpp )
    SET(QtApp_MOCS timerwindow.h tracewindow.h LoadBalance.h MemoryPlot.h )
    SET(QtApp_RCCS application.qrc)
    # generate rules for building source files from the resources
    QT_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})
    # generate rules for building source files that moc generates
    QT_WRAP_CPP(QtApp_MOC_SRCS ${QtApp_MOCS})
    # build sources, moc'd sources, and rcc'd sources
    ADD_EXECUTABLE( load_timer ${QtApp_SRCS} ${QtApp_MOC_SRCS} ${QtApp_RCC_SRCS})
    TARGET_LINK_LIBRARIES(load_timer QWT ${QT_QTMAIN_LIBRARY} ${QT_LIBRARIES})
    ADD_PROJ_EXE_DEP( load_timer )
    INSTALL( TARGETS load_timer DESTINATION ${${PROJ}_INSTALL_DIR}/gui )

    # Add the tests
    SET( COPY_LIST  set1.1.timer  set1.2.timer  set1.3.timer  set1.4.timer
                    set2.1.timer  set2.2.timer  set2.3.timer  set2.4.timer 
                    set2.1.trace  set2.2.trace  set2.3.trace  set2.4.trace 
                    set2.1.memory set2.2.memory set2.3.memory set2.4.memory )
    FOREACH( tmp ${COPY_LIST} )
        CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/../test/data/${tmp} ${CMAKE_CURRENT_BINARY_DIR}/${tmp} COPYONLY )
    ENDFOREACH()
    ADD_TIMER_TEST( load_timer set1.1.timer set2.1.timer )
ENDIF()


