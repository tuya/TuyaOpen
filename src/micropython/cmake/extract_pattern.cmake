# Extract pattern from source files
# This is a generic pattern extraction script used by mpy_prepare.cmake
# 
# Usage:
#   cmake -DSOURCE_FILES="file1.c;file2.c" \
#         -DOUTPUT_FILE=output.txt \
#         -DPATTERN="MP_REGISTER_MODULE" \
#         -P extract_pattern.cmake

# Parse the source files list
string(REPLACE ";" ";" SOURCE_FILE_LIST "${SOURCE_FILES}")

# Open output file
file(WRITE ${OUTPUT_FILE} "")

# Process each source file
foreach(SRC_FILE ${SOURCE_FILE_LIST})
    if(EXISTS ${SRC_FILE})
        # Read the file content
        file(READ ${SRC_FILE} FILE_CONTENT)

        # CMake treats semicolons as list separators, which interferes with regex matching
        # Replace semicolons with a placeholder before processing
        string(REPLACE ";" "__SEMICOLON__" FILE_CONTENT "${FILE_CONTENT}")
        
        # Extract lines containing the pattern
        # For MP_REGISTER_MODULE, we want the entire macro call
        if(PATTERN STREQUAL "MP_REGISTER_MODULE")
            # Match MP_REGISTER_MODULE(...); with placeholder for semicolon
            string(REGEX MATCHALL "MP_REGISTER_MODULE\\([^)]*\\)__SEMICOLON__" MATCHES "${FILE_CONTENT}")

            foreach(MATCH ${MATCHES})
                # Clean up the match (remove excessive whitespace/newlines)
                string(REGEX REPLACE "\n" " " CLEAN_MATCH "${MATCH}")
                string(REGEX REPLACE "[ \t]+" " " CLEAN_MATCH "${CLEAN_MATCH}")

                # Replace placeholder back to semicolon
                string(REPLACE "__SEMICOLON__" ";" CLEAN_MATCH "${CLEAN_MATCH}")

                # Write to output file with semicolon
                file(APPEND ${OUTPUT_FILE} "${CLEAN_MATCH}\n")

                # Debug output (remove semicolon for display)
                string(REGEX REPLACE ";$" "" DISPLAY_MATCH "${CLEAN_MATCH}")
                message(STATUS "Found: ${DISPLAY_MATCH}")
            endforeach()
        elseif(PATTERN STREQUAL "MP_REGISTER_ROOT_POINTER")
            # Match MP_REGISTER_ROOT_POINTER(...); with placeholder for semicolon
            string(REGEX MATCHALL "MP_REGISTER_ROOT_POINTER\\([^)]*\\)__SEMICOLON__" MATCHES "${FILE_CONTENT}")

            foreach(MATCH ${MATCHES})
                # Clean up the match (remove excessive whitespace/newlines)
                string(REGEX REPLACE "\n" " " CLEAN_MATCH "${MATCH}")
                string(REGEX REPLACE "[ \t]+" " " CLEAN_MATCH "${CLEAN_MATCH}")

                # Replace placeholder back to semicolon
                string(REPLACE "__SEMICOLON__" ";" CLEAN_MATCH "${CLEAN_MATCH}")

                # Write to output file with semicolon
                file(APPEND ${OUTPUT_FILE} "${CLEAN_MATCH}\n")

                # Debug output (remove semicolon for display)
                string(REGEX REPLACE ";$" "" DISPLAY_MATCH "${CLEAN_MATCH}")
                message(STATUS "Found: ${DISPLAY_MATCH}")
            endforeach()
        else()
            # Generic pattern matching (for other potential uses)
            string(REGEX MATCHALL ".*${PATTERN}.*" MATCHES "${FILE_CONTENT}")
            
            foreach(MATCH ${MATCHES})
                file(APPEND ${OUTPUT_FILE} "${MATCH}\n")
            endforeach()
        endif()
    endif()
endforeach()

message(STATUS "Pattern extraction complete: ${OUTPUT_FILE}")