FIND_PACKAGE(Qt4 COMPONENTS QtCore QtGui QtSql REQUIRED)
IF ( CXX_STD STREQUAL "98" )
    MESSAGE("gui requires c++11 or higher")
ELSEIF( NOT Qt4_FOUND )
    MESSAGE("QT not found, gui will be disabled")
ELSE()
    INCLUDE(${QT_USE_FILE})

    SET(QtApp_SRCS load_timer.cpp timerwindow.cpp)
    SET(QtApp_MOCS timerwindow.h)
    SET(QtApp_RCCS application.qrc)
    # generate rules for building source files from the resources
    QT4_ADD_RESOURCES(QtApp_RCC_SRCS ${QtApp_RCCS})
    # generate rules for building source files that moc generates
    QT4_WRAP_CPP(QtApp_MOC_SRCS ${QtApp_MOCS})
    # build sources, moc'd sources, and rcc'd sources
    ADD_EXECUTABLE( load_timer ${QtApp_SRCS} ${QtApp_MOC_SRCS} ${QtApp_RCC_SRCS})
    TARGET_LINK_LIBRARIES(load_timer ${QT_QTMAIN_LIBRARY} ${QT_LIBRARIES})
    ADD_PROJ_EXE_DEP( load_timer )
ENDIF()


