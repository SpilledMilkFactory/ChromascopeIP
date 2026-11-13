# M6.1: fully offline CLI command validation. No model weights or network.
if(NOT EXISTS "${CHROMASCOPE_CLI}")
    message(FATAL_ERROR "Missing CLI executable for M6.1")
endif()
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
set(_output "${CHROMASCOPE_EXPORT_DIR}/must-not-exist.png")
file(REMOVE "${_output}")

function(expect_reject tag)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${ARGN}
        RESULT_VARIABLE _status OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_status EQUAL 0)
        message(FATAL_ERROR "M6.1 ${tag}: invalid command succeeded: ${_stdout}")
    endif()
    if(EXISTS "${_output}")
        message(FATAL_ERROR "M6.1 ${tag}: output created despite failed command")
    endif()
endfunction()

expect_reject("missing profile" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame 0 --model "${CHROMASCOPE_EXPORT_DIR}/absent.onnx" --output "${_output}")
expect_reject("missing model" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame 0 --profile object --output "${_output}")
expect_reject("conflicting actions" --input "${CHROMASCOPE_INPUT}" --video-probe
    --detect-video-frame 0 --profile object --model invalid.onnx --output "${_output}")
expect_reject("bad frame index" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame -1 --profile object --model invalid.onnx --output "${_output}")
expect_reject("invalid score" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame 0 --profile object --model invalid.onnx
    --confidence nan --output "${_output}")
expect_reject("wrong image suffix" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame 0 --profile object --model invalid.onnx --output "${_output}.jpg")
expect_reject("missing pinned model" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame 0 --profile object --model "${CHROMASCOPE_EXPORT_DIR}/absent.onnx"
    --output "${_output}")
expect_reject("overwrite source video" --input "${CHROMASCOPE_INPUT}"
    --detect-video-frame 0 --profile object --model invalid.onnx
    --output "${CHROMASCOPE_INPUT}")
message(STATUS "PASS: M6.1 offline recorded-video object CLI rejection paths")
