# Applies a patch to a fetched dependency, idempotently (safe to run again on an already
# patched tree). Used as a FetchContent PATCH_COMMAND:
#   cmake -DPATCH_FILE=<patch> -DSOURCE_DIR=<dir> -P apply_patch.cmake

execute_process(COMMAND git apply --check "${PATCH_FILE}"
                WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE can_apply
                OUTPUT_QUIET ERROR_QUIET)
if(can_apply EQUAL 0)
    execute_process(COMMAND git apply "${PATCH_FILE}"
                    WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE applied)
    if(NOT applied EQUAL 0)
        message(FATAL_ERROR "Applying ${PATCH_FILE} failed")
    endif()
    message(STATUS "Applied ${PATCH_FILE}")
    return()
endif()

execute_process(COMMAND git apply --reverse --check "${PATCH_FILE}"
                WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE already_applied
                OUTPUT_QUIET ERROR_QUIET)
if(already_applied EQUAL 0)
    message(STATUS "Already applied: ${PATCH_FILE}")
else()
    message(FATAL_ERROR "${PATCH_FILE} doesn't apply (did the dependency version change?)")
endif()
