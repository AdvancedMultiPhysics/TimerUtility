CMAKE_MINIMUM_REQUIRED( VERSION 3.28.3 )
CMAKE_POLICY( SET CMP0057 NEW )
CMAKE_POLICY( SET CMP0110 NEW )
CMAKE_POLICY( SET CMP0115 NEW )

INCLUDE( CheckCCompilerFlag )
INCLUDE( CheckCSourceCompiles )
INCLUDE( CheckCXXCompilerFlag )
INCLUDE( CheckCXXSourceCompiles )

IF ( NOT TEST_FAIL_REGULAR_EXPRESSION )
    # Note: we cannot check for "handles are still allocated" due to PETSc.
    #    See static variable Petsc_Reduction_keyval on line 234 of comb.c
    # SET( TEST_FAIL_REGULAR_EXPRESSION "( FAILED )|( leaked context IDs detected )|( handles are still allocated )" )
    SET( TEST_FAIL_REGULAR_EXPRESSION "( FAILED )" )
ENDIF()

# Check that the PROJ and ${PROJ}_INSTALL_DIR variables are set
# These variables are used to generate the ADD_PROJ_TEST macros
IF ( NOT PROJ )
    MESSAGE( FATAL_ERROR "PROJ must be set before including macros.cmake" )
ENDIF()
IF ( NOT ${PROJ}_INSTALL_DIR )
    MESSAGE( FATAL_ERROR "${PROJ}_INSTALL_DIR must be set before including macros.cmake" )
ENDIF()
IF ( NOT CMAKE_BUILD_TYPE AND NOT ONLY_BUILD_DOCS )
    MESSAGE( FATAL_ERROR "CMAKE_BUILD_TYPE must be set before including macros.cmake" )
ENDIF()

# Enable json
SET( CMAKE_EXPORT_COMPILE_COMMANDS ON )


# Set default compiler id
IF ( NOT DEFAULT_COMPILER_ID )
    IF ( CMAKE_CXX_COMPILER_ID )
        SET( DEFAULT_COMPILER_ID ${CMAKE_CXX_COMPILER_ID} )
    ELSEIF ( CMAKE_C_COMPILER_ID )
        SET( DEFAULT_COMPILER_ID ${CMAKE_C_COMPILER_ID} )
    ELSEIF ( CMAKE_Fortran_COMPILER_ID )
        SET( DEFAULT_COMPILER_ID ${CMAKE_Fortran_COMPILER_ID} )
    ELSE()
        AMP_ERROR( "Default compiler ID not set" )
    ENDIF()
ENDIF()


# Check for link time optimization ( LTO )
IF ( "${CMAKE_BUILD_TYPE}" STREQUAL "Release" AND NOT DISABLE_LTO AND NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION )
    INCLUDE( CheckIPOSupported )
    CHECK_IPO_SUPPORTED( RESULT supported OUTPUT error )
    IF ( supported )
        MESSAGE( STATUS "IPO / LTO enabled" )
        SET( LTO TRUE )
        SET( CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE )
    ELSE()
        SET( LTO FALSE )
        MESSAGE( STATUS "IPO / LTO not supported" )
    ENDIF()
ELSEIF ( NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION )
    SET( LTO FALSE )
    MESSAGE( STATUS "IPO / LTO disabled" )
ENDIF()

# Set default value
MACRO( SET_DEFAULT FLAG ${ARGN} )
    IF ( NOT DEFINED ${FLAG} )
        SET( ${FLAG} ${ARGN} )
    ENDIF()
ENDMACRO()

# Check if a flag is enabled
MACRO( CHECK_ENABLE_FLAG FLAG DEFAULT )
    IF ( NOT DEFINED ${FLAG} )
        SET( ${FLAG} ${DEFAULT} )
    ELSEIF ( ${FLAG} STREQUAL "" )
        SET( ${FLAG} ${DEFAULT} )
    ELSEIF ( (${${FLAG}} STREQUAL "FALSE" ) OR ( ${${FLAG}} STREQUAL "false" ) OR ( ${${FLAG}} STREQUAL "0" ) OR ( ${${FLAG}} STREQUAL "OFF" ) )
        SET( ${FLAG} 0 )
    ELSEIF ( (${${FLAG}} STREQUAL "TRUE" ) OR ( ${${FLAG}} STREQUAL "true" ) OR ( ${${FLAG}} STREQUAL "1" ) OR ( ${${FLAG}} STREQUAL "ON" ) )
        SET( ${FLAG} 1 )
    ELSE()
        MESSAGE( "Bad value for ${FLAG} ( ${${FLAG}} ); use true or false" )
    ENDIF()
ENDMACRO()

# Check for gold linker
IF ( UNIX AND NOT APPLE AND NOT DISABLE_GOLD AND NOT DEFINED CMAKE_LINKER_TYPE )
    SET( CMAKE_LINKER_TYPE DEFAULT )
    INCLUDE( CheckLinkerFlag )
    IF ( DISABLE_GOLD )
        MESSAGE( "DISABLE_GOLD is deprecated, please set CMAKE_LINKER_TYPE (DEFAULT) instead" )
    ELSEIF ( "${DEFAULT_COMPILER_ID}" MATCHES "GNU" )
        CHECK_LINKER_FLAG( CXX "-fuse-ld=gold" test_gold )
        IF ( test_gold )
            MESSAGE( "Using gold linker" )
            SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold" )
            SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold" )
        ENDIF()
    ELSEIF ( "${DEFAULT_COMPILER_ID}" MATCHES "(Clang|LLVM)" )
        CHECK_LINKER_FLAG( CXX "-fuse-ld=lld" test_lld )
        IF ( test_lld )
            MESSAGE( "Using lld linker" )
            SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld" )
            SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld" )
        ENDIF()
    ENDIF()
ENDIF()

# Add some default targets if they do not exist
IF ( NOT TARGET copy-${PROJ}-Data )
    ADD_CUSTOM_TARGET( copy-${PROJ}-Data ALL )
ENDIF()
IF ( NOT TARGET copy-${PROJ}-include )
    ADD_CUSTOM_TARGET( copy-${PROJ}-include ALL )
ENDIF()
IF ( NOT TARGET clean-makefile )
    ADD_CUSTOM_TARGET( clean-makefile ALL )
    ADD_DEPENDENCIES( copy-${PROJ}-Data clean-makefile )
    ADD_DEPENDENCIES( copy-${PROJ}-include clean-makefile )
    ADD_CUSTOM_COMMAND( TARGET clean-makefile PRE_BUILD COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_BINARY_DIR}/time.log" )
ENDIF()
IF ( NOT TARGET build-test )
    ADD_CUSTOM_TARGET( build-test )
ENDIF()
IF ( NOT TARGET build-examples )
    ADD_CUSTOM_TARGET( build-examples )
ENDIF()
IF ( NOT TARGET check )
    ADD_CUSTOM_TARGET( check )
ENDIF()

# Dummy use to prevent unused cmake variable warning
FUNCTION( NULL_USE VAR )
    IF ( "${${VAR}}" STREQUAL "dummy" )
        MESSAGE( FATAL_ERROR "NULL_USE fail" )
    ENDIF()
ENDFUNCTION()
NULL_USE( CMAKE_C_FLAGS )

# Set a global variable
MACRO( GLOBAL_SET VARNAME )
    SET( ${VARNAME} ${ARGN} CACHE INTERNAL "" )
ENDMACRO()

# Print all variables
FUNCTION( PRINT_ALL_VARIABLES )
    GET_CMAKE_PROPERTY( _variableNames VARIABLES )
    FOREACH( _variableName ${_variableNames} )
        MESSAGE( STATUS "${_variableName}=${${_variableName}}" )
    ENDFOREACH()
ENDFUNCTION()

# CMake assert
FUNCTION( ASSERT test comment )
    IF ( NOT ${test} )
        MESSSAGE( FATAL_ERROR "Assertion failed: ${comment}" )
    ENDIF()
ENDFUNCTION()

# Function to get the number of physical processors
# Note: CMake uses OMP_NUM_THREADS which we don't want to use
FUNCTION( GETPHYSICALPROCS VAR )
    INCLUDE( ProcessorCount )
    SET( OMP_NUM_THREADS $ENV{OMP_NUM_THREADS} )
    SET( ENV{OMP_NUM_THREADS} )
    PROCESSORCOUNT( PROCS )
    SET( ${VAR} ${PROCS} PARENT_SCOPE )
    SET( ENV{OMP_NUM_THREADS} ${OMP_NUM_THREADS} )
ENDFUNCTION()

# Set the maximum number of processors
IF ( NOT TEST_MAX_PROCS )
    GETPHYSICALPROCS( TEST_MAX_PROCS )
    IF ( ${TEST_MAX_PROCS} EQUAL 0 )
        SET( TEST_MAX_PROCS 16 )
    ENDIF()
ENDIF()

