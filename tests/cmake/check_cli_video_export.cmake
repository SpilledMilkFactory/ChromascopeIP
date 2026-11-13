# M6.4: CLI input rejection and explicit codec/audio contract without ONNX weights.
if(NOT EXISTS "${CHROMASCOPE_CLI}")
    message(FATAL_ERROR "Missing CLI executable")
endif()
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
set(_output "${CHROMASCOPE_EXPORT_DIR}/must-not-exist.avi")
file(REMOVE "${_output}")
function(reject label)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(status EQUAL 0)
        message(FATAL_ERROR "M6.4 ${label}: invalid command succeeded: ${output}")
    endif()
    if(EXISTS "${_output}")
        message(FATAL_ERROR "M6.4 ${label}: invalid command created a video")
    endif()
endfunction()
set(_common --export-annotated-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model "${CHROMASCOPE_EXPORT_DIR}/no-model.onnx"
    --output "${_output}" --video-codec mjpeg --audio-policy drop)
reject("missing audio policy" --export-annotated-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model missing.onnx --output "${_output}" --video-codec mjpeg)
reject("missing codec policy" --export-annotated-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model missing.onnx --output "${_output}" --audio-policy drop)
reject("unsupported audio copy" ${_common} --audio-policy copy)
reject("unsupported h264" --export-annotated-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model missing.onnx --output "${_output}" --video-codec h264 --audio-policy drop)
reject("MP4 extension" --export-annotated-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model missing.onnx --output "${_output}.mp4" --video-codec mjpeg --audio-policy drop)
reject("missing output" --export-annotated-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model missing.onnx --video-codec mjpeg --audio-policy drop)
reject("invalid fps" ${_common} --output-fps 0)
reject("invalid fps suffix" ${_common} --output-fps 24junk)
reject("forbidden video step" ${_common} --frame-step 2)
reject("JSONL conflict" ${_common} --jsonl -)
reject("JSON conflict" ${_common} --json -)
reject("conflicting operations" ${_common} --analyze-video)
reject("audio policy without action" --input "${CHROMASCOPE_INPUT}" --audio-policy drop)
reject("missing model" ${_common})
message(STATUS "PASS: M6.4 codec/audio/output contract validation")
