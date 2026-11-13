# M4.5: real executable, deterministic synthetic MJPEG, independent decoded
# pixel comparison, schema-v1 JSON, and negative destination/option guards.
foreach(_var CHROMASCOPE_CLI CHROMASCOPE_VIDEO_FIXTURE CHROMASCOPE_EXPORT_DIR)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "Missing ${_var}")
    endif()
endforeach()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
set(_movie "${CHROMASCOPE_EXPORT_DIR}/movie with spaces.avi")
set(_dest "${CHROMASCOPE_EXPORT_DIR}/extracted frames")
execute_process(COMMAND "${CHROMASCOPE_VIDEO_FIXTURE}" generate "${_movie}"
    RESULT_VARIABLE _result ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Unable to generate synthetic test recording: ${_stderr}")
endif()
file(SHA256 "${_movie}" _input_hash)

execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${_movie}"
    --video-probe --video-backend opencv-mjpeg --json -
    RESULT_VARIABLE _result OUTPUT_VARIABLE _json ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Video probe failed: ${_stderr}")
endif()
string(JSON _schema GET "${_json}" schema_version)
string(JSON _operation GET "${_json}" operation)
string(JSON _backend GET "${_json}" backend)
string(JSON _fps GET "${_json}" fps)
string(JSON _count GET "${_json}" reported_frame_count)
string(JSON _index GET "${_json}" frame index)
string(JSON _width GET "${_json}" frame width)
string(JSON _height GET "${_json}" frame height)
string(JSON _stamp GET "${_json}" frame timestamp_ns)
if(NOT _schema EQUAL 1 OR NOT _operation STREQUAL "video_probe" OR
   NOT _backend STREQUAL "opencv-mjpeg" OR NOT _index EQUAL 0 OR
   NOT _width EQUAL 96 OR NOT _height EQUAL 64 OR
   _fps LESS 9.9 OR _fps GREATER 10.1 OR _count LESS 6 OR _stamp LESS 0)
    message(FATAL_ERROR "Video probe schema-v1 contract incorrect: ${_json}")
endif()

execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${_movie}"
    --video-probe --video-backend opencv-mjpeg --output-dir "${_dest}"
    --json "probe.json"
    RESULT_VARIABLE _result ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0 OR NOT EXISTS "${_dest}/probe.json")
    message(FATAL_ERROR "Video probe JSON-file output failed: ${_stderr}")
endif()

execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${_movie}"
    --extract-frame 3 --video-backend opencv-mjpeg
    --output-dir "${_dest}" --output "frame-3.png" --json -
    RESULT_VARIABLE _result OUTPUT_VARIABLE _json ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Indexed PNG extraction failed: ${_stderr}")
endif()
string(JSON _operation GET "${_json}" operation)
string(JSON _index GET "${_json}" frame index)
string(JSON _output GET "${_json}" output)
file(TO_CMAKE_PATH "${_output}" _normalized_output)
file(TO_CMAKE_PATH "${_dest}/frame-3.png" _expected_output)
if(NOT _operation STREQUAL "frame_extract" OR NOT _index EQUAL 3 OR
   NOT _normalized_output STREQUAL _expected_output)
    message(FATAL_ERROR "Extraction JSON metadata incorrect: ${_json}")