# Convert a m4 file
# This command converts a file of the format "global_path/file.m4"
# and convertes it to file.F.  It also requires the path.
MACRO( CONVERT_M4_FORTRAN IN LOCAL_PATH OUT_PATH )
    STRING( REGEX REPLACE ${LOCAL_PATH} "" OUT ${IN} )
    STRING( REGEX REPLACE "/" "" OUT ${OUT} )
    STRING( REGEX REPLACE "( .fm4 )|( .m4 )" ".F" OUT "${CMAKE_CURRENT_BINARY_DIR}/${OUT_PATH}/${OUT}" )
    IF ( NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${OUT_PATH}" )
        FILE( MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${OUT_PATH}" )
    ENDIF()
    IF ( "${CMAKE_GENERATOR}" STREQUAL "Xcode" )
        STRING( REGEX REPLACE ".F" ".o" OUT2 "${OUT}" )
        STRING( REGEX REPLACE ";" " " COMPILE_CMD "${CMAKE_Fortran_COMPILER} -c ${OUT} ${CMAKE_Fortran_FLAGS} -o ${OUT2}" )
        STRING( REGEX REPLACE "\\\\" "" COMPILE_CMD "${COMPILE_CMD}" )
        MESSAGE( "COMPILE_CMD = ${COMPILE_CMD}" )
        SET( COMPILE_CMD ${COMPILE_CMD} )
        ADD_CUSTOM_COMMAND( OUTPUT ${OUT2} COMMAND m4 -I${LOCAL_PATH} -I${SAMRAI_FORTDIR} ${M4DIRS} ${IN} > ${OUT} COMMAND ${COMPILE_CMD} DEPENDS ${IN} )
        SET_SOURCE_FILES_PROPERTIES( ${OUT2} PROPERTIES GENERATED true )
        SET( SOURCES ${SOURCES} "${OUT2}" )
    ELSE()
        ADD_CUSTOM_COMMAND( OUTPUT ${OUT} COMMAND m4 -I${LOCAL_PATH} -I${SAMRAI_FORTDIR} ${M4DIRS} ${M4_OPTIONS} ${IN} > ${OUT} DEPENDS ${IN} )
        SET_SOURCE_FILES_PROPERTIES( ${OUT} PROPERTIES GENERATED true )
        SET( SOURCES ${SOURCES} "${OUT}" )
    ENDIF()
ENDMACRO()

# Add a package to the project's library
MACRO( ADD_${PROJ}_LIBRARY PACKAGE )
    SET( CURRENT_LIBRARY ${PACKAGE} )
    ADD_SUBDIRECTORY( ${PACKAGE} )
    SET( CURRENT_LIBRARY )
ENDMACRO()

# Add a project executable
MACRO( ADD_${PROJ}_EXECUTABLE EXEFILE )
    IF ( TARGET ${EXEFILE} AND NOT INCLUDE_${EXEFILE} )
        MESSAGE( FATAL_ERROR "ADD_${PROJ}_EXECUTABLE should be called before adding tests" )
    ENDIF()
    SET( INCLUDE_${EXEFILE} TRUE )
    ADD_PROJ_PROVISIONAL_TEST( ${EXEFILE} )
    INSTALL( TARGETS ${EXEFILE} DESTINATION ${${PROJ}_INSTALL_DIR}/bin )
ENDMACRO()

# Initialize a package
MACRO( BEGIN_PACKAGE_CONFIG PACKAGE )
    SET( HEADERS "" )
    SET( CXXSOURCES "" )
    SET( CSOURCES "" )
    SET( FSOURCES "" )
    SET( M4FSOURCES "" )
    SET( CUDASOURCES "" )
    SET( HIPSOURCES "" )
    SET( SOURCES "" )
    SET( CURPACKAGE ${PACKAGE} )
    IF ( NOT DEFINED PACKAGE_NAME )
        SET( PACKAGE_NAME "${PROJ}_${PACKAGE}" )
        STRING( TOUPPER "${PACKAGE_NAME}" PACKAGE_NAME )
    ENDIF()
ENDMACRO()

# Find the source files
MACRO( FIND_FILES )
    # Find the C/C++ headers
    SET( T_HEADERS "" )
    FILE(
        GLOB
        T_HEADERS
        "*.h"
        "*.H"
        "*.hh"
        "*.hpp"
        "*.I" )
    # Find the CUDA sources
    SET( T_CUDASOURCES "" )
    FILE( GLOB T_CUDASOURCES "*.cu" )
    # Find the HIP sources
    SET( T_HIPSOURCES "" )
    FILE( GLOB T_HIPSOURCES "*.cu" "*.hip" )
    # Find the C sources
    SET( T_CSOURCES "" )
    FILE( GLOB T_CSOURCES "*.c" )
    # Find the C++ sources
    SET( T_CXXSOURCES "" )
    FILE( GLOB T_CXXSOURCES "*.cc" "*.cpp" "*.cxx" "*.C" )
    # Find the Fortran sources
    SET( T_FSOURCES "" )
    FILE( GLOB T_FSOURCES "*.f" "*.f90" "*.F" "*.F90" )
    # Find the m4 fortran source ( and convert )
    SET( T_M4FSOURCES "" )
    FILE( GLOB T_M4FSOURCES "*.m4" "*.fm4" )
    FOREACH( m4file ${T_M4FSOURCES} )
        CONVERT_M4_FORTRAN( ${m4file} ${CMAKE_CURRENT_SOURCE_DIR} "" )
    ENDFOREACH()
    # Add all found files to the current lists
    SET( HEADERS ${HEADERS} ${T_HEADERS} )
    SET( CXXSOURCES ${CXXSOURCES} ${T_CXXSOURCES} )
    SET( CUDASOURCES ${CUDASOURCES} ${T_CUDASOURCES} )
    SET( CUDASOURCES ${HIPSOURCES} ${T_HIPSOURCES} )
    SET( CSOURCES ${CSOURCES} ${T_CSOURCES} )
    SET( FSOURCES ${FSOURCES} ${T_FSOURCES} )
    SET( M4FSOURCES ${M4FSOURCES} ${T_M4FSOURCES} )
    SET( SOURCES
        ${SOURCES}
        ${T_CXXSOURCES}
        ${T_CSOURCES}
        ${T_FSOURCES}
        ${T_M4FSOURCES}
        ${T_CUDASOURCES}
        ${T_HIPSOURCES} )
ENDMACRO()

# Find the source files
MACRO( FIND_FILES_PATH IN_PATH )
    # Find the C/C++ headers
    SET( T_HEADERS "" )
    FILE(
        GLOB
        T_HEADERS
        "${IN_PATH}/*.h"
        "${IN_PATH}/*.H"
        "${IN_PATH}/*.hh"
        "${IN_PATH}/*.hpp"
        "${IN_PATH}/*.I" )
    # Find the CUDA sources
    SET( T_CUDASOURCES "" )
    FILE( GLOB T_CUDASOURCES "${IN_PATH}/*.cu" )
    # Find the HIP sources
    SET( T_HIPSOURCES "" )
    FILE( GLOB T_HIPSOURCES "${IN_PATH}/*.cu" "${IN_PATH}/*.hip" )
    # Find the C sources
    SET( T_CSOURCES "" )
    FILE( GLOB T_CSOURCES "${IN_PATH}/*.c" )
    # Find the C++ sources
    SET( T_CXXSOURCES "" )
    FILE( GLOB T_CXXSOURCES "${IN_PATH}/*.cc" "${IN_PATH}/*.cpp" "${IN_PATH}/*.cxx" "${IN_PATH}/*.C" )
    # Find the Fortran sources
    SET( T_FSOURCES "" )
    FILE( GLOB T_FSOURCES "${IN_PATH}/*.f" "${IN_PATH}/*.f90" "${IN_PATH}/*.F" "${IN_PATH}/*.F90" )
    # Find the m4 fortran source ( and convert )
    SET( T_M4FSOURCES "" )
    FILE( GLOB T_M4FSOURCES "${IN_PATH}/*.m4" "${IN_PATH}/*.fm4" )
    FOREACH( m4file ${T_M4FSOURCES} )
        CONVERT_M4_FORTRAN( ${m4file} ${CMAKE_CURRENT_SOURCE_DIR}/${IN_PATH} ${IN_PATH} )
    ENDFOREACH()
    # Add all found files to the current lists
    SET( HEADERS ${HEADERS} ${T_HEADERS} )
    SET( CXXSOURCES ${CXXSOURCES} ${T_CXXSOURCES} )
    SET( CUDASOURCES ${CUDASOURCES} ${T_CUDASOURCES} )
    SET( HIPSOURCES ${HIPSOURCES} ${T_HIPSOURCES} )
    SET( CSOURCES ${CSOURCES} ${T_CSOURCES} )
    SET( FSOURCES ${FSOURCES} ${T_FSOURCES} )
    SET( SOURCES ${SOURCES} ${T_CXXSOURCES} ${T_CSOURCES} ${T_FSOURCES} ${T_CUDASOURCES} ${T_HIPSOURCES} )
ENDMACRO()

# Add a subdirectory
MACRO( ADD_PACKAGE_SUBDIRECTORY SUBDIR )
    FIND_FILES_PATH( ${SUBDIR} )
ENDMACRO()

# Add an external directory Note: this will add the files to compile but will not copy the headers
MACRO( ADD_EXTERNAL_DIRECTORY SUBDIR )
    FIND_FILES_PATH( ${SUBDIR} )
ENDMACRO()

# Add assembly target
IF ( NOT TARGET assembly )
    ADD_CUSTOM_TARGET( assembly )
ENDIF()
MACRO( ADD_ASSEMBLY PACKAGE SOURCE )
    # Create the assembly targets
    IF ( CMAKE_GENERATOR STREQUAL "Unix Makefiles" )
        # The trick is that makefiles generator defines a [sourcefile].s target for each sourcefile of a target to generate the listing
        # of that file. We hook a command after build to invoke that target and copy the result file to our ourput directory:
        SET( MAKE_TARGETS )
        SET( COPY_COMMANDS )
        FOREACH( src ${SOURCES} )
            GET_SOURCE_FILE_PROPERTY( lang "${src}" LANGUAGE )
            IF ( lang STREQUAL "C" OR lang STREQUAL "CXX" OR lang STREQUAL "Fortran" )
                STRING( REPLACE "${${PROJ}_SOURCE_DIR}/" "" src_out "${src}" )
                STRING( REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" src "${src}" )
                SET( MAKE_TARGETS ${MAKE_TARGETS} ${src}.s )
                SET( COPY_COMMANDS ${COPY_COMMANDS} COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PACKAGE}.dir/${src}.s"
                                  "${CMAKE_BINARY_DIR}/assembly/${src_out}.s" )
            ENDIF()
        ENDFOREACH()
        IF ( MAKE_TARGETS )
            ADD_CUSTOM_TARGET( ${PACKAGE}-assembly )
            ADD_CUSTOM_COMMAND( TARGET ${PACKAGE}-assembly PRE_LINK COMMAND $(MAKE) ${MAKE_TARGETS} ${COPY_COMMANDS} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
            ADD_DEPENDENCIES( assembly ${PACKAGE}-assembly )
        ENDIF()
    ENDIF()
ENDMACRO()

# Install a package
MACRO( INSTALL_${PROJ}_TARGET PACKAGE )
    # Find all files in the current directory
    FIND_FILES()
    # Create the copy target
    SET( COPY_TARGET "copy-${PROJ}-${CMAKE_CURRENT_SOURCE_DIR}-include" )
    STRING( REGEX REPLACE "${${PROJ}_SOURCE_DIR}/" "" COPY_TARGET "${COPY_TARGET}" )
    STRING( REGEX REPLACE "${${PROJ}_SOURCE_DIR}" "" COPY_TARGET "${COPY_TARGET}" )
    STRING( REGEX REPLACE "/" "-" COPY_TARGET ${COPY_TARGET} )
    IF ( NOT TARGET ${COPY_TARGET} )
        ADD_CUSTOM_TARGET( ${COPY_TARGET} ALL )
        ADD_DEPENDENCIES( copy-${PROJ}-include ${COPY_TARGET} )
    ENDIF()
    # Copy the header files to the include path
    IF ( HEADERS )
        FILE( GLOB HFILES RELATIVE "${${PROJ}_SOURCE_DIR}" ${HEADERS} )
        FOREACH( HFILE ${HFILES} )
            SET( SRC_FILE "${${PROJ}_SOURCE_DIR}/${HFILE}" )
            SET( DST_FILE "${${PROJ}_INSTALL_DIR}/include/${${PROJ}_INC}/${HFILE}" )
            # Only copy the headers if they exist in the project source directory
            IF ( EXISTS "${SRC_FILE}" )
                ADD_CUSTOM_COMMAND( TARGET ${COPY_TARGET} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_FILE}" "${DST_FILE}" )
            ENDIF()
        ENDFOREACH()
    ENDIF()
    # Add the library and install the package
    IF ( NOT ONLY_BUILD_DOCS AND SOURCES )
        # Set RPATH variables
        IF ( NOT CMAKE_RPATH_VARIABLES_SET AND NOT USE_STATIC )
            SET( CMAKE_RPATH_VARIABLES_SET ON )
            SET( CMAKE_SKIP_BUILD_RPATH FALSE )
            SET( CMAKE_BUILD_WITH_INSTALL_RPATH FALSE )
            SET( CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} "${CMAKE_INSTALL_PREFIX}/lib" )
            SET( CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE )
            SET( MACOSX_RPATH 0 )
            LIST( FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir )
        ENDIF()
        # Add the library to the project libs list
        SET( ${PROJ}_LIBS ${${PROJ}_LIBS} ${PACKAGE} CACHE INTERNAL "" )
        LIST( REMOVE_DUPLICATES ${PROJ}_LIBS )
        SET( ${PROJ}_LIBS ${${PROJ}_LIBS} CACHE INTERNAL "" )
        # If we are using CUDA and COMPILE_CXX_AS_CUDA is set, change the language
        IF ( USE_CUDA AND COMPILE_CXX_AS_CUDA )
            SET_SOURCE_FILES_PROPERTIES( ${CXXSOURCES} PROPERTIES LANGUAGE CUDA )
        ENDIF()
        # If we are using HIP and COMPILE_CXX_AS_HIP is set, change the language
        IF ( USE_HIP AND COMPILE_CXX_AS_HIP )
            SET_SOURCE_FILES_PROPERTIES( ${CXXSOURCES} PROPERTIES LANGUAGE HIP )
        ENDIF()
        # Create the library
        IF ( ${PROJ}_LIB )
            # We are using a single project library
            ADD_LIBRARY( ${PACKAGE} OBJECT ${SOURCES} )
        ELSE()
            # We are creating individual libraries
            ADD_LIBRARY( ${PACKAGE} ${LIB_TYPE} ${SOURCES} ${CUBINS} )
            TARGET_LINK_EXTERNAL_LIBRARIES( ${PACKAGE} )
        ENDIF()
        # Create the assembly targets
        ADD_ASSEMBLY( ${PACKAGE} ${SOURCES} )
        # Add coverage flags to target
        IF ( NOT DISABLE_TARGET_COVERAGE )
            TARGET_COMPILE_DEFINITIONS( ${PACKAGE} PUBLIC ${COVERAGE_FLAGS} )
        ENDIF()
        # Add target dependencies
        IF ( TARGET write_repo_version )
            ADD_DEPENDENCIES( ${PACKAGE} write_repo_version )
        ENDIF()
        ADD_DEPENDENCIES( ${PACKAGE} copy-${PROJ}-include )
        IF ( NOT ${PROJ}_LIB )
            INSTALL( TARGETS ${PACKAGE} DESTINATION ${${PROJ}_INSTALL_DIR}/lib )
        ENDIF()
    ELSE()
        ADD_CUSTOM_TARGET( ${PACKAGE} ALL )
    ENDIF()
    # Clear the sources
    SET( HEADERS "" )
    SET( CSOURCES "" )
    SET( CXXSOURCES "" )
ENDMACRO()

# Install the project library
MACRO( INSTALL_PROJ_LIB )
    SET( tmp_link_list )
    FOREACH( tmp ${${PROJ}_LIBS} )
        SET( tmp_link_list ${tmp_link_list} $<TARGET_OBJECTS:${tmp}> )
    ENDFOREACH()
    ADD_LIBRARY( ${${PROJ}_LIB} ${LIB_TYPE} ${tmp_link_list} )
    TARGET_LINK_LIBRARIES( ${${PROJ}_LIB} LINK_PUBLIC ${COVERAGE_LIBS} ${SYSTEM_LIBS} )
    TARGET_LINK_EXTERNAL_LIBRARIES( ${${PROJ}_LIB} LINK_PUBLIC )
    IF ( NOT DISABLE_TARGETS_EXPORT )
        INSTALL( TARGETS ${${PROJ}_LIB} EXPORT ${PROJ}Targets DESTINATION ${${PROJ}_INSTALL_DIR}/lib )
        INSTALL( EXPORT ${PROJ}Targets FILE ${PROJ}Targets.cmake NAMESPACE ${PROJ}:: DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/${PROJ} )
    ENDIF()
ENDMACRO()

# Verify that a variable has been set
FUNCTION( VERIFY_VARIABLE VARIABLE_NAME )
    IF ( NOT ${VARIABLE_NAME} )
        MESSAGE( FATAL_ERROR "Please set: " ${VARIABLE_NAME} )
    ENDIF()
ENDFUNCTION()

# Verify that a path has been set
FUNCTION( VERIFY_PATH PATH_NAME )
    IF ( "${PATH_NAME}" STREQUAL "" )
        MESSAGE( FATAL_ERROR "Path is not set: ${PATH_NAME}" )
    ENDIF()
    IF ( NOT EXISTS "${PATH_NAME}" )
        MESSAGE( FATAL_ERROR "Path does not exist: ${PATH_NAME}" )
    ENDIF()
ENDFUNCTION()

# Tell CMake to use static libraries
MACRO( SET_STATIC_FLAGS )
    # Remove extra library links
    SET( CMAKE_EXE_LINK_DYNAMIC_C_FLAGS ) # remove -Wl,-Bdynamic
    SET( CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS )
    # Add the static flag if necessary
    SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static" )
    SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static " )
    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static " )
ENDMACRO()

# Set the proper warnings
MACRO( SET_WARNINGS )
    IF ( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR ( ${CMAKE_C_COMPILER_ID} MATCHES "GNU" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" ) )
        # Add gcc specific compiler options
        # Note: adding -Wlogical-op causes a wierd linking error on Titan using the nvcc wrapper:
        #    /usr/bin/ld: cannot find gical-op: No such file or directory
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat-security -Wformat-overflow=2 -Wformat-nonliteral" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat-security -Wformat-overflow=2 -Wformat-nonliteral -Woverloaded-virtual -Wsign-compare -pedantic" )
        # Note: avx512 causes changes in answers and some tests to fail, disable avx
        IF ( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" )
            SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-avx" )
            SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-avx" )
        ENDIF()
    ELSEIF ( MSVC )
        # Add Microsoft specifc compiler options
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267 /Zc:preprocessor" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267 /Zc:preprocessor" )
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "Intel" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "Intel" ) )
        # Add Intel specifc compiler options
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
        # Note: avx512 causes changes in answers and some tests to fail, disable avx
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-avx" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-avx" )
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "(CRAY|Cray)" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "(CRAY|Cray)" ) )
        # Add Cray specifc compiler options
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "PGI" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "PGI" ) )
        # Add PGI specifc compiler options
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lpthread" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -Minform=inform -Mlist --display_error_number" )
        # Suppress unreachable code warning, it causes non-useful warnings with some tests/templates
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --diag_suppress 111,128,185" )
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "(CLANG|Clang)" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "(CLANG|Clang)" ) )
        # Add CLANG specifc compiler options
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-missing-braces -Wmissing-field-initializers -ftemplate-depth=1024" )
        IF ( USE_HIP )
            SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-anonymous-struct -Wno-nested-anon-types" )
        ENDIF()
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "XL" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "XL" ) )
        # Add XL specifc compiler options
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -ftemplate-depth=512" )
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "NVHPC" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "NVHPC" ) )
        # Try adding gcc specific compiler options as a first shot
        SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat-security" )
        SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat-security -Woverloaded-virtual -Wsign-compare -pedantic" )
    ELSE()
        MESSAGE( "Compiler specific features are not set for this compiler" )
    ENDIF()
    # SET the Fortran compiler
    IF ( CMAKE_Fortran_COMPILER_WORKS )
        IF ( CMAKE_COMPILER_IS_GNUG77 OR ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "GNU" ) )
            IF ( NOT ( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR ( ${CMAKE_C_COMPILER_ID} MATCHES "GNU" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" ) ) )
                LIST( REMOVE_ITEM CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES gcc )
            ENDIF()
        ELSEIF ( (${CMAKE_Fortran_COMPILER_ID} MATCHES "Intel" ) )

        ELSEIF ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "PGI" )

        ELSEIF ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "NVHPC" )

        ELSEIF ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "(CRAY|Cray)" )

        ELSEIF ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "(CLANG|Clang|FLANG|Flang)" )

        ELSEIF ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "XL" )

        ELSE()
            MESSAGE( "CMAKE_Fortran_COMPILER=${CMAKE_Fortran_COMPILER}" )
            MESSAGE( "CMAKE_Fortran_COMPILER_ID=${CMAKE_Fortran_COMPILER_ID}" )
            MESSAGE( FATAL_ERROR "Unknown Fortran compiler ( ${CMAKE_Fortran_COMPILER_ID} )" )
        ENDIF()
    ENDIF()
