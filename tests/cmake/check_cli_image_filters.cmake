# Headless end-to-end profile discovery, export, and machine-readable CLI checks.
foreach(profile grayscale gaussian_blur binary_threshold canny_edges)
    set(output "${CHROMASCOPE_EXPORT_DIR}/${profile}.png")
    set(json "${CHROMASCOPE_EXPORT_DIR}/${profile}.json")
    file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
    execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${CHROMASCOPE_INPUT}"
        --profile "${profile}" --output "${output}" --json "${json}"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(NOT result EQUAL 0 OR NOT EXISTS "${output}" OR NOT EXISTS "${json}")
        message(FATAL_ERROR "${profile} CLI export failed: ${stdout} ${stderr}")
    endif()
    file(SIZE "${output}" output_size)
    if(output_size LESS 100)
        message(FATAL_ERROR "${profile} image export was empty")
    endif()
    file(READ "${json}" content)
    if(NOT content MATCHES "\\\"profile\\\": \\\"${profile}\\\"" OR
       NOT content MATCHES "\\\"detections\\\": \\[\\]" OR
       NOT content MATCHES "\\\"total\\\": 0")
        message(FATAL_ERROR "Unexpected ${profile} JSON: ${content}")
    endif()
endforeach()
# CLI-specific parameter overrides must reach the processor, rather than being ignored.
execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${CHROMASCOPE_INPUT}"
    --profile gaussian_blur --set kernel_size=7 --set sigma=2
    --output "${CHROMASCOPE_EXPORT_DIR}/custom-blur.png"
    RESULT_VARIABLE result ERROR_VARIABLE stderr)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Custom blur options rejected: ${stderr}")
endif()
