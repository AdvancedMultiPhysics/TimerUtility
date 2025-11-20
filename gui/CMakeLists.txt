# Set some CMake properties
CMAKE_MINIMUM_REQUIRED( VERSION 3.20.0 )
SET( DISABLE_LTO TRUE )

MESSAGE( "=============================" )
MESSAGE( "Configuring Timer Utility GUI" )
MESSAGE( "=============================" )

# Set the project name
SET( PROJ TIMER_GUI ) # Set the project name for CMake

# Initialize the project
PROJECT( ${PROJ} LANGUAGES CXX )

# Prevent users from building in place
IF ( "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()

# Set source/install paths
SET( ${PROJ}_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )
SET( ${PROJ}_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
IF ( ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${${PROJ}_INSTALL_DIR}" )
ELSEIF ( NOT ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()
SET( CMAKE_INSTALL_PREFIX ${${PROJ}_INSTALL_DIR} )
INCLUDE_DIRECTORIES( "${${PROJ}_INSTALL_DIR}/include" )
SET( CMAKE_MODULE_PATH ${${PROJ}_SOURCE_DIR} "${${PROJ}_SOURCE_DIR}/../cmake" ${CMAKE_MODULE_PATH} )

# Include CMake files
INCLUDE( ../cmake/macros.cmake )
INCLUDE( ../cmake/libraries.cmake )

# Include timer library
FIND_PACKAGE( Timer REQUIRED PATHS "${TIMER_INSTALL_DIR}" )

# Configure system libraries
CONFIGURE_SYSTEM()

# Find QT
CONFIGURE_QT()

# Find QWT
IF ( NOT QWT_INSTALL_DIR )
    MESSAGE( "FATAL_ERROR Qwt is not found.  Please set either QWT_INSTALL_DIR" )
ENDIF()
INCLUDE_DIRECTORIES( ${QWT_INSTALL_DIR}/include )
LINK_DIRECTORIES( ${QWT_INSTALL_DIR}/lib )
SET( CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib" )

# Strip coverage flags ( we want to add them only to our files ( not qwt )
REMOVE_DEFINITIONS( ${COVERAGE_FLAGS} )

# Set extra compiler flags that are necessary for qt
SET( QT_FLAGS )
IF ( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR ( ${CMAKE_C_COMPILER_ID} MATCHES "GNU" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" ) )
    SET( QT_FLAGS "-Wno-strict-overflow -Wno-overloaded-virtual -Wno-unused-variable" )
ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "(CLANG|Clang)" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "(CLANG|Clang)" ) )
    SET( QT_FLAGS "-Wno-deprecated-register -Wno-unused-function -Wno-tautological-constant-out-of-range-compare" )
ENDIF()

# Set Qt include paths
FOREACH( tmp ${QT_USE_FILE} )
    INCLUDE( ${tmp} )
ENDFOREACH()
SET( QT_INCLUDES ${${QT}_INCLUDES} )
FOREACH( tmp ${QT_INCLUDES} )
    INCLUDE_DIRECTORIES( ${tmp} )
ENDFOREACH()

# Setup rpath
SET( CMAKE_SKIP_BUILD_RPATH FALSE )
SET( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
SET( CMAKE_INSTALL_RPATH "${${PROJ}_INSTALL_DIR}/lib ${TIMER_GUI_INSTALL_DIR}/lib" "${TIMER_INSTALL_DIR}/lib" "${QWT_INSTALL_DIR}/lib" )

# Create the application
SET( QtApp_SRCS
    load_timer.cpp
    timerwindow.cpp
    tracewindow.cpp
    memorywindow.cpp
    LoadBalance.cpp
    MemoryPlot.cpp
    QSplitterGrid.cpp
    ThreadedSlotsClass.cpp )
SET( QtApp_MOCS
    timerwindow.h
    tracewindow.h
    memorywindow.h
    LoadBalance.h
    MemoryPlot.h
    QSplitterGrid.h
    ThreadedSlotsClass.h )
SET( QtApp_RCCS application.qrc )
IF ( ${QT_VERSION} EQUAL "4" )
    QT4_ADD_RESOURCES( QtApp_RCC_SRCS ${QtApp_RCCS} )
    QT4_WRAP_CPP( QtApp_MOC_SRCS ${QtApp_MOCS} )
ELSEIF ( ${QT_VERSION} EQUAL "5" )
    QT5_ADD_RESOURCES( QtApp_RCC_SRCS ${QtApp_RCCS} )
    QT5_WRAP_CPP( QtApp_MOC_SRCS ${QtApp_MOCS} )
ENDIF()
# build sources, moc'd sources, and rcc'd sources
ADD_EXECUTABLE( load_timer ${QtApp_SRCS} ${QtApp_MOC_SRCS} ${QtApp_RCC_SRCS} )
INSTALL( TARGETS load_timer DESTINATION ${${PROJ}_INSTALL_DIR}/bin )
SET_TARGET_PROPERTIES( load_timer PROPERTIES COMPILE_FLAGS "${QT_FLAGS} ${COVERAGE_FLAGS}" )
TARGET_LINK_LIBRARIES( load_timer Timer::timerutility ${SYSTEM_LIBS} )
TARGET_LINK_LIBRARIES( load_timer qwt )
TARGET_LINK_LIBRARIES( load_timer ${${QT}_QTMAIN_LIBRARY} ${${QT}_LIBRARIES} )
FOREACH( tmp ${QT_COMPONENTS} )
    TARGET_LINK_LIBRARIES( load_timer ${Qt}::${tmp} )
ENDFOREACH()
GLOBAL_SET( load_timer-BINDIR "${CMAKE_CURRENT_BINARY_DIR}" )
IF ( NOT "${CMAKE_CURRENT_BINARY_DIR}" MATCHES "${${PROJ}_INSTALL_DIR}/gui" )
    INSTALL( TARGETS load_timer DESTINATION ${${PROJ}_INSTALL_DIR}/bin )
    INSTALL( TARGETS load_timer DESTINATION ${TIMER_INSTALL_DIR}/bin )
ENDIF()

# Add the tests
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
    set2.4.memory )
FOREACH( tmp ${COPY_LIST} )
    CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/../test/data/${tmp} ${CMAKE_CURRENT_BINARY_DIR}/${tmp} COPYONLY )
ENDFOREACH()
ADD_TIMER_GUI_TEST( load_timer set1.1.timer set2.1.timer )