ENDMACRO()

# Add user compile flags
MACRO( ADD_USER_FLAGS )
    STRING( STRIP "${CMAKE_C_FLAGS} ${CFLAGS} ${CFLAGS_EXTRA}" CMAKE_C_FLAGS )
    STRING( STRIP "${CMAKE_CXX_FLAGS} ${CXXFLAGS} ${CXXFLAGS_EXTRA}" CMAKE_CXX_FLAGS )
    STRING( STRIP "${CMAKE_Fortran_FLAGS} ${FFLAGS} ${FFLAGS_EXTRA}" CMAKE_Fortran_FLAGS )
    STRING( STRIP "${LDFLAGS} ${LDFLAGS_EXTRA}" LDFLAGS )
    STRING( STRIP "${LDLIBS} ${LDLIBS_EXTRA}" LDLIBS )
ENDMACRO()

# Set the compile/link flags
MACRO( SET_COMPILER_FLAGS )
    # Set the default flags for each build type
    IF ( MSVC )
        SET( CMAKE_C_FLAGS_DEBUG "-D_DEBUG /DEBUG /Od /EHsc /MDd /Z7" )
        SET( CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG /DEBUG /Od /EHsc /MDd /Z7" )
        SET( CMAKE_C_FLAGS_RELEASE "/O2 /EHsc /MD" )
        SET( CMAKE_CXX_FLAGS_RELEASE "/O2 /EHsc /MD" )
    ELSEIF ( (${CMAKE_C_COMPILER_ID} MATCHES "Intel" ) OR ( ${CMAKE_CXX_COMPILER_ID} MATCHES "Intel" ) )
        SET( CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fp-model precise" )
    ELSE()

    ENDIF()
    # Set debug definitions
    IF ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND NOT ( "${CMAKE_CXX_FLAGS_DEBUG}" MATCHES "-D_DEBUG" ) )
        SET( CMAKE_C_FLAGS_DEBUG " ${CMAKE_C_FLAGS_DEBUG}   -DDEBUG -D_DEBUG" )
        SET( CMAKE_CXX_FLAGS_DEBUG " ${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -D_DEBUG" )
        SET( CMAKE_CUDA_FLAGS_DEBUG " ${CMAKE_CUDA_FLAGS_DEBUG} -DDEBUG -D_DEBUG" )
        SET( CMAKE_HIP_FLAGS_DEBUG " ${CMAKE_HIP_FLAGS_DEBUG} -DDEBUG -D_DEBUG" )
    ENDIF()
    # Enable GLIBCXX_DEBUG flags
    CHECK_ENABLE_FLAG( ENABLE_GXX_DEBUG 0 )
    IF ( ENABLE_GXX_DEBUG AND NOT ( "${CMAKE_CXX_FLAGS_DEBUG}" MATCHES "-D_GLIBCXX_DEBUG" ) )
        SET( CMAKE_CXX_FLAGS_DEBUG " ${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC" )
    ENDIF()
    # Save the debug/release specific flags to the cache
    SET( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "Debug flags" FORCE )
    SET( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "Release flags" FORCE )
    SET( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "Debug flags" FORCE )
    SET( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Release flags" FORCE )
    # Add the user flags
    ADD_USER_FLAGS()
    # Set the warnings to use
    SET_WARNINGS()
    # Test the compile flags
    IF ( CMAKE_C_COMPILER_WORKS )
        CHECK_C_COMPILER_FLAG( "${CMAKE_C_FLAGS}" CHECK_C_FLAGS )
        IF ( NOT CHECK_C_FLAGS )
            MESSAGE( FATAL_ERROR "Invalid C flags detected: ${CMAKE_C_FLAGS}\n" )
        ENDIF()
    ENDIF()
    IF ( CMAKE_CXX_COMPILER_WORKS )
        CHECK_CXX_COMPILER_FLAG( "${CMAKE_CXX_FLAGS}" CHECK_CXX_FLAGS )
        IF ( NOT CHECK_CXX_FLAGS )
            MESSAGE( FATAL_ERROR "Invalid CXX flags detected: ${CMAKE_CXX_FLAGS}\n" )
        ENDIF()
    ENDIF()
ENDMACRO()

# Copy data file at build time
FUNCTION( COPY_DATA_FILE SRC_FILE DST_FILE )
    STRING( REGEX REPLACE "${${PROJ}_SOURCE_DIR}/" "" COPY_TARGET "copy-${PROJ}-${CMAKE_CURRENT_SOURCE_DIR}" )
    STRING( REGEX REPLACE "-${${PROJ}_SOURCE_DIR}" "" COPY_TARGET "${COPY_TARGET}" )
    STRING( REGEX REPLACE "/" "-" COPY_TARGET ${COPY_TARGET} )
    IF ( NOT TARGET ${COPY_TARGET} )
        ADD_CUSTOM_TARGET( ${COPY_TARGET} ALL )
        ADD_DEPENDENCIES( copy-${PROJ}-Data ${COPY_TARGET} )
    ENDIF()
    IF ( NOT SRC_FILE OR NOT DST_FILE )
        MESSAGE( FATAL_ERROR "COPY_DATA_FILE( ${SRC_FILE} ${DST_FILE} )" )
    ENDIF()
    ADD_CUSTOM_COMMAND( TARGET ${COPY_TARGET} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_FILE}" "${DST_FILE}" )
ENDFUNCTION()

# Copy a data or input file
FUNCTION( COPY_TEST_FILE FILENAME ${ARGN} )
    SET( SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}" )
    SET( SEARCH_PATHS ${SEARCH_PATHS} "${CMAKE_CURRENT_SOURCE_DIR}/data" )
    SET( SEARCH_PATHS ${SEARCH_PATHS} "${CMAKE_CURRENT_SOURCE_DIR}/inputs" )
    FOREACH( tmp ${ARGN} )
        SET( SEARCH_PATHS ${SEARCH_PATHS} "${tmp}" )
        SET( SEARCH_PATHS ${SEARCH_PATHS} "${CMAKE_CURRENT_SOURCE_DIR}/${tmp}" )
    ENDFOREACH()
    SET( FILE_TO_COPY )
    FOREACH( tmp ${SEARCH_PATHS} )
        IF ( EXISTS "${tmp}/${FILENAME}" )
            SET( FILE_TO_COPY "${tmp}/${FILENAME}" )
        ENDIF()
    ENDFOREACH()
    IF ( FILE_TO_COPY )
        SET( DESTINATION_NAME "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}" )
        COPY_DATA_FILE( ${FILE_TO_COPY} ${DESTINATION_NAME} )
    ELSE()
        SET( MSG_STR "Cannot find file: ${FILENAME}, searched:\n" )
        FOREACH( tmp ${SEARCH_PATHS} )
            SET( MSG_STR "${MSG_STR}   ${tmp}\n" )
        ENDFOREACH()
        MESSAGE( WARNING "test ${MSG_STR}" )
    ENDIF()
ENDFUNCTION()

# Copy a data file
FUNCTION( RENAME_TEST_FILE SRC_NAME DST_NAME ${ARGN} )
    SET( SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}" )
    SET( SEARCH_PATHS ${SEARCH_PATHS} "${CMAKE_CURRENT_SOURCE_DIR}/data" )
    SET( SEARCH_PATHS ${SEARCH_PATHS} "${CMAKE_CURRENT_SOURCE_DIR}/inputs" )
    FOREACH( tmp ${ARGN} )
        SET( SEARCH_PATHS ${SEARCH_PATHS} "${tmp}" )
        SET( SEARCH_PATHS ${SEARCH_PATHS} "${CMAKE_CURRENT_SOURCE_DIR}/${tmp}" )
    ENDFOREACH()
    SET( FILE_TO_COPY )
    FOREACH( tmp ${SEARCH_PATHS} )
        IF ( EXISTS "${tmp}/${SRC}" )
            SET( FILE_TO_COPY "${tmp}/${SRC_NAME}" )
        ENDIF()
    ENDFOREACH()
    IF ( FILE_TO_COPY )
        SET( DESTINATION_NAME ${CMAKE_CURRENT_BINARY_DIR}/${DST_NAME} )
        COPY_DATA_FILE( ${FILE_TO_COPY} ${DESTINATION_NAME} )
    ELSE()
        SET( MSG_STR "Cannot find file: ${SRC_NAME}, searched:\n" )
        FOREACH( tmp ${SEARCH_PATHS} )
            SET( MSG_STR "${MSG_STR}   ${tmp}\n" )
        ENDFOREACH()
        MESSAGE( WARNING "test ${MSG_STR}" )
    ENDIF()
ENDFUNCTION()

# Copy a data file
FUNCTION( COPY_EXAMPLE_DATA_FILE FILENAME )
    SET( FILE_TO_COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/${FILENAME} )
    SET( DESTINATION1 ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} )
    SET( DESTINATION2 ${EXAMPLE_INSTALL_DIR}/${FILENAME} )
    IF ( EXISTS ${FILE_TO_COPY} )
        COPY_DATA_FILE( ${FILE_TO_COPY} ${DESTINATION1} )
        COPY_DATA_FILE( ${FILE_TO_COPY} ${DESTINATION2} )
    ELSE()
        MESSAGE( WARNING "Cannot find file: " ${FILE_TO_COPY} )
    ENDIF()
ENDFUNCTION()

# Copy a mesh file
FUNCTION( COPY_MESH_FILE MESHNAME )
    # Check the local data directory
    FILE( GLOB MESHPATH "${CMAKE_CURRENT_SOURCE_DIR}/data/${MESHNAME}" )
    # Search all data paths
    FOREACH( path ${${PROJ}_DATA} )
        # Check the DATA directory
        IF ( NOT MESHPATH )
            FILE( GLOB MESHPATH "${path}/${MESHNAME}" )
        ENDIF()
        # Check the DATA/vvu directory
        IF ( NOT MESHPATH )
            FILE( GLOB MESHPATH "${path}/vvu/meshes/${MESHNAME}" )
        ENDIF()
        # Check the DATA/meshes directory
        IF ( NOT MESHPATH )
            FILE( GLOB_RECURSE MESHPATH "${path}/meshes/*/${MESHNAME}" )
        ENDIF()
        # Check the entire DATA directory
        IF ( NOT MESHPATH )
            FILE( GLOB_RECURSE MESHPATH "${path}/*/${MESHNAME}" )
        ENDIF()
    ENDFOREACH()
    # We have either found the mesh or failed
    IF ( NOT MESHPATH )
        MESSAGE( WARNING "Cannot find mesh: " ${MESHNAME} )
    ELSE()
        SET( MESHPATH2 )
        FOREACH( tmp ${MESHPATH} )
            SET( MESHPATH2 "${tmp}" )
        ENDFOREACH()
        STRING( REGEX REPLACE "//${MESHNAME}" "" MESHPATH "${MESHPATH2}" )
        STRING( REGEX REPLACE "${MESHNAME}" "" MESHPATH "${MESHPATH}" )
        COPY_DATA_FILE( "${MESHPATH}/${MESHNAME}" "${CMAKE_CURRENT_BINARY_DIR}/${MESHNAME}" )
    ENDIF()
ENDFUNCTION()

# Link the libraries to the given target
MACRO( TARGET_LINK_EXTERNAL_LIBRARIES TARGET_NAME )
    # Include external libraries
    FOREACH( tmp ${EXTERNAL_LIBS} )
        TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${ARGN} ${tmp} )
    ENDFOREACH()
    # Include libraries found through the TPL builder
    FOREACH( tmp ${TPL_LIBS} )
        TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${ARGN} ${tmp} )
    ENDFOREACH()
    # Check if we can link with Fortran libraries
    IF ( NOT DEFINED MACROS_FORTRAN_LINK_LIBRARIES )
        SET( MACROS_FORTRAN_LINK_LIBRARIES )
        IF ( CMAKE_Fortran_COMPILER AND CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES )
            INCLUDE( CheckLinkerFlag )
            CHECK_LINKER_FLAG( CXX "${CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES}" testFortranImplicit CMAKE_REQUIRED_QUIET )
            IF ( testFortranImplicit )
                SET( MACROS_FORTRAN_LINK_LIBRARIES ${CMAKE_Fortran_IMPLICIT_LINK_LIBRARIES} )
            ENDIF()
        ENDIF()
    ENDIF()
    # Include CMake implicit libraries
    IF ( NOT DISABLE_IMPLICIT_LINK )
        SET( CMAKE_IMPLICIT_LINK_LIBRARIES ${CMAKE_HIP_IMPLICIT_LINK_LIBRARIES} ${CMAKE_CUDA_IMPLICIT_LINK_LIBRARIES} ${CMAKE_C_IMPLICIT_LINK_LIBRARIES}
                                          ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES} ${MACROS_FORTRAN_LINK_LIBRARIES} )
        SET( CMAKE_IMPLICIT_LINK_DIRECTORIES ${CMAKE_HIP_IMPLICIT_LINK_DIRECTORIES} ${CMAKE_CUDA_IMPLICIT_LINK_DIRECTORIES} ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
                                            ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES} ${CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES} )
        LIST( REMOVE_DUPLICATES CMAKE_IMPLICIT_LINK_LIBRARIES )
        LIST( REMOVE_DUPLICATES CMAKE_IMPLICIT_LINK_DIRECTORIES )
        FOREACH( tmp ${CMAKE_IMPLICIT_LINK_LIBRARIES} )
            TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${ARGN} ${tmp} )
        ENDFOREACH()
        FOREACH( tmp ${CMAKE_IMPLICIT_LINK_DIRECTORIES} )
            TARGET_LINK_DIRECTORIES( ${TARGET_NAME} PUBLIC ${tmp} )
          ENDFOREACH()
    ENDIF()
ENDMACRO()

# Choose the debug or optimized library based on the build type
FUNCTION( KEEP_BUILD_LIBRARIES VAR )
    IF ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
        SET( build_type debug )
    ELSE()
        SET( build_type optimized )
    ENDIF()
    SET( build ${build_type} )
    SET( LIBS )
    FOREACH( tmp ${${VAR}} )
        IF ( (${tmp} STREQUAL debug ) OR ( ${tmp} STREQUAL optimized ) )
            SET( build ${tmp} )
        ELSEIF ( ${build} STREQUAL ${build_type} )
            SET( LIBS ${LIBS} ${tmp} )
        ENDIF()
    ENDFOREACH()
    SET( ${VAR} ${LIBS} PARENT_SCOPE )
ENDFUNCTION()

# Add the dependencies and libraries to an executable
MACRO( ADD_PROJ_EXE_DEP EXE )
    # Add the package dependencies
    IF ( ${PROJ}_TEST_LIB_EXISTS )
        ADD_DEPENDENCIES( ${EXE} ${PACKAGE_TEST_LIB} )
        TARGET_LINK_LIBRARIES( ${EXE} ${PACKAGE_TEST_LIB} )
    ENDIF()
    # Add the executable to the dependencies of check and build-test
    ADD_DEPENDENCIES( check ${EXE} )
    ADD_DEPENDENCIES( build-test ${EXE} )
    # Add the file copy targets to the dependency list
    IF ( TARGET copy-${PROJ}-Data )
        ADD_DEPENDENCIES( ${EXE} copy-${PROJ}-Data )
    ENDIF()
    # Add the project libraries
    IF ( ${PROJ}_LIB )
        TARGET_LINK_LIBRARIES( ${EXE} ${${PROJ}_LIB} )
    ELSE()
        TARGET_LINK_LIBRARIES( ${EXE} ${${PROJ}_LIBS} ${${PROJ}_LIBS} )
    ENDIF()
    TARGET_LINK_LIBRARIES( ${EXE} ${${PROJECT_NAME}_LIBRARIES} )
    # Add coverage flags to target
    IF ( NOT DISABLE_TARGET_COVERAGE )
        TARGET_COMPILE_DEFINITIONS( ${EXE} PUBLIC ${COVERAGE_FLAGS} )
    ENDIF()
    # Link to external libraries
    TARGET_LINK_LIBRARIES( ${EXE} ${LINK_LIBRARIES} )
    TARGET_LINK_EXTERNAL_LIBRARIES( ${EXE} )
    TARGET_LINK_LIBRARIES( ${EXE} ${COVERAGE_LIBS} ${LDLIBS} ${LDLIBS_EXTRA} )
    TARGET_LINK_LIBRARIES( ${EXE} ${SYSTEM_LIBS} ${SYSTEM_LDFLAGS} )
    # Set extra link flags
    IF ( USE_MPI OR USE_EXT_MPI OR HAVE_MPI )
        SET_TARGET_PROPERTIES( ${EXE} PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LDFLAGS} ${LDFLAGS_EXTRA}" )
    ELSE()
        SET_TARGET_PROPERTIES( ${EXE} PROPERTIES LINK_FLAGS "${LDFLAGS} ${LDFLAGS_EXTRA}" )
    ENDIF()
    IF ( USE_CUDA )
        SET_TARGET_PROPERTIES( ${EXE} PROPERTIES CUDA_RESOLVE_DEVICE_SYMBOLS ON )
    ENDIF()
    IF ( USE_HIP )
        SET_TARGET_PROPERTIES( ${EXE} PROPERTIES HIP_RESOLVE_DEVICE_SYMBOLS ON )
    ENDIF()
