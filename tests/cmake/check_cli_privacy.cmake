# Headless runtime check. No model, network, or media files are needed.
if(NOT DEFINED CHROMASCOPE_CLI OR NOT DEFINED CHROMASCOPE_EXPORT_DIR)
    message(FATAL_ERROR "CLI and private output directory are required")
endif()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
execute_process(
    COMMAND "${CHROMASCOPE_CLI}" --privacy-info
    WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "--privacy-info failed: ${status} / ${err}")
endif()
foreach(needle IN ITEMS "local privacy" "no upload" "no face recognition"
    "audio discarded" "not securely erased" "docs/M7_6_LOCAL_PRIVACY.md")
    string(FIND "${out}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Missing privacy disclosure: ${needle}")
    endif()
endforeach()
execute_process(
    COMMAND "${CHROMASCOPE_CLI}" --privacy-info --input nonexistent.jpg
    WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
    RESULT_VARIABLE mixed_status OUTPUT_VARIABLE mixed_out ERROR_VARIABLE mixed_err)
if(mixed_status EQUAL 0 OR NOT mixed_out STREQUAL "")
    message(FATAL_ERROR "Mixed --privacy-info invocation was accepted")
endif()
execute_process(
    COMMAND "${CHROMASCOPE_CLI}" --help --privacy-info
    WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
    RESULT_VARIABLE mixed_help_status OUTPUT_VARIABLE mixed_help_out ERROR_VARIABLE mixed_help_err)
if(mixed_help_status EQUAL 0 OR NOT mixed_help_out STREQUAL "")
    message(FATAL_ERROR "Mixed --help and --privacy-info was accepted")
endif()
string(FIND "${mixed_err}" "must be used alone" mixed_pos)
if(mixed_pos EQUAL -1)
    message(FATAL_ERROR "Mixed invocation error not explicit: ${mixed_err}")
endif()
file(GLOB leftover LIST_DIRECTORIES TRUE "${CHROMASCOPE_EXPORT_DIR}/*"
    "${CHROMASCOPE_EXPORT_DIR}/.*")
# Some platforms return . and .., which are not created runtime artifacts.
foreach(item IN LISTS leftover)
    get_filename_component(name "${item}" NAME)
    if(NOT name STREQUAL "." AND NOT name STREQUAL "..")
        message(FATAL_ERROR "Informational CLI created a file: ${item}")
    endif()
endforeach()
