SET( QT_COMPONENTS Core Widgets Gui Sql Chart )
#FIND_PACKAGE( Qt5 COMPONENTS ${QT_COMPONENTS} )
FIND_PACKAGE(Qt4 COMPONENTS QtCore QtGui QtSql QtSvg QtOpenGL )
IF ( CXX_STD STREQUAL "98" )
    MESSAGE("gui requires c++11 or higher")
ELSEIF( NOT Qt4_FOUND )
    MESSAGE("Qt not found, gui will be disabled")
ELSEIF ( (NOT QWT_URL) AND (NOT SRC_DIR) )
    MESSAGE("Qwt source is not found, gui will be disabled."
            " Please set either QWT_URL or QWT_SRC_DIR" )
ELSE()
    # Create Qwt library
    IF ( QWT_URL )
        SET( QWT_SOURCE_DIR "${CMAKE_BINARY_DIR}/QWT" )
        EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E make_directory ${QWT_SOURCE_DIR} )
        EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E tar xf ${QWT_URL} 
            WORKING_DIRECTORY "${QWT_SOURCE_DIR}" )
        FILE(GLOB QWT_SRC_DIR "${QWT_SOURCE_DIR}/qwt-*/src" )
        MESSAGE("QWT_SRC_DIR = ${QWT_SRC_DIR}")
    ELSE()
        SET( QWT_SRC_DIR "${QWT_SOURCE_DIR}/src" )
    ENDIF()
    VERIFY_PATH( ${QWT_SRC_DIR} )
    INCLUDE_DIRECTORIES( "${QWT_SRC_DIR}" )
    FILE( GLOB QWT_CPP_SOURCES "${QWT_SRC_DIR}/*.cpp" )
    FILE( GLOB QWT_HEADERS "${QWT_SRC_DIR}/*.h" )
    QT4_WRAP_CPP(QWT_H_SOURCES ${QWT_HEADERS})
    ADD_LIBRARY( QWT ${QWT_CPP_SOURCES} ${QWT_H_SOURCES} )
    INSTALL( TARGETS ${PACKAGE} DESTINATION "${TIMER_INSTALL_DIR}/lib" )

    INCLUDE( ${QT_USE_FILE} )
    #FOREACH( tmp ${QT_COMPONENTS} )
    #    INCLUDE_DIRECTORIES( ${Qt5${tmp}_INCLUDE_DIRS} )
    #ENDFOREACH()
    #, ${Qt5Widgets_LIBRARIES}, ${Qt5Declarative_INCLUDE_DIRS}, ${Qt5Declarative_LIBRARIES}
    
    SET(QtApp_SRCS load_timer.cpp timerwindow.cpp tracewindow.cpp LoadBalance.cpp )
    SET(QtApp_MOCS timerwindow.h tracewindow.h LoadBalance.h )
    SET(QtApp_RCCS application.qrc)
    # generate rules for building source files from the resources
    QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})
    # generate rules for building source files that moc generates
    QT4_WRAP_CPP(QtApp_MOC_SRCS ${QtApp_MOCS})
    # build sources, moc'd sources, and rcc'd sources
    ADD_EXECUTABLE( load_timer ${QtApp_SRCS} ${QtApp_MOC_SRCS} ${QtApp_RCC_SRCS})
    TARGET_LINK_LIBRARIES(load_timer QWT ${QT_QTMAIN_LIBRARY} ${QT_LIBRARIES})
    ADD_PROJ_EXE_DEP( load_timer )
ENDIF()