ENDMACRO()

# Check if we want to keep the test
FUNCTION( KEEP_TEST RESULT )
    SET( ${RESULT} 1 PARENT_SCOPE )
    SET_DEFAULT( ${PACKAGE_NAME}_ENABLE_TESTS 1 )
    IF ( NOT ${PACKAGE_NAME}_ENABLE_TESTS )
        SET( ${RESULT} 0 PARENT_SCOPE )
    ENDIF()
    IF ( ONLY_BUILD_DOCS )
        SET( ${RESULT} 0 PARENT_SCOPE )
    ENDIF()
ENDFUNCTION()

# Add a provisional test
FUNCTION( ADD_PROJ_PROVISIONAL_TEST EXEFILE ${ARGN} )
    # Check if we actually want to add the test
    KEEP_TEST( RESULT )
    IF ( NOT RESULT )
        RETURN()
    ENDIF()
    # Check if the target does not exist ( may not be added yet or we are re-configuring )
    IF ( NOT TARGET ${EXEFILE} )
        GLOBAL_SET( ${EXEFILE}-BINDIR )
    ENDIF()
    # Check if test has already been added
    IF ( NOT ${EXEFILE}-BINDIR )
        GLOBAL_SET( ${EXEFILE}-BINDIR "${CMAKE_CURRENT_BINARY_DIR}" )
        # The target has not been added
        FIND_FILE( NAMES "${EXEFILE}.*" NO_DEFAULT_PATH )
        FILE( GLOB SOURCES "${EXEFILE}" "${EXEFILE}.*" )
        FOREACH( tmp ${ARGN} )
            SET( SOURCES ${SOURCES} ${tmp} )
        ENDFOREACH()
        SET( TESTS_SO_FAR ${TESTS_SO_FAR} ${EXEFILE} )
        # Check if we want to add the test to all
        ADD_EXECUTABLE( ${EXEFILE} ${SOURCES} )
        IF ( EXCLUDE_TESTS_FROM_ALL AND NOT INCLUDE_${EXEFILE} )
            SET_TARGET_PROPERTIES( ${EXEFILE} PROPERTIES EXCLUDE_FROM_ALL TRUE )
        ENDIF()
        ADD_PROJ_EXE_DEP( ${EXEFILE} )
        IF ( CURRENT_LIBRARY )
            IF ( NOT TARGET ${CURRENT_LIBRARY}-test )
                ADD_CUSTOM_TARGET( ${CURRENT_LIBRARY}-test )
            ENDIF()
            ADD_DEPENDENCIES( ${CURRENT_LIBRARY}-test ${EXEFILE} )
        ENDIF()
        # If we are using CUDA and COMPILE_CXX_AS_CUDA is set, change the language
        IF ( USE_CUDA AND COMPILE_CXX_AS_CUDA )
            SET( OVERRIDE_LINKER FALSE )
            FOREACH( tmp ${SOURCES} )
                GET_SOURCE_FILE_PROPERTY( lang "${tmp}" LANGUAGE )
                IF ( lang STREQUAL "CXX" )
                    SET( OVERRIDE_LINKER TRUE )
                    SET_SOURCE_FILES_PROPERTIES( ${tmp} PROPERTIES LANGUAGE CUDA )
                ENDIF()
            ENDFOREACH()
            IF ( CUDA_SEPARABLE_COMPILATION )
                MESSAGE( STATUS "Enabling CUDA_SEPARABLE_COMPILATION for target ${EXEFILE}" )
                SET_PROPERTY( TARGET ${EXEFILE} PROPERTY CUDA_SEPARABLE_COMPILATION ON )
            ENDIF()
        ENDIF()
        # If we are using HIP and COMPILE_CXX_AS_HIP is set, change the language
        IF ( USE_HIP AND COMPILE_CXX_AS_HIP )
            SET( OVERRIDE_LINKER FALSE )
            FOREACH( tmp ${SOURCES} )
                GET_SOURCE_FILE_PROPERTY( lang "${tmp}" LANGUAGE )
                IF ( lang STREQUAL "CXX" )
                    SET( OVERRIDE_LINKER TRUE )
                    SET_SOURCE_FILES_PROPERTIES( ${tmp} PROPERTIES LANGUAGE HIP )
                ENDIF()
            ENDFOREACH()
            IF ( OVERRIDE_LINKER )
                SET_PROPERTY( TARGET ${EXEFILE} PROPERTY LINKER_LANGUAGE HIP )
            ENDIF()
        ENDIF()
    ELSEIF ( NOT ${EXEFILE-BINDIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR} )
        # We are trying to add 2 different tests with the same name
        MESSAGE( "Existing test: ${EXEFILE-BINDIR}/${EXEFILE}" )
        MESSAGE( "New test:      ${CMAKE_CURRENT_BINARY_DIR}/${EXEFILE}" )
        MESSAGE( FATAL_ERROR "Trying to add 2 different tests with the same name" )
    ENDIF()
    SET( LAST_TEST ${EXEFILE} PARENT_SCOPE )
ENDFUNCTION()
FUNCTION( ADD_${PROJ}_PROVISIONAL_TEST EXEFILE ${ARGN} )
    ADD_PROJ_PROVISIONAL_TEST( ${EXEFILE} ${ARGN} )
ENDFUNCTION()

# Parse test arguments
MACRO( PARSE_TEST_ARGUMENTS )
    # Parse the input arguments
    SET( optionalArgs
        WEEKLY
        TESTBUILDER
        GPU
        RUN_SERIAL
        EXAMPLE
        MATLAB
        NO_RESOURCES
        CATCH2 )
    SET( oneValueArgs TESTNAME PROCS THREADS TIMEOUT MAIN COST )
    SET( multiValueArgs RESOURCES LIBRARIES DEPENDS ARGS LABELS REQUIRES )
    CMAKE_PARSE_ARGUMENTS( TEST "${optionalArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
    # Set default values for threads/procs/GPUs
    SET_DEFAULT( TEST_THREADS 1 )
    SET_DEFAULT( TEST_PROCS 1 )
    SET_DEFAULT( TEST_GPU ${ASSUME_TEST_USE_GPU} )
    SET_DEFAULT( TEST_ARGS ${TEST_UNPARSED_ARGUMENTS} )
    SET_DEFAULT( NUMBER_OF_GPUS 0 )
    MATH( EXPR TOT_PROCS "${TEST_PROCS} * ${TEST_THREADS}" )
ENDMACRO()

# Create the test name
FUNCTION( CREATE_TEST_NAME EXEFILE )
    PARSE_TEST_ARGUMENTS( ${ARGN} )
    IF ( NOT DEFINED USE_NEW_TESTNAMES )
        SET( USE_NEW_TESTNAMES FALSE )
    ENDIF()
    IF ( TEST_TESTNAME )
        # User provided test name
        SET( TESTNAME ${TEST_TESTNAME} )
    ELSEIF ( NOT USE_NEW_TESTNAMES )
        # Use the old testname format
        SET( TESTNAME ${EXEFILE} )
        IF ( "${TEST_PROCS}" GREATER "1" )
            SET( TESTNAME ${TESTNAME}_${TEST_PROCS}procs )
        ENDIF()
        IF ( "${TEST_THREADS}" GREATER "1" )
            SET( TESTNAME ${TESTNAME}_${TEST_THREADS}threads )
        ENDIF()
        IF ( TEST_WEEKLY )
            SET( TESTNAME ${TESTNAME}_WEEKLY )
        ENDIF()
        IF ( TEST_EXAMPLE )
            SET( TESTNAME "example--${TESTNAME}" )
        ENDIF()
        IF ( TEST_MATLAB )
            SET( TESTNAME "MATLAB--${TESTNAME}" )
        ENDIF()
        IF ( PACKAGE )
            SET( TESTNAME "${PACKAGE}::${TESTNAME}" )
        ENDIF()
        FOREACH( tmp ${TEST_ARGS} )
            SET( TESTNAME "${TESTNAME}--${tmp}" )
        ENDFOREACH()
    ELSE()
        # Use the new test name format
        STRING( REPLACE ";" " " TESTNAME "${EXEFILE} ${TEST_ARGS}" )
        SET( ALTERATIONS )
        IF ( ${TEST_PROCS} GREATER "1" )
            SET( ALTERATIONS "${ALTERATIONS}, ${TEST_PROCS} procs" )
        ENDIF()
        IF ( ${TEST_THREADS} GREATER "1" )
            SET( ALTERATIONS "${ALTERATIONS}, ${TEST_THREADS} threads" )
        ENDIF()
        IF ( TEST_WEEKLY )
            SET( ALTERATIONS "${ALTERATIONS}, WEEKLY" )
        ENDIF()
        IF ( TEST_EXAMPLE )
            SET( ALTERATIONS "${ALTERATIONS}, EXAMPLE" )
        ENDIF()
        IF ( TEST_MATLAB )
            SET( ALTERATIONS "${ALTERATIONS}, MATLAB" )
        ENDIF()
        IF ( ALTERATIONS )
            STRING( REGEX REPLACE "^, " "" ALTERATIONS "${ALTERATIONS}" )
            SET( TESTNAME "${TESTNAME} ( ${ALTERATIONS} )" )
        ENDIF()
        STRING( REPLACE "  " " " TESTNAME "${TESTNAME}" )
    ENDIF()
    SET( TESTNAME "${TESTNAME}" PARENT_SCOPE )
ENDFUNCTION()

# Function to add a test
# Call should be of the form:
#    CALL_ADD_TEST( exefile
#                   TESTNAME testname
#                   THREADS #
#                   PROCS #
#                   WEEKLY
#                   TESTBUILDER
#                   RESOURCES resources
#                   GPU
#                   TIMEOUT seconds
#                   COST seconds
#                   RUN_SERIAL
#                   DEPENDS depends
#                   ARGS arguments )
FUNCTION( CALL_ADD_TEST EXEFILE )

    # Check if we actually want to add the test
    KEEP_TEST( RESULT )
    IF ( NOT RESULT )
        MESSAGE( "Discarding test: ${EXEFILE}" )
        RETURN()
    ENDIF()

    # Create the testname
    CREATE_TEST_NAME( ${EXEFILE} ${ARGN} )
    SET( LAST_TESTNAME "${TESTNAME}" PARENT_SCOPE )

    # Parse the input arguments
    PARSE_TEST_ARGUMENTS( ${ARGN} )

    # Check dependencies
    FOREACH( tmp ${TEST_REQUIRES} )
        IF ( NOT ${tmp} )
            MESSAGE( "Disabling \"${TESTNAME}\" because ${tmp} was required and not found" )
            RETURN()
        ENDIF()
    ENDFOREACH()

    # Add example to list
    IF ( TEST_EXAMPLE )
        SET( VALUE 0 )
        FOREACH( _variableName ${EXAMPLE_LIST} )
            IF ( "${_variableName}" STREQUAL "${EXEFILE}" )
                SET( VALUE 1 )
            ENDIF()
        ENDFOREACH()
        IF ( NOT ${VALUE} )
            FILE( APPEND ${EXAMPLE_INSTALL_DIR}/examples.h "* \\ref ${EXEFILE} \"${EXEFILE}\"\n" )
            SET( EXAMPLE_LIST ${EXAMPLE_LIST} ${EXEFILE} CACHE INTERNAL "example_list" FORCE )
        ENDIF()
    ENDIF()

    # Check if we are dealing with a TestBuilder test
    IF ( TEST_TESTBUILDER )
        # Add the test to the TestBuilder sources
        SET( TESTBUILDER_SOURCES ${TESTBUILDER_SOURCES} ${EXEFILE} PARENT_SCOPE )
    ELSE()
        # Add the provisional test
        ADD_PROJ_PROVISIONAL_TEST( ${EXEFILE} ${TEST_MAIN} )
    ENDIF()

    # Add catch2 link if needed
    IF ( TEST_CATCH2 )
        IF ( TEST_MAIN )
            TARGET_LINK_LIBRARIES( ${EXEFILE} Catch2::Catch2 )
        ELSE()
            TARGET_LINK_LIBRARIES( ${EXEFILE} Catch2::Catch2WithMain )
        ENDIF()
    ENDIF()

    # ADD additional link libraries
    FOREACH( tmp ${TEST_LIBRARIES} )
        TARGET_LINK_LIBRARIES( ${EXEFILE} ${tmp} )
    ENDFOREACH()

    # Copy example
    IF ( TEST_EXAMPLE )
        ADD_DEPENDENCIES( build-examples ${EXEFILE} )
        ADD_CUSTOM_COMMAND( TARGET ${EXEFILE} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${LAST_TEST}> "${EXAMPLE_INSTALL_DIR}/${EXEFILE}" )
    ENDIF()

    # Create the tests for ctest
    IF ( ${TOT_PROCS} EQUAL 0 )
        # Skip test ( provisional )
        RETURN()
    ELSEIF ( USE_CUDA AND TEST_GPU AND ( ${TEST_PROCS} GREATER ${NUMBER_OF_GPUS} ) )
        MESSAGE( "Disabling test \"${TESTNAME}\" ( exceeds maximum number of GPUs available ${NUMBER_OF_GPUS} )" )
        RETURN()
    ELSEIF ( USE_HIP AND TEST_GPU AND ( ${TEST_PROCS} GREATER ${NUMBER_OF_GPUS} ) )
        MESSAGE( "Disabling test \"${TESTNAME}\" ( exceeds maximum number of GPUs available ${NUMBER_OF_GPUS} )" )
        RETURN()
    ELSEIF ( ${TOT_PROCS} GREATER ${TEST_MAX_PROCS} )
        MESSAGE( "Disabling test \"${TESTNAME}\" ( exceeds maximum number of processors ${TEST_MAX_PROCS} )" )
        RETURN()
    ELSEIF ( (${TEST_PROCS} STREQUAL "1" ) AND NOT USE_MPI_FOR_SERIAL_TESTS )
        IF ( TEST_TESTBUILDER )
            ADD_TEST( NAME ${TESTNAME} COMMAND $<TARGET_FILE:${TB_TARGET}> ${EXEFILE} ${TEST_ARGS} )
        ELSE()
            ADD_TEST( NAME ${TESTNAME} COMMAND $<TARGET_FILE:${LAST_TEST}> ${TEST_ARGS} )
        ENDIF()
    ELSEIF ( USE_MPI OR USE_EXT_MPI )
        IF ( TEST_TESTBUILDER )
            ADD_TEST( NAME ${TESTNAME} COMMAND ${MPIEXEC} ${MPIEXEC_FLAGS} ${MPIEXEC_NUMPROC_FLAG} ${TEST_PROCS} $<TARGET_FILE:${TB_TARGET}> ${EXEFILE} ${TEST_ARGS} )
        ELSE()
            ADD_TEST( NAME ${TESTNAME} COMMAND ${MPIEXEC} ${MPIEXEC_FLAGS} ${MPIEXEC_NUMPROC_FLAG} ${TEST_PROCS} $<TARGET_FILE:${LAST_TEST}> ${TEST_ARGS} )
        ENDIF()
        SET_PROPERTY( TEST ${TESTNAME} APPEND PROPERTY ENVIRONMENT "OMPI_MCA_hwloc_base_binding_policy=none" )
    ELSE()
        # Disable test
        RETURN()
    ENDIF()
    SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES FAIL_REGULAR_EXPRESSION "${TEST_FAIL_REGULAR_EXPRESSION}" PROCESSORS ${TOT_PROCS} )
    SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES ENVIRONMENT "OMP_NUM_THREADS=${TEST_THREADS};OMP_WAIT_POLICY=passive" )

    # Add labels
    IF ( TEST_LABELS )
        SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES LABELS ${TEST_LABELS} )
    ENDIF()

    # Add resource locks
    IF ( NOT TEST_NO_RESOURCES )
        IF ( NOT TEST_RESOURCES )
            SET( TEST_RESOURCES ${EXEFILE} ${TEST_ARGS} )
        ENDIF()
        SET_PROPERTY( TEST ${TESTNAME} PROPERTY RESOURCE_LOCK ${TEST_RESOURCES} )
    ENDIF()

    # Add resource locks for GPUs
    IF ( USE_CUDA AND TEST_GPU )
        SET( GPU_RESOURCE )
        FOREACH( tmp RANGE 1 ${TEST_PROCS} )
            SET( GPU_RESOURCE "${GPU_RESOURCE},gpus:1" )
        ENDFOREACH()
        STRING( REGEX REPLACE "^," "" GPU_RESOURCE ${GPU_RESOURCE} )
        SET_PROPERTY( TEST ${TESTNAME} PROPERTY RESOURCE_GROUPS ${GPU_RESOURCE} )
    ENDIF()

    # Add resource locks for GPUs
    IF ( USE_HIP AND TEST_GPU )
        SET( GPU_RESOURCE )
        FOREACH( tmp RANGE 1 ${TEST_PROCS} )
            SET( GPU_RESOURCE "${GPU_RESOURCE},gpus:1" )
        ENDFOREACH()
        STRING( REGEX REPLACE "^," "" GPU_RESOURCE ${GPU_RESOURCE} )
        SET_PROPERTY( TEST ${TESTNAME} PROPERTY RESOURCE_GROUPS ${GPU_RESOURCE} )
    ENDIF()

    # Add timeout
    IF ( TEST_TIMEOUT )
        SET_PROPERTY( TEST ${TESTNAME} PROPERTY TIMEOUT ${TEST_TIMEOUT} )
    ENDIF()

    # Add cost data
    IF ( TEST_COST )
        SET_PROPERTY( TEST ${TESTNAME} PROPERTY COST ${TEST_COST} )
    ENDIF()

    # Run the test by itself
    IF ( TEST_RUN_SERIAL )
        SET_PROPERTY( TEST ${TESTNAME} PROPERTY RUN_SERIAL TRUE )
    ENDIF()

    # Add dependencies
    IF ( TEST_DEPENDS )
        SET_PROPERTY( TEST ${TESTNAME} APPEND PROPERTY DEPENDS ${TEST_DEPENDS} )
    ENDIF()

    # Print the call and compiled line for debugging purposes
    # MESSAGE( "CALL_ADD_TEST( ${EXEFILE} TESTNAME ${TESTNAME} PROCS ${TOT_PROCS} WEEKLY ${WEEKLY} TESTBUILDER ${TESTBUILDER} RESOURCES ${TEST_RESOURCES} GPU ${GPU} DEPENDS ${TESTDEPENDS} ARGS ${ARGS}" )
ENDFUNCTION()

