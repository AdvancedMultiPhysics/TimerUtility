CHECK_ENABLE_FLAG( ENABLE_GUI 1 )
IF ( NOT ENABLE_GUI )
    MESSAGE("gui disabled")
    RETURN()
ENDIF()
FIND_PACKAGE( Qt4 COMPONENTS QtCore QtGui QtOpenGL QtSvg QtSql REQUIRED )
IF( NOT Qt4_FOUND )
    MESSAGE("Qt not found, gui will be disabled")
    RETURN()
ENDIF()
IF ( (NOT QWT_URL) AND (NOT QWT_SRC_DIR) )
    MESSAGE("Qwt source is not found, gui will be disabled."
            " Please set either QWT_URL or QWT_SRC_DIR" )
    RETURN()
ENDIF()

# Strip coverage flags (we want to add them only to our files (not qwt)
REMOVE_DEFINITIONS( ${COVERAGE_FLAGS} )

# Set extra compiler flags that are necessary for qt
SET( QT4_FLAGS )
IF ( USING_GCC )
    SET( QT4_FLAGS "-Wno-strict-overflow -Wno-overloaded-virtual -Wno-unused-variable" )
ELSEIF ( USING_CLANG )
    SET( QT4_FLAGS "-Wno-deprecated-register -Wno-unused-function -Wno-tautological-constant-out-of-range-compare" )
ENDIF()

# Set Qt include paths
INCLUDE( ${QT_USE_FILE} )
FOREACH( tmp ${QT4_INCLUDES} )
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
QT4_WRAP_CPP(QWT_H_SOURCES ${QWT_HEADERS})
ADD_LIBRARY( QWT ${QWT_CPP_SOURCES} ${QWT_H_SOURCES} )
SET_TARGET_PROPERTIES( QWT PROPERTIES COMPILE_FLAGS "${QT4_FLAGS}" )
FOREACH( tmp ${QT4_INCLUDES} )
    target_include_directories( QWT PUBLIC ${tmp} )
ENDFOREACH()
INSTALL( TARGETS QWT DESTINATION "${TIMER_INSTALL_DIR}/lib" )

# Create the application
SET(QtApp_SRCS load_timer.cpp timerwindow.cpp tracewindow.cpp LoadBalance.cpp MemoryPlot.cpp QSplitterGrid.cpp ThreadedSlotsClass.cpp )
SET(QtApp_MOCS timerwindow.h tracewindow.h LoadBalance.h MemoryPlot.h QSplitterGrid.h ThreadedSlotsClass.h )
SET(QtApp_RCCS application.qrc)
# generate rules for building source files from the resources
QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})
# generate rules for building source files that moc generates
QT4_WRAP_CPP(QtApp_MOC_SRCS ${QtApp_MOCS})
# build sources, moc'd sources, and rcc'd sources
ADD_EXECUTABLE( load_timer ${QtApp_SRCS} ${QtApp_MOC_SRCS} ${QtApp_RCC_SRCS} )
INSTALL( TARGETS load_timer DESTINATION ${${PROJ}_INSTALL_DIR}/bin )
SET_TARGET_PROPERTIES( load_timer PROPERTIES COMPILE_FLAGS "${QT4_FLAGS} ${COVERAGE_FLAGS}" )
TARGET_LINK_LIBRARIES( load_timer QWT ${QT4_QTMAIN_LIBRARY} ${QT4_LIBRARIES} )
TARGET_LINK_LIBRARIES( load_timer Qt4::QtSql )
TARGET_LINK_LIBRARIES( load_timer Qt4::QtSvg )
TARGET_LINK_LIBRARIES( load_timer Qt4::QtOpenGL )
TARGET_LINK_LIBRARIES( load_timer Qt4::QtGui )
ADD_PROJ_EXE_DEP( load_timer )
GLOBAL_SET( load_timer-BINDIR "${CMAKE_CURRENT_BINARY_DIR}" )
IF ( NOT "${CMAKE_CURRENT_BINARY_DIR}" MATCHES "${${PROJ}_INSTALL_DIR}/gui" )
    INSTALL( TARGETS load_timer DESTINATION ${${PROJ}_INSTALL_DIR}/gui )
ENDIF()

# Add the tests
SET( COPY_LIST  set1.1.timer  set1.2.timer  set1.3.timer  set1.4.timer
                set2.1.timer  set2.2.timer  set2.3.timer  set2.4.timer 
                set2.1.trace  set2.2.trace  set2.3.trace  set2.4.trace 
                set2.1.memory set2.2.memory set2.3.memory set2.4.memory )
FOREACH( tmp ${COPY_LIST} )
    CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/../test/data/${tmp} ${CMAKE_CURRENT_BINARY_DIR}/${tmp} COPYONLY )
ENDFOREACH()
ADD_TIMER_TEST( load_timer set1.1.timer set2.1.timer )


