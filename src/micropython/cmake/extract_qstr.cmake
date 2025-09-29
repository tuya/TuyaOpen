##
# @file extract_qstr.cmake
# @brief CMake script to extract QSTR from source files
# This script is called by add_custom_command to extract MP_QSTR_* patterns
#/

# Input variables:
# SOURCE_FILES - Semicolon-separated list of source files
# OUTPUT_FILE - Output file for extracted QSTRs

# Handle both space-separated and semicolon-separated lists
# First replace spaces with semicolons if present
string(REPLACE " " ";" SOURCE_FILES_TEMP "${SOURCE_FILES}")
# Then ensure it's a proper CMake list
set(SOURCE_LIST ${SOURCE_FILES_TEMP})

# Clear output file
file(WRITE ${OUTPUT_FILE} "")

# Process each source file
foreach(SOURCE_FILE ${SOURCE_LIST})
    if(EXISTS ${SOURCE_FILE})
        # Read file content
        file(READ ${SOURCE_FILE} FILE_CONTENT)
        
        # Extract MP_QSTR_xxx patterns using regex
        # Pattern: MP_QSTR_ followed by valid identifier characters (including multiple underscores)
        string(REGEX MATCHALL "MP_QSTR_[a-zA-Z0-9_]+" QSTR_MATCHES "${FILE_CONTENT}")
        
        # Write each match to output file (removing MP_QSTR_ prefix)
        foreach(QSTR ${QSTR_MATCHES})
            string(REPLACE "MP_QSTR_" "" QSTR_NAME "${QSTR}")
            file(APPEND ${OUTPUT_FILE} "${QSTR_NAME}\n")
        endforeach()
    endif()
endforeach()

# Sort and remove duplicates
file(READ ${OUTPUT_FILE} ALL_QSTRS)
string(REPLACE "\n" ";" QSTR_LIST "${ALL_QSTRS}")
list(REMOVE_DUPLICATES QSTR_LIST)
list(SORT QSTR_LIST)

# Write sorted unique list back to file
file(WRITE ${OUTPUT_FILE} "")
foreach(QSTR ${QSTR_LIST})
    if(NOT "${QSTR}" STREQUAL "")
        file(APPEND ${OUTPUT_FILE} "${QSTR}\n")
    endif()
endforeach()