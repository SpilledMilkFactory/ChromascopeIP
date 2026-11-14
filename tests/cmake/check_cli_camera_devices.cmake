# M8.2 CLI is passive, standalone and creates no state in fresh directory.
if(NOT DEFINED CHROMASCOPE_CLI OR NOT DEFINED CHROMASCOPE_EXPORT_DIR)
    message(FATAL_ERROR "Expected CLI and scratch directory")
endif()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
execute_process(COMMAND "${CHROMASCOPE_CLI}" --camera-devices
    WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT status EQUAL 0 OR NOT errors STREQUAL "")
    message(FATAL_ERROR "Camera listing failed: ${status} ${errors}")
endif()
foreach(needle IN ITEMS "Camera device listing" "no devices opened"
    "permission prompt" "Windows requires explicit opt-in")
    string(FIND "${output}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Camera listing omitted '${needle}': ${output}")
    endif()
endforeach()
foreach(args IN ITEMS "--camera-devices;--help" "--help;--camera-devices"
    "--camera-devices;--input;absent.jpg" "--camera-devices;--camera-backends"
    "--camera-devices;--camera-devices")
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${args}
        WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(result EQUAL 0 OR NOT out STREQUAL "")
        message(FATAL_ERROR "Mixed camera listing accepted: ${args}")
    endif()
    string(FIND "${err}" "must be used alone" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Missing standalone error: ${err}")
    endif()
endforeach()
file(GLOB leftovers LIST_DIRECTORIES TRUE "${CHROMASCOPE_EXPORT_DIR}/*"
    "${CHROMASCOPE_EXPORT_DIR}/.*")
foreach(item IN LISTS leftovers)
    get_filename_component(name "${item}" NAME)
    if(NOT name STREQUAL "." AND NOT name STREQUAL "..")
        message(FATAL_ERROR "Camera listing wrote file: ${item}")
    endif()
endforeach()