# Functions to create a test builder
FUNCTION( INITIALIZE_TESTBUILDER )
    SET( TESTBUILDER_SOURCES PARENT_SCOPE )
    # Check if we actually want to add the test
    KEEP_TEST( RESULT )
    IF ( NOT RESULT )
        RETURN()
    ENDIF()
    # Check if the target does not exist ( may not be added yet or we are re-configuring )
    STRING( REGEX REPLACE "${${PROJ}_SOURCE_DIR}/" "" TB_TARGET "TB-${PROJ}-${CMAKE_CURRENT_SOURCE_DIR}" )
    STRING( REGEX REPLACE "/" "-" TB_TARGET ${TB_TARGET} )
    IF ( NOT TARGET ${TB_TARGET} )
        GLOBAL_SET( ${TB_TARGET}-BINDIR )
    ENDIF()
    # Check if test has already been added
    IF ( NOT ${TB_TARGET}-BINDIR )
        GLOBAL_SET( ${TB_TARGET}-BINDIR "${CMAKE_CURRENT_BINARY_DIR}" )
        # Create the initial file
        IF ( NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder.cpp" )
            FILE( WRITE "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder.cpp" "// Auto generated file\n\n" )
        ENDIF()
        # The target has not been added
        SET( CXXFILE "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder.cpp" )
        SET( TESTS_SO_FAR ${TESTS_SO_FAR} ${TB_TARGET} )
        # Check if we want to add the test to all
        ADD_EXECUTABLE( ${TB_TARGET} ${CXXFILE} )
        IF ( EXCLUDE_TESTS_FROM_ALL )
            SET_TARGET_PROPERTIES( ${TB_TARGET} PROPERTIES EXCLUDE_FROM_ALL TRUE )
        ENDIF()
        SET_TARGET_PROPERTIES( ${TB_TARGET} PROPERTIES OUTPUT_NAME "TB" )
    ELSEIF ( NOT ${TB-BINDIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR} )
        # We are trying to add 2 different tests with the same name
        MESSAGE( "Existing test: ${TB-BINDIR}/${TB_TARGET}" )
        MESSAGE( "New test:      ${CMAKE_CURRENT_BINARY_DIR}/${TB_TARGET}}" )
        MESSAGE( FATAL_ERROR "Trying to add 2 different tests with the same name" )
    ENDIF()
    SET( TB_TARGET ${TB_TARGET} PARENT_SCOPE )
ENDFUNCTION()
FUNCTION( FINALIZE_TESTBUILDER )
    # Check if we actually want to add the test
    KEEP_TEST( RESULT )
    IF ( NOT RESULT )
        RETURN()
    ENDIF()
    # Create the library for the test builder
    IF ( TESTBUILDER_SOURCES )
        LIST( REMOVE_DUPLICATES TESTBUILDER_SOURCES )
        SET( TB_TARGET_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder.cpp" )
        FOREACH( tmp ${TESTBUILDER_SOURCES} )
            FIND_FILE( NAMES "${tmp}.*" NO_DEFAULT_PATH )
            FILE( GLOB SOURCES "${tmp}" "${tmp}.*" )
            SET( TB_TARGET_SOURCES ${TB_TARGET_SOURCES} ${SOURCES} )
        ENDFOREACH()
        TARGET_SOURCES( ${TB_TARGET} PUBLIC ${TB_TARGET_SOURCES} )
        ADD_DEPENDENCIES( ${TB_TARGET} copy-${PROJ}-include )
    ELSE()
        SET( TB_TARGET_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder.cpp" )
    ENDIF()
    # Add the dependencies to the test builder
    ADD_PROJ_EXE_DEP( ${TB_TARGET} )
    IF ( CURRENT_LIBRARY )
        IF ( NOT TARGET ${CURRENT_LIBRARY}-test )
            ADD_CUSTOM_TARGET( ${CURRENT_LIBRARY}-test )
        ENDIF()
        ADD_DEPENDENCIES( ${CURRENT_LIBRARY}-test ${TB_TARGET} )
    ENDIF()
    SET( TB_TARGET ${TB_TARGET} PARENT_SCOPE )
    # If we are using CUDA and COMPILE_CXX_AS_CUDA is set, change the language
    IF ( USE_CUDA AND COMPILE_CXX_AS_CUDA )
        FOREACH( tmp ${TB_TARGET_SOURCES} )
            GET_SOURCE_FILE_PROPERTY( lang "${tmp}" LANGUAGE )
            IF ( lang STREQUAL "CXX" )
                SET_SOURCE_FILES_PROPERTIES( ${tmp} PROPERTIES LANGUAGE CUDA )
            ENDIF()
        ENDFOREACH()
    ENDIF()
    # If we are using HIP and COMPILE_CXX_AS_HIP is set, change the language
    IF ( USE_HIP AND COMPILE_CXX_AS_HIP )
        FOREACH( tmp ${TB_TARGET_SOURCES} )
            GET_SOURCE_FILE_PROPERTY( lang "${tmp}" LANGUAGE )
            IF ( lang STREQUAL "CXX" OR lang STREQUAL "NOTFOUND" )
                SET_SOURCE_FILES_PROPERTIES( ${tmp} PROPERTIES LANGUAGE HIP )
            ENDIF()
        ENDFOREACH()
        SET_TARGET_PROPERTIES( ${TB_TARGET} PROPERTIES LINKER_LANGUAGE HIP )
    ENDIF()
    # Sort the sources
    LIST( SORT TESTBUILDER_SOURCES )
    # Finish generating TestBuilder.cpp
    SET( TB_FILE "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder-tmp.cpp" )
    FILE( WRITE "${TB_FILE}" "// Auto generated file\n\n" )
    FILE( APPEND "${TB_FILE}" "#include <cstring>\n" )
    FILE( APPEND "${TB_FILE}" "#include <iostream>\n\n" )
    FOREACH( tmp ${TESTBUILDER_SOURCES} )
        FILE( APPEND "${TB_FILE}" "extern int ${tmp}( int argc, char *argv[] );\n" )
    ENDFOREACH()
    FILE( APPEND "${TB_FILE}" "\n\n// Main Program\n" )
    FILE( APPEND "${TB_FILE}" "int main( int argc, char *argv[] )\n" )
    FILE( APPEND "${TB_FILE}" "{\n" )
    FILE( APPEND "${TB_FILE}" "    if ( argc < 2 ) {\n" )
    FILE( APPEND "${TB_FILE}" "        std::cerr << \"Invalid number of arguments for TestBuilder\\n\";\n" )
    FILE( APPEND "${TB_FILE}" "        std::cerr << \"Supported tests:\\n\";\n" )
    FOREACH( tmp ${TESTBUILDER_SOURCES} )
        FILE( APPEND "${TB_FILE}" "        std::cerr << \"   ${tmp}\\n\";\n" )
    ENDFOREACH()
    FILE( APPEND "${TB_FILE}" "        return 1;\n" )
    FILE( APPEND "${TB_FILE}" "    }\n\n" )
    FILE( APPEND "${TB_FILE}" "    int rtn = 0;\n" )
    FILE( APPEND "${TB_FILE}" "    if ( strcmp( argv[1], \"NULL\" ) == 0 ) {\n" )
    FOREACH( tmp ${TESTBUILDER_SOURCES} )
        FILE( APPEND "${TB_FILE}" "    } else if ( strcmp( argv[1], \"${tmp}\" ) == 0 ) {\n" )
        FILE( APPEND "${TB_FILE}" "        rtn = ${tmp}( argc-1, &argv[1] );\n" )
    ENDFOREACH()
    FILE( APPEND "${TB_FILE}" "    } else {\n" )
    FILE( APPEND "${TB_FILE}" "        std::cerr << \"Unknown test: \" << argv[1] << std::endl;\n" )
    FILE( APPEND "${TB_FILE}" "        return 1;\n" )
    FILE( APPEND "${TB_FILE}" "    }\n\n" )
    FILE( APPEND "${TB_FILE}" "    return rtn;\n" )
    FILE( APPEND "${TB_FILE}" "}\n" )
    EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TB_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/TestBuilder.cpp" )
ENDFUNCTION()

# Convenience functions to add a test
# Note: These should be macros so that LAST_TESTNAME is set properly
MACRO( ADD_${PROJ}_TEST EXEFILE ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} ${ARGN} )
ENDMACRO()
MACRO( ADD_${PROJ}_TEST_1_2_4 EXEFILE ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 1 ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 2 ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 4 ${ARGN} )
ENDMACRO()
MACRO( ADD_TB_PROVISIONAL_TEST EXEFILE )
    SET( TESTBUILDER_SOURCES ${TESTBUILDER_SOURCES} ${EXEFILE} )
ENDMACRO()
MACRO( ADD_TB_TEST EXEFILE ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 1 TESTBUILDER ${ARGN} )
ENDMACRO()
MACRO( ADD_TB_TEST_1_2_4 EXEFILE ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 1 TESTBUILDER ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 2 TESTBUILDER ${ARGN} )
    CALL_ADD_TEST( ${EXEFILE} PROCS 4 TESTBUILDER ${ARGN} )
ENDMACRO()

# Add a executable as an example
MACRO( ADD_${PROJ}_EXAMPLE EXEFILE )
    CALL_ADD_TEST( ${EXEFILE} EXAMPLE ${ARGN} )
ENDMACRO()

# Begin configure for the examples for a package
MACRO( BEGIN_EXAMPLE_CONFIG PACKAGE )
    # Set example install dir
    SET( EXAMPLE_INSTALL_DIR ${${PROJ}_INSTALL_DIR}/examples/${PACKAGE} )
    # Create list of examples
    SET( EXAMPLE_LIST "dummy" CACHE INTERNAL "example_list" FORCE )
    # Create doxygen input file for examples
    SET( DOXYFILE_EXTRA_SOURCES ${DOXYFILE_EXTRA_SOURCES} ${EXAMPLE_INSTALL_DIR} CACHE INTERNAL "doxyfile_extra_sources" )
    FILE( WRITE ${EXAMPLE_INSTALL_DIR}/examples.h "// Include file for doxygen providing the examples for ${PACKAGE}\n" )
    FILE( APPEND ${EXAMPLE_INSTALL_DIR}/examples.h "/*! \\page Examples_${PACKAGE}\n" )
ENDMACRO()

# Install the examples
MACRO( INSTALL_${PROJ}_EXAMPLE PACKAGE )
    FILE( APPEND ${EXAMPLE_INSTALL_DIR}/examples.h "*/\n" )
    SET( EXAMPLE_INSTALL_DIR "" )
ENDMACRO()

# Add a latex file to the build
FUNCTION( ADD_LATEX_DOCS FILE )
    GET_FILENAME_COMPONENT( LATEX_TARGET ${FILE} NAME_WE )
    ADD_CUSTOM_TARGET(
        ${LATEX_TARGET}_pdf
        ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${CMAKE_CURRENT_BINARY_DIR}/.
        COMMAND pdflatex -interaction=batchmode -draftmode ${FILE} ";" echo ""
        COMMAND bibtex -terse ${LATEX_TARGET} ";" echo ""
        COMMAND pdflatex -interaction=batchmode ${FILE} ";" echo ""
        SOURCES ${FILE} )
    ADD_CUSTOM_COMMAND( TARGET ${LATEX_TARGET}_pdf POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${LATEX_TARGET}.pdf ${${PROJ}_INSTALL_DIR}/doc/.
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
    ADD_DEPENDENCIES( latex_docs ${LATEX_TARGET}_pdf )
ENDFUNCTION()

# Add a matlab mex file
FUNCTION( ADD_MATLAB_MEX SOURCE )
    STRING( REGEX REPLACE "[.]cpp" "" TARGET ${SOURCE} )
    STRING( REGEX REPLACE "[.]c" "" TARGET ${TARGET} )
    MATLAB_ADD_MEX( NAME ${TARGET} SRC ${SOURCE} MODULE R2017b )
    TARGET_LINK_LIBRARIES( ${TARGET} ${MATLAB_TARGET} )
    ADD_PROJ_EXE_DEP( ${TARGET} )
    ADD_DEPENDENCIES( mex ${TARGET} )
    INSTALL( TARGETS ${TARGET} DESTINATION ${${PROJ}_INSTALL_DIR}/mex )
    ADD_DEPENDENCIES( mex ${TARGET} )
    SET( MEX_FILES2 ${MEX_FILES} "${${PROJ}_INSTALL_DIR}/mex/${TARGET}.${Matlab_MEX_EXTENSION}" )
    LIST( REMOVE_DUPLICATES MEX_FILES2 )
    SET( MEX_FILES ${MEX_FILES2} CACHE INTERNAL "" )
ENDFUNCTION()

# Add a matlab test
FUNCTION( ADD_MATLAB_TEST EXEFILE ${ARGN} )
    IF ( NOT MATLAB_EXE )
        MESSAGE( FATAL_ERROR "MATLAB_EXE not set, did you call CREATE_MATLAB_WRAPPER()" )
    ENDIF()
    CONFIGURE_FILE( ${EXEFILE}.m ${CMAKE_CURRENT_BINARY_DIR}/${EXEFILE}.m )
    CREATE_TEST_NAME( ${EXEFILE} MATLAB ${ARGN} )
    SET( LAST_TESTNAME ${TESTNAME} PARENT_SCOPE )
    IF ( MSVC )
        SET( MATLAB_OPTIONS "-logfile" "log_${EXEFILE}" )
    ENDIF()
    SET( MATLAB_COMMAND "try, ${EXEFILE}, catch ME, disp( getReport( ME ) ), clear all global, exit( 1 ), end, disp( 'ALL TESTS PASSED' ); exit( 0 )" )
    SET( MATLAB_DEBUGGER_OPTIONS )
    IF ( MATLAB_DEBUGGER )
        SET( MATLAB_DEBUGGER_OPTIONS -D${MATLAB_DEBUGGER} )
        STRING( REPLACE "\"" "" MATLAB_DEBUGGER_OPTIONS "${MATLAB_DEBUGGER_OPTIONS}" )
    ENDIF()
    ADD_TEST( NAME ${TESTNAME} COMMAND "${MATLAB_EXE}" ${MATLAB_OPTIONS} -r "${MATLAB_COMMAND}" ${MATLAB_DEBUGGER_OPTIONS} )
    SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" FAIL_REGULAR_EXPRESSION "FAILED" )
    SET_PROPERTY( TEST ${TESTNAME} APPEND PROPERTY ENVIRONMENT RATES_DIRECTORY=${RATES_DIRECTORY} )
ENDFUNCTION()

# Create a script to start matlab preloading libraries
FUNCTION( CREATE_MATLAB_WRAPPER )
    SET( MATLABPATH ${MATLABPATH} "${${PROJ}_INSTALL_DIR}/mex" )
    SET( tmp_libs ${MEX_LIBCXX} ${MEX_FILES} )
    LIST( REMOVE_DUPLICATES MATLABPATH )
    STRING( REGEX REPLACE ";" ":" tmp_libs "${tmp_libs}" )
    STRING( REGEX REPLACE ";" ":" tmp_path "${MATLABPATH}" )
    IF ( MSVC )
        # Create a matlab wrapper for windows
        SET( MATLAB_GUI "${CMAKE_CURRENT_BINARY_DIR}/tmp/matlab-gui.bat" )
        SET( MATLAB_CMD "${CMAKE_CURRENT_BINARY_DIR}/tmp/matlab-cmd.bat" )
        SET( MATLAB_INSTALL_CMD "matlab-cmd.bat" )
        FILE( WRITE "${MATLAB_GUI}" "@echo off\n" )
        FILE( WRITE "${MATLAB_CMD}" "@echo off\n" )
        FILE( APPEND "${MATLAB_GUI}" "matlab  -singleCompThread -nosplash %*\n" )
        FILE( APPEND "${MATLAB_CMD}" "matlab  -singleCompThread -nosplash -nodisplay -nodesktop -wait %*\n" )
        FILE( WRITE "${MATLAB_GUI}" "@echo on\n" )
        FILE( WRITE "$${MATLAB_CMD}" "@echo on\n" )
    ELSE()
        # Create a matlab wrapper for linux/mac
        SET( MATLAB_GUI "${CMAKE_CURRENT_BINARY_DIR}/tmp/matlab-gui" )
        SET( MATLAB_CMD "${CMAKE_CURRENT_BINARY_DIR}/tmp/matlab-cmd" )
        SET( MATLAB_INSTALL_CMD "matlab-cmd" )
        FILE( WRITE "${MATLAB_GUI}" "LD_PRELOAD=\"${tmp_libs}\" MKL_NUM_THREADS=1 MATLABPATH=\"${tmp_path}\" \"${Matlab_MAIN_PROGRAM}\" -singleCompThread -nosplash \"$@\"\n" )
        FILE( WRITE "${MATLAB_CMD}"
             "LD_PRELOAD=\"${tmp_libs}\" MKL_NUM_THREADS=1 MATLABPATH=\"${tmp_path}\" \"${Matlab_MAIN_PROGRAM}\" -singleCompThread -nosplash -nodisplay -nojvm \"$@\"\n" )
    ENDIF()
    FILE(
        COPY "${MATLAB_GUI}"
        DESTINATION "${${PROJ}_INSTALL_DIR}/mex"
        FILE_PERMISSIONS
            OWNER_READ
            OWNER_WRITE
            OWNER_EXECUTE
            GROUP_READ
            GROUP_EXECUTE
            WORLD_READ
            WORLD_EXECUTE )
    FILE(
        COPY "${MATLAB_CMD}"
        DESTINATION "${${PROJ}_INSTALL_DIR}/mex"
        FILE_PERMISSIONS
            OWNER_READ
            OWNER_WRITE
            OWNER_EXECUTE
            GROUP_READ
            GROUP_EXECUTE
            WORLD_READ
            WORLD_EXECUTE )
    SET( MATLAB_EXE "${${PROJ}_INSTALL_DIR}/mex/${MATLAB_INSTALL_CMD}" CACHE INTERNAL "" )
ENDFUNCTION()

# Append a list to a file
FUNCTION( APPEND_LIST FILENAME VARS PREFIX POSTFIX )
    FOREACH( tmp ${VARS} )
        FILE( APPEND "${FILENAME}" "${PREFIX}" )
        FILE( APPEND "${FILENAME}" "${tmp}" )
        FILE( APPEND "${FILENAME}" "${POSTFIX}" )
    ENDFOREACH()
ENDFUNCTION()

# add custom target distclean
# cleans and removes cmake generated files etc.
FUNCTION( ADD_DISTCLEAN ${ARGN} )
    SET( DISTCLEANED
        assembly
        cmake.depends
        cmake.check_depends
        CMakeCache.txt
        CMakeFiles
        CMakeTmp
        CMakeDoxy*
        cmake.check_cache
        *.cmake
        compile.log
        cppcheck-build
        Doxyfile
        Makefile
        core
        core.*
        DartConfiguration.tcl
        install_manifest.txt
        Testing
        include
        doc
        docs
        examples
        latex_docs
        lib
        Makefile.config
        install_manifest.txt
        test
        matlab
        Matlab
        mex
        tmp
        bin
        cmake
        cppclean
        compile_commands.json
        TPLs.h
        ${ARGN} )
    IF ( UNIX )
        ADD_CUSTOM_TARGET( distclean COMMENT "distribution clean" COMMAND rm -Rf ${DISTCLEANED} WORKING_DIRECTORY "${CMAKE_CURRENT_BUILD_DIR}" )
    ELSE()
        SET( DISTCLEANED ${DISTCLEANED} *.vcxproj* ipch x64 Debug )
        SET( DISTCLEAN_FILE "${CMAKE_CURRENT_BINARY_DIR}/distclean.bat" )
        FILE( WRITE "${DISTCLEAN_FILE}" "del /s /q /f " )
        APPEND_LIST( "${DISTCLEAN_FILE}" "${DISTCLEANED}" " " " " )
        FILE( APPEND "${DISTCLEAN_FILE}" "\n" )
        APPEND_LIST( "${DISTCLEAN_FILE}" "${DISTCLEANED}" "for /d %%x in ( " " ) do rd /s /q \"%%x\"\n" )
        ADD_CUSTOM_TARGET( distclean COMMENT "distribution clean" COMMAND distclean.bat & del /s/q/f distclean.bat WORKING_DIRECTORY "${CMAKE_CURRENT_BUILD_DIR}" )
    ENDIF()
ENDFUNCTION()

# add custom target mex_clean
FUNCTION( ADD_MEXCLEAN )
    IF ( UNIX )
        ADD_CUSTOM_TARGET( mexclean COMMENT "mex clean" COMMAND rm ARGS -Rf libmatlab.* *.mex* test/*.mex* )
    ENDIF( UNIX )
ENDFUNCTION()

# Print the current repo version and create target to write to a file
SET( WriteRepoVersionCmakeFile "${CMAKE_CURRENT_LIST_DIR}/WriteRepoVersion.cmake" )
FUNCTION( WRITE_REPO_VERSION FILENAME )
    SET( CMD
        ${CMAKE_COMMAND}
        -Dfilename="${FILENAME}"
        -Dsrc_dir="${${PROJ}_SOURCE_DIR}"
        -Dtmp_file="${CMAKE_CURRENT_BINARY_DIR}/tmp/version.h"
        -DPROJ=${PROJ}
        -P
        "${WriteRepoVersionCmakeFile}" )
    EXECUTE_PROCESS( COMMAND ${CMD} )
    ADD_CUSTOM_TARGET( write_repo_version COMMENT "Write repo version" COMMAND ${CMD} )
ENDFUNCTION()

# Check if we can include a python module
FUNCTION( FIND_PYTHON_MODULE MODULE )
    STRING( TOUPPER ${MODULE} MODULE2 )
    SET( PY_${MODULE}_FOUND FALSE )
    IF ( NOT PY_${MODULE2} )
        IF ( ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED" )
            SET( PY_${MODULE2}_FIND_REQUIRED TRUE )
        ENDIF()
        SET( CMD "import ${MODULE}; print( 'Success' )" )
        EXECUTE_PROCESS( COMMAND "${PYTHON_EXECUTABLE}" "-c" "${CMD}" RESULT_VARIABLE _${MODULE2}_status OUTPUT_VARIABLE _${MODULE2}_output ERROR_QUIET
                                                                                                                                           OUTPUT_STRIP_TRAILING_WHITESPACE )
        IF ( "${_${MODULE2}_output}" STREQUAL "Success" )
            SET( PY_${MODULE2}_FOUND TRUE )
        ENDIF()
    ELSE()
        SET( PY_${MODULE2}_FOUND TRUE )
    ENDIF()
    IF ( PY_${MODULE2}_FOUND )
        MESSAGE( STATUS "Performing Test PYTHON_${MODULE2} - Success" )
    ELSE()
        MESSAGE( STATUS "Performing Test PYTHON_${MODULE2} - Failure" )
    ENDIF()
    IF ( NOT PY_${MODULE2}_FOUND AND PY_${MODULE2}_FIND_REQUIRED )
        MESSAGE( FATAL_ERROR "Unable to find required python module: ${MODULE2}" )
    ENDIF()
    SET( PY_${MODULE2}_FOUND ${PY_${MODULE2}_FOUND} PARENT_SCOPE )
ENDFUNCTION( FIND_PYTHON_MODULE )
