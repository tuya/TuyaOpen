##
# @file util.cmake
# @brief 
# @author Tuya
# @version 1.0.0
# @date 2023-03-26
#/

function(list_components RETURN DIR)
    execute_process(COMMAND "find" ${DIR} "-maxdepth" "2" "-name" "CMakeLists.txt"
        OUTPUT_VARIABLE find_dir)
    string(REPLACE "\n" ";" sub_split ${find_dir})
    foreach(s ${sub_split})
        get_filename_component(sub_dir ${s} DIRECTORY)
        get_filename_component(comp_name ${sub_dir} NAME)
        list(APPEND ans ${comp_name})
    endforeach(s)
    set(${RETURN} "${ans}" PARENT_SCOPE)
endfunction()


function(copy_public_include COMP_PUBINC HDIR)
    execute_process(COMMAND "mkdir" "-p" ${HDIR})
    foreach(inc ${COMP_PUBINC})
        file(GLOB allCopyFiles  "${inc}/*")
        file(COPY ${allCopyFiles} DESTINATION ${HDIR})
    endforeach(inc)
endfunction()


function(deal_with_components COMPONENTS_LIB HDIR)
    # message(STATUS "[UTIL] COMPONENTS_LIB: ${COMPONENTS_LIB}")
    # message(STATUS "[UTIL] HDIR: ${HDIR}")
    foreach(c ${COMPONENTS_LIB})
        # target_link_libraries(${c} ${COMPONENTS_LIB})
        target_include_directories(${c}
            PUBLIC
            ${HDIR}
            )
    endforeach(c)
endfunction()


function(list_uts RETURN DIR)
    execute_process(COMMAND "find" ${DIR} "-maxdepth" "3" "-wholename" "*ut/CMakeLists.txt"
        OUTPUT_VARIABLE find_dir)
    string(REPLACE "\n" ";" sub_split ${find_dir})
    foreach(s ${sub_split})
        get_filename_component(ut_dir ${s} DIRECTORY)
        get_filename_component(sub_dir ${ut_dir} DIRECTORY)
        get_filename_component(comp_name ${sub_dir} NAME)
        list(APPEND ans ${comp_name})
    endforeach(s)
    set(${RETURN} "${ans}" PARENT_SCOPE)
endfunction()


function(git_clone REPO DIR)
    if(EXISTS ${DIR})
        message(STATUS "[UTIL] Repo already exists [${DIR}]")
        return()
    endif()
    execute_process(COMMAND "git" "clone" ${REPO} ${DIR}
        RESULT_VARIABLE res)
    if(NOT ${res} STREQUAL "0")
        message(WARNING "
        [UTIL]Git clone fail [${REPO}].
        Please check or run the command manually:
        [git clone ${GTEST_REPO} ${GTEST_DIR}]
        ")
    endif()
endfunction()


function(list_subdirectories RETURN DIR)
    file(GLOB_RECURSE subdirectories LIST_DIRECTORIES true "${DIR}/*")
    set(non_hidden_subdirectories ${DIR})
    foreach(subdir ${subdirectories})
        if(IS_DIRECTORY "${subdir}")
            get_filename_component(subdir_name ${subdir} NAME)
            if(NOT ${subdir_name} MATCHES "^\\.")
                list(APPEND non_hidden_subdirectories "${subdir}")
            endif()
        endif()
    endforeach()
    # message(STATUS "[UTIL] non_hidden_subdirectories:[${non_hidden_subdirectories}]")
    set(${RETURN} ${non_hidden_subdirectories} PARENT_SCOPE)
endfunction()


function(get_latest_git_commit target_path result_var)
    # only for .git
    if(NOT EXISTS "${target_path}/.git")
        set(${result_var} "NONE" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND git -C "${target_path}" rev-parse --is-inside-work-tree
        RESULT_VARIABLE is_git_repo_result
        OUTPUT_VARIABLE is_git_repo_output
        ERROR_VARIABLE git_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(NOT is_git_repo_result EQUAL 0 OR NOT is_git_repo_output STREQUAL "true")
        set(${result_var} "NONE" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND git -C "${target_path}" rev-parse HEAD
        RESULT_VARIABLE commit_result
        OUTPUT_VARIABLE commit_hash
        ERROR_VARIABLE git_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(NOT commit_result EQUAL 0)
        set(${result_var} "NONE" PARENT_SCOPE)
        return()
    endif()

    set(${result_var} "${commit_hash}" PARENT_SCOPE)
endfunction()


function(get_latest_git_tag target_path result_var)
    # only for .git
    if(NOT EXISTS "${target_path}/.git")
        set(${result_var} "NONE" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND git -C "${target_path}" rev-parse --is-inside-work-tree
        RESULT_VARIABLE is_git_repo_result
        OUTPUT_VARIABLE is_git_repo_output
        ERROR_VARIABLE git_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(NOT is_git_repo_result EQUAL 0 OR NOT is_git_repo_output STREQUAL "true")
        set(${result_var} "NONE" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND git -C "${target_path}" describe --tags --abbrev=0
        RESULT_VARIABLE tag_result
        OUTPUT_VARIABLE latest_tag
        ERROR_VARIABLE git_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(NOT tag_result EQUAL 0)
        set(${result_var} "NONE" PARENT_SCOPE)
    endif()

    set(${result_var} "${latest_tag}" PARENT_SCOPE)
endfunction()
