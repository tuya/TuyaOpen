##
# @file process_qstr.cmake
# @brief CMake script to process QSTR files
# This script handles different QSTR processing modes
#/

# Input variables:
# INPUT_FILE - Input file to process
# OUTPUT_FILE - Output file
# MODE - Processing mode:
#   APPEND_Q_LINES - Extract Q(...) lines from input and append to output
#   CONVERT_TO_Q - Convert plain QSTR names to Q(...) format
#   APPEND_Q_LINES_IF_EXISTS - Same as APPEND_Q_LINES but only if input exists

if(MODE STREQUAL "APPEND_Q_LINES")
    # Extract Q(...) lines from input file and append to output
    if(EXISTS ${INPUT_FILE})
        file(READ ${INPUT_FILE} FILE_CONTENT)
        # Match Q(...) patterns
        string(REGEX MATCHALL "Q\\([^)]+\\)" Q_MATCHES "${FILE_CONTENT}")
        
        foreach(Q_LINE ${Q_MATCHES})
            file(APPEND ${OUTPUT_FILE} "${Q_LINE}\n")
        endforeach()
    else()
        message(WARNING "QSTR: Input file not found: ${INPUT_FILE}")
    endif()
    
elseif(MODE STREQUAL "CONVERT_TO_Q")
    # Convert plain QSTR names to Q(...) format
    if(EXISTS ${INPUT_FILE})
        file(READ ${INPUT_FILE} FILE_CONTENT)
        string(REPLACE "\n" ";" QSTR_LIST "${FILE_CONTENT}")
        
        foreach(QSTR ${QSTR_LIST})
            if(NOT "${QSTR}" STREQUAL "")
                file(APPEND ${OUTPUT_FILE} "Q(${QSTR})\n")
            endif()
        endforeach()
    endif()
    
elseif(MODE STREQUAL "APPEND_Q_LINES_IF_EXISTS")
    # Same as APPEND_Q_LINES but only if file exists (for optional port file)
    if(EXISTS ${INPUT_FILE})
        file(APPEND ${OUTPUT_FILE} "\n// Port-specific QSTRs\n")
        file(READ ${INPUT_FILE} FILE_CONTENT)
        string(REGEX MATCHALL "Q\\([^)]+\\)" Q_MATCHES "${FILE_CONTENT}")
        
        foreach(Q_LINE ${Q_MATCHES})
            file(APPEND ${OUTPUT_FILE} "${Q_LINE}\n")
        endforeach()
    endif()
    
else()
    message(FATAL_ERROR "Unknown QSTR processing mode: ${MODE}")
endif()