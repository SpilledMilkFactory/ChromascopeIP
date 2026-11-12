# M4.7: one deterministic, private MJPEG fixture drives CLI error checks.
# All artifacts remain under the configured out-of-source test directory.
foreach(_required CHROMASCOPE_CLI CHROMASCOPE_VIDEO_FIXTURE CHROMASCOPE_EXPORT_DIR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "Missing required variable ${_required}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
set(_movie "${CHROMASCOPE_EXPORT_DIR}/m4-7 source with spaces.avi")
set(_output "${CHROMASCOPE_EXPORT_DIR}/must-not-exist.png")
set(_json "${CHROMASCOPE_EXPORT_DIR}/must-not-exist.json")
file(REMOVE "${_movie}" "${_output}" "${_json}")
execute_process(COMMAND "${CHROMASCOPE_VIDEO_FIXTURE}" generate "${_movie}"
    RESULT_VARIABLE _generator_rc ERROR_VARIABLE _generator_stderr)
if(NOT "${_generator_rc}" STREQUAL "0")
    message(FATAL_ERROR "Synthetic MJPEG fixture generation failed: ${_generator_stderr}")
endif()
file(SHA256 "${_movie}" _original_hash)

set(_empty "${CHROMASCOPE_EXPORT_DIR}/empty.avi")
set(_malformed "${CHROMASCOPE_EXPORT_DIR}/malformed.avi")
set(_truncated "${CHROMASCOPE_EXPORT_DIR}/truncated.avi")
file(WRITE "${_empty}" "")
file(WRITE "${_malformed}" "not a RIFF/AVI container")
# Deliberately incomplete RIFF header. OpenCV must reject this malformed input.
file(WRITE "${_truncated}" "RIFF")

function(expect_failure scenario)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${ARGN}
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr
        TIMEOUT 20)
    # The application catches CLI errors and returns 1. A crash, signal,
    # timeout, or accidental success must not count as a passing rejection.
    if(NOT "${_rc}" STREQUAL "1")
        message(FATAL_ERROR "${scenario}: expected controlled exit 1, got '${_rc}'\n${_stderr}")
    endif()
    string(FIND "${_stderr}" "chromascope-cli:" _diagnostic)
    if(_diagnostic EQUAL -1)
        message(FATAL_ERROR "${scenario}: missing actionable CLI diagnostic: ${_stderr}")
    endif()
    if(EXISTS "${_output}" OR EXISTS "${_json}")
        message(FATAL_ERROR "${scenario}: rejected command created an output")
    endif()
endfunction()

expect_failure(missing_input --video-probe --input
    "${CHROMASCOPE_EXPORT_DIR}/does-not-exist.avi" --video-backend opencv-mjpeg)
expect_failure(directory_input --video-probe --input
    "${CHROMASCOPE_EXPORT_DIR}" --video-backend opencv-mjpeg)
expect_failure(empty_input --video-probe --input
    "${_empty}" --video-backend opencv-mjpeg)
expect_failure(malformed_input --video-probe --input
    "${_malformed}" --video-backend opencv-mjpeg)
expect_failure(truncated_header --video-probe --input
    "${_truncated}" --video-backend opencv-mjpeg)
expect_failure(malformed_extraction --input "${_malformed}"
    --extract-frame 0 --video-backend opencv-mjpeg --output "${_output}")
expect_failure(missing_output --input "${_movie}"
    --extract-frame 1 --video-backend opencv-mjpeg)
expect_failure(negative_frame --input "${_movie}"
    --extract-frame -1 --output "${_output}")
expect_failure(invalid_frame_suffix --input "${_movie}"
    --extract-frame 3junk --output "${_output}")
expect_failure(overflow_frame --input "${_movie}"
    --extract-frame 18446744073709551616 --output "${_output}")
expect_failure(out_of_range_frame --input "${_movie}"
    --extract-frame 99999 --video-backend opencv-mjpeg --output "${_output}")
expect_failure(invalid_backend --input "${_movie}"
    --video-probe --video-backend definitely-not-a-backend)
expect_failure(conflicting_actions --input "${_movie}"
    --video-probe --extract-frame 1 --output "${_output}")
expect_failure(unsupported_json_version --input "${_movie}"
    --video-probe --json "${_json}" --json-version 999)
expect_failure(output_traversal --input "${_movie}"
    --extract-frame 1 --output-dir "${CHROMASCOPE_EXPORT_DIR}"
    --output "../m4-7-escape.png")
expect_failure(input_overwrite --input "${_movie}"
    --extract-frame 1 --output "${_movie}")

file(SHA256 "${_movie}" _after_hash)
if(NOT _after_hash STREQUAL _original_hash OR
   EXISTS "${_output}" OR EXISTS "${_json}" OR
   EXISTS "${CHROMASCOPE_EXPORT_DIR}/../m4-7-escape.png")
    message(FATAL_ERROR "Rejected CLI command changed source bytes or created a forbidden output")
endif()
message(STATUS "PASS M4.7: generated MJPEG fixture, controlled CLI failures, no unsafe outputs")
