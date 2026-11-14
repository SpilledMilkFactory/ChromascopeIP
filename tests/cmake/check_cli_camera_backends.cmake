# M8.1 must query only videoio's registry; do NOT touch any camera device.
if(NOT DEFINED CHROMASCOPE_CLI OR NOT DEFINED CHROMASCOPE_EXPORT_DIR)
    message(FATAL_ERROR "Expected CLI and output scratch directory")
endif()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
execute_process(
    COMMAND "${CHROMASCOPE_CLI}" --camera-backends
    WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "--camera-backends failed: ${status} / ${err}")
endif()
foreach(needle IN ITEMS "Camera backend registry" "v4l2" "msmf" "dshow"
    "no devices opened" "not device discovery")
    string(FIND "${out}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "Backend report missing '${needle}': ${out}")
    endif()
endforeach()
foreach(args IN ITEMS "--camera-backends;--help" "--help;--camera-backends"
    "--camera-backends;--input;absent.jpg" "--camera-backends;--camera-backends")
    execute_process(
        COMMAND "${CHROMASCOPE_CLI}" ${args}
        WORKING_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}"
        RESULT_VARIABLE mixed OUTPUT_VARIABLE mixed_out ERROR_VARIABLE mixed_err)
    if(mixed EQUAL 0 OR NOT mixed_out STREQUAL "")
        message(FATAL_ERROR "Mixed --camera-backends accepted: ${args}")
    endif()
    string(FIND "${mixed_err}" "must be used alone" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "No standalone error: ${mixed_err}")
    endif()
endforeach()
file(GLOB leftovers LIST_DIRECTORIES TRUE "${CHROMASCOPE_EXPORT_DIR}/*"
    "${CHROMASCOPE_EXPORT_DIR}/.*")
foreach(item IN LISTS leftovers)
    get_filename_component(name "${item}" NAME)
    if(NOT name STREQUAL "." AND NOT name STREQUAL "..")
        message(FATAL_ERROR "Camera report created file: ${item}")
    endif()
endforeach()
