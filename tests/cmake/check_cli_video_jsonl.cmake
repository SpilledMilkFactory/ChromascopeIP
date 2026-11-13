# M6.3: offline argument validation and file safety, no ONNX weights needed.
if(NOT EXISTS "${CHROMASCOPE_CLI}")
    message(FATAL_ERROR "Missing CLI executable")
endif()
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
set(_output "${CHROMASCOPE_EXPORT_DIR}/should-not-exist.jsonl")
file(REMOVE "${_output}")

function(reject label)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(status EQUAL 0)
        message(FATAL_ERROR "M6.3 ${label}: invalid command succeeded: ${output}")
    endif()
    if(EXISTS "${_output}")
        message(FATAL_ERROR "M6.3 ${label}: wrote output after invalid arguments")
    endif()
endfunction()

reject("missing profile" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --model bad.onnx --jsonl "${_output}")
reject("missing model" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --jsonl "${_output}")
reject("missing jsonl" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx)
reject("conflicting actions" --analyze-video --detect-video-frame 1
    --input "${CHROMASCOPE_INPUT}" --profile object --model bad.onnx --jsonl "${_output}")
reject("wrong extension" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${_output}.json")
reject("json conflict" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${_output}" --json -)
reject("png conflict" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${_output}" --output annotated.png)
reject("bad step" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${_output}" --frame-step 0)
reject("bad max" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${_output}" --max-video-frames 100001)
reject("float max" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${_output}" --max-video-frames 1junk)
reject("step outside action" --input "${CHROMASCOPE_INPUT}" --frame-step 2)
reject("jsonl outside action" --input "${CHROMASCOPE_INPUT}" --jsonl "${_output}")
reject("overwrite source" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model bad.onnx --jsonl "${CHROMASCOPE_INPUT}")
reject("missing pinned weights" --analyze-video --input "${CHROMASCOPE_INPUT}"
    --profile object --model "${CHROMASCOPE_EXPORT_DIR}/no-weights.onnx" --jsonl "${_output}")
message(STATUS "PASS: M6.3 offline CLI JSONL argument and output safety")