endif()
execute_process(COMMAND "${CHROMASCOPE_VIDEO_FIXTURE}" verify "${_movie}"
    "${_dest}/frame-3.png" 3 RESULT_VARIABLE _result ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Extracted PNG pixels are not the indexed decoded frame: ${_stderr}")
endif()
execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${_movie}"
    --extract-frame 0 --video-backend opencv-mjpeg --output "${_dest}/frame-0.png"
    RESULT_VARIABLE _result ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Extraction at frame zero failed: ${_stderr}")
endif()
execute_process(COMMAND "${CHROMASCOPE_VIDEO_FIXTURE}" verify "${_movie}"
    "${_dest}/frame-0.png" 0 RESULT_VARIABLE _result ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Frame-zero extracted pixels differ from reference: ${_stderr}")
endif()

function(rejected name expected_error)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${ARGN}
        RESULT_VARIABLE _result ERROR_VARIABLE _stderr OUTPUT_VARIABLE _stdout)
    if("${_result}" STREQUAL "0")
        message(FATAL_ERROR "${name}: rejected command unexpectedly succeeded")
    endif()
    string(FIND "${_stderr}" "${expected_error}" _offset)
    if(_offset EQUAL -1)
        message(FATAL_ERROR "${name}: expected '${expected_error}', got '${_stderr}'")
    endif()
endfunction()

set(_missing "${_dest}/must-not-exist.png")
rejected(missing_target "requires --output" --input "${_movie}" --extract-frame 2)
rejected(negative_index "--extract-frame requires" --input "${_movie}" --extract-frame -1
    --output "${_missing}")
rejected(numeric_suffix "--extract-frame requires" --input "${_movie}" --extract-frame 3x
    --output "${_missing}")
rejected(nonexact_index "--extract-frame requires" --input "${_movie}"
    --extract-frame 9007199254740993 --output "${_missing}")
rejected(duplicate_action "Repeated or conflicting" --input "${_movie}" --video-probe
    --extract-frame 1 --output "${_missing}")
rejected(lousy_format "requires a .png" --input "${_movie}" --extract-frame 1
    --output "${_dest}/bad.jpg")
rejected(unsupported_backend "Unknown --video-backend" --input "${_movie}" --video-probe
    --video-backend missing)
rejected(backend_for_image "--video-backend requires" --input "${_movie}"
    --video-backend opencv-mjpeg)
rejected(profile_mix "cannot use still-image" --input "${_movie}" --video-probe
    --profile skittles)
rejected(profile_option_mix "cannot use still-image" --input "${_movie}" --video-probe
    --set kernel_size=5)
# M6.1 preserves the M4.5 still-image error contract while rejecting the new
# object-only options for legacy video probe/extraction operations.
rejected(extract_profile_mix "cannot use still-image" --input "${_movie}"
    --extract-frame 0 --profile skittles --output "${_missing}")
rejected(object_profile_mix "object-profile options" --input "${_movie}"
    --video-probe --profile object --model "${_dest}/absent.onnx")
rejected(probe_output "does not accept --output" --input "${_movie}" --video-probe
    --output "${_missing}")
set(_input_hardlink "${_dest}/video-hardlink.png")
file(CREATE_LINK "${_movie}" "${_input_hardlink}" RESULT _link_result)
if(_link_result STREQUAL "0")
    rejected(source_hardlink "cannot overwrite the input video" --input "${_movie}"
        --extract-frame 2 --output "${_input_hardlink}")
    file(REMOVE "${_input_hardlink}")
endif()
rejected(no_video "existing regular file" --input "${_dest}/absent.avi" --video-probe)
rejected(invalid_schema "Unsupported JSON schema" --input "${_movie}" --video-probe
    --json - --json-version 2)
rejected(unsupported_range "past reported frame count" --input "${_movie}" --extract-frame 777
    --output "${_missing}")
rejected(output_escape "escapes --output-dir" --input "${_movie}" --extract-frame 1
    --output-dir "${_dest}" --output "../escape.png")
file(WRITE "${CHROMASCOPE_EXPORT_DIR}/corrupt.avi" "not a valid video")
rejected(malformed_file "could not open" --input "${CHROMASCOPE_EXPORT_DIR}/corrupt.avi"
    --video-probe --video-backend opencv-mjpeg)
file(SHA256 "${_movie}" _after_hash)
if(NOT _input_hash STREQUAL _after_hash OR EXISTS "${_missing}" OR
   EXISTS "${CHROMASCOPE_EXPORT_DIR}/escape.png")
    message(FATAL_ERROR "Failed video CLI request modified the input or produced an unsafe output")
endif()
message(STATUS "PASS: M4.5 video probe, schema, indexed lossless extraction, negative cases")
