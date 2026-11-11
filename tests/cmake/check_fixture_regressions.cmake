# Exercise the existing SHA-256 integrity gate on isolated temporary copies.
# Never corrupt, rewrite or move the four original, rights-restricted images.
foreach(_required CHROMASCOPE_FIXTURE_DIR CHROMASCOPE_FIXTURE_MANIFEST
                  CHROMASCOPE_FIXTURE_VERIFY CHROMASCOPE_EXPORT_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "missing ${_required}")
    endif()
endforeach()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
foreach(_name s1.jpg s2.jpg s3.jpg s4.jpg)
    file(COPY_FILE "${CHROMASCOPE_FIXTURE_DIR}/${_name}"
                   "${CHROMASCOPE_EXPORT_DIR}/${_name}")
endforeach()
set(_manifest "${CHROMASCOPE_EXPORT_DIR}/skittles.sha256")
file(COPY_FILE "${CHROMASCOPE_FIXTURE_MANIFEST}" "${_manifest}")

function(_verify _should_pass _case)
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DCHROMASCOPE_FIXTURE_DIR=${CHROMASCOPE_EXPORT_DIR}"
        "-DCHROMASCOPE_FIXTURE_MANIFEST=${_manifest}"
        -P "${CHROMASCOPE_FIXTURE_VERIFY}"
        RESULT_VARIABLE _code OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(_should_pass AND NOT "${_code}" STREQUAL "0")
        message(FATAL_ERROR "${_case}: unmodified fixtures unexpectedly rejected: ${_err}")
    endif()
    if(NOT _should_pass AND "${_code}" STREQUAL "0")
        message(FATAL_ERROR "${_case}: corrupted fixture set was accepted")
    endif()
endfunction()

_verify(TRUE pristine_copies)
file(APPEND "${CHROMASCOPE_EXPORT_DIR}/s1.jpg" "M3.6 isolated tamper")
_verify(FALSE modified_bytes)
file(COPY_FILE "${CHROMASCOPE_FIXTURE_DIR}/s1.jpg"
               "${CHROMASCOPE_EXPORT_DIR}/s1.jpg")
_verify(TRUE restored_bytes)
file(REMOVE "${CHROMASCOPE_EXPORT_DIR}/s3.jpg")
_verify(FALSE missing_fixture)
file(COPY_FILE "${CHROMASCOPE_FIXTURE_DIR}/s3.jpg"
               "${CHROMASCOPE_EXPORT_DIR}/s3.jpg")
_verify(TRUE restored_fixture)
file(READ "${_manifest}" _original)
file(APPEND "${_manifest}" "\ns1.jpg 0000000000000000000000000000000000000000000000000000000000000000\n")
_verify(FALSE duplicate_manifest_entry)
file(WRITE "${_manifest}" "${_original}")
_verify(TRUE restored_manifest)
file(WRITE "${_manifest}" "s1.jpg abc\n")
_verify(FALSE invalid_sha256)
file(WRITE "${_manifest}" "${_original}")
_verify(TRUE restored_after_invalid_manifest)
message(STATUS "PASS: pristine fixtures, byte tamper, missing file and invalid manifest cases")
