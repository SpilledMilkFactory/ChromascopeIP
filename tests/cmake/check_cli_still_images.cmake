# Real CLI smoke/negative regression. All generated files remain under the
# CTest build tree, never alongside source-controlled historical photographs.
foreach(_required CHROMASCOPE_CLI CHROMASCOPE_INPUT CHROMASCOPE_EXPORT_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "missing ${_required}")
    endif()
endforeach()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
file(SHA256 "${CHROMASCOPE_INPUT}" _source_hash)

# Test signature-driven decoding independently of the filename extension.
file(COPY_FILE "${CHROMASCOPE_INPUT}" "${CHROMASCOPE_EXPORT_DIR}/renamed.bin")
execute_process(COMMAND "${CHROMASCOPE_CLI}"
    --input "${CHROMASCOPE_EXPORT_DIR}/renamed.bin" --profile grayscale --json -
    RESULT_VARIABLE _code OUTPUT_VARIABLE _result ERROR_VARIABLE _error)
if(NOT "${_code}" STREQUAL "0")
    message(FATAL_ERROR "JPEG data with alternate suffix failed: ${_error}")
endif()
string(JSON _profile GET "${_result}" profile)
string(JSON _version GET "${_result}" schema_version)
string(JSON _width GET "${_result}" width)
string(JSON _height GET "${_result}" height)
if(NOT _profile STREQUAL "grayscale" OR NOT _version EQUAL 1 OR
   NOT _width EQUAL 4032 OR NOT _height EQUAL 3024)
    message(FATAL_ERROR "Renamed JPEG did not preserve schema/profile/dimensions")
endif()

function(expect_bad _case _path)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${_path}"
        --profile grayscale --output "${CHROMASCOPE_EXPORT_DIR}/${_case}-output.png"
        RESULT_VARIABLE _code OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if("${_code}" STREQUAL "0")
        message(FATAL_ERROR "${_case}: invalid image unexpectedly accepted")
    endif()
    if(EXISTS "${CHROMASCOPE_EXPORT_DIR}/${_case}-output.png")
        message(FATAL_ERROR "${_case}: failed decode wrote an export")
    endif()
endfunction()
file(WRITE "${CHROMASCOPE_EXPORT_DIR}/empty.jpg" "")
file(WRITE "${CHROMASCOPE_EXPORT_DIR}/garbage.png" "not-a-png")
file(WRITE "${CHROMASCOPE_EXPORT_DIR}/unsupported.gif" "GIF89a invalid")
file(WRITE "${CHROMASCOPE_EXPORT_DIR}/short.jpg" "J")
expect_bad(empty "${CHROMASCOPE_EXPORT_DIR}/empty.jpg")
expect_bad(unknown "${CHROMASCOPE_EXPORT_DIR}/garbage.png")
expect_bad(unsupported "${CHROMASCOPE_EXPORT_DIR}/unsupported.gif")
expect_bad(truncated "${CHROMASCOPE_EXPORT_DIR}/short.jpg")
expect_bad(missing "${CHROMASCOPE_EXPORT_DIR}/missing.png")
expect_bad(directory "${CHROMASCOPE_EXPORT_DIR}")
file(SHA256 "${CHROMASCOPE_INPUT}" _source_after)
if(NOT _source_hash STREQUAL _source_after)
    message(FATAL_ERROR "Rejected input path modified a historical sample")
endif()
message(STATUS "PASS: CLI signature-based JPEG input and six error paths")
