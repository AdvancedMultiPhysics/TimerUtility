# Check the inputs
SET( PROJ     ${PROJF}    )
SET( src_dir  ${src_dir}  )
SET( tmp_file ${tmp_file} )
SET( filename ${filename} )
STRING(REGEX REPLACE "\"" "" PROJ     "${PROJ}"     )
STRING(REGEX REPLACE "\"" "" src_dir  "${src_dir}"  )
STRING(REGEX REPLACE "\"" "" tmp_file "${tmp_file}" )
STRING(REGEX REPLACE "\"" "" filename "${filename}" )
IF ( NOT PROJ )
    MESSAGE( FATAL_ERROR "PROJ is not set")
ENDIF()
IF ( NOT IS_DIRECTORY "${src_dir}" )
    MESSAGE( FATAL_ERROR "Repo path ${src_dir} does not exist")
ENDIF()
IF ( NOT tmp_file )
    MESSAGE( FATAL_ERROR "Temporary file is not set")
ENDIF()
IF ( NOT filename )
    MESSAGE( FATAL_ERROR "Output file is not set")
ENDIF()

# Get the repo version
EXECUTE_PROCESS( COMMAND hg id -i  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE VERSION_OUT )
STRING(REGEX REPLACE "(\r?\n)+$" "" VERSION_OUT "${VERSION_OUT}")

# Write the results to the file
FILE(WRITE  "${tmp_file}" "#define ${PROJ}_VERSION \"${VERSION_OUT}\"\n" )
EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tmp_file}" "${filename}" )
MESSAGE("${PROJ} Version = ${VERSION_OUT}")

