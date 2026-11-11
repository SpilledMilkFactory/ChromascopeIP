# M3.5 real-executable contract: profile configuration, schema v1, output
# directory, stdout cleanliness, and negative paths. Uses CMake's JSON parser.
foreach(_required CHROMASCOPE_CLI CHROMASCOPE_INPUT CHROMASCOPE_EXPORT_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "missing ${_required}")
    endif()
endforeach()
execute_process(COMMAND "${CHROMASCOPE_CLI}" --list-profiles
    RESULT_VARIABLE _profiles_code OUTPUT_VARIABLE _profiles ERROR_VARIABLE _profiles_error)
if(NOT _profiles_code EQUAL 0 OR NOT _profiles MATCHES "gaussian_blur - Gaussian Blur" OR
   NOT _profiles MATCHES "kernel_size=5" OR NOT _profiles MATCHES "integer")
    message(FATAL_ERROR "Profile discovery/options bounds missing: ${_profiles_error}")
endif()
file(REMOVE_RECURSE "${CHROMASCOPE_EXPORT_DIR}")
file(MAKE_DIRECTORY "${CHROMASCOPE_EXPORT_DIR}")
set(_out "${CHROMASCOPE_EXPORT_DIR}/with spaces")

execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${CHROMASCOPE_INPUT}"
    --profile gaussian_blur --set kernel_size=5 --set sigma=1.8
    --json-version 1 --output-dir "${_out}"
    --output "blur.png" --json "blur.json"
    RESULT_VARIABLE _code OUTPUT_VARIABLE _text ERROR_VARIABLE _error)
if(NOT _code EQUAL 0)
    message(FATAL_ERROR "Gaussian invocation failed: ${_error} ${_text}")
endif()
foreach(_name blur.png blur.json)
    if(NOT EXISTS "${_out}/${_name}")
        message(FATAL_ERROR "Output missing from explicit directory: ${_name}")
    endif()
endforeach()
file(READ "${_out}/blur.png" _signature LIMIT 8 HEX)
if(NOT _signature STREQUAL "89504e470d0a1a0a")
    message(FATAL_ERROR "Expected PNG signature in CLI raster output")
endif()
file(READ "${_out}/blur.json" _json)
string(JSON _version GET "${_json}" schema_version)
string(JSON _profile GET "${_json}" profile)
string(JSON _total GET "${_json}" total)
string(JSON _length LENGTH "${_json}" detections)
if(NOT _version EQUAL 1 OR NOT _profile STREQUAL "gaussian_blur" OR
   NOT _total EQUAL 0 OR NOT _length EQUAL 0)
    message(FATAL_ERROR "Generic schema v1 changed unexpectedly")
endif()

execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${CHROMASCOPE_INPUT}"
    --profile hsv_segmentation --set min_hue=170 --set max_hue=10
    --output-dir "${_out}" --mask-output "seg-mask.png" --json -
    RESULT_VARIABLE _code OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _code EQUAL 0)
    message(FATAL_ERROR "HSV wraparound/stdout invocation failed: ${_stderr}")
endif()
string(JSON _hsv_version GET "${_stdout}" schema_version)
string(JSON _hsv_profile GET "${_stdout}" profile)
if(NOT _hsv_version EQUAL 1 OR NOT _hsv_profile STREQUAL "hsv_segmentation" OR
   NOT EXISTS "${_out}/seg-mask.png")
    message(FATAL_ERROR "HSV mask/stdout schema not produced as requested")
endif()

execute_process(COMMAND "${CHROMASCOPE_CLI}" --input "${CHROMASCOPE_INPUT}"
    --mode skittles --json-version 1 --json -
    RESULT_VARIABLE _code OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
if(NOT _code EQUAL 0)
    message(FATAL_ERROR "Legacy JSON v1 invocation failed: ${_stderr}")
endif()
string(JSON _legacy_version GET "${_stdout}" schema_version)
string(JSON _legacy_counts GET "${_stdout}" counts red)
string(JSON _legacy_profile GET "${_stdout}" profile)
if(NOT _legacy_version EQUAL 1 OR NOT _legacy_profile STREQUAL "skittles" OR
   _legacy_counts LESS 0)
    message(FATAL_ERROR "Legacy Skittles JSON fields were lost")
endif()

# Invalid commands must not create user-requested outputs or mutate source data.
file(SHA256 "${CHROMASCOPE_INPUT}" _before)
function(expect_rejection _case _fragment)
    execute_process(COMMAND "${CHROMASCOPE_CLI}" ${ARGN}
        RESULT_VARIABLE _code OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if("${_code}" STREQUAL "0")
        message(FATAL_ERROR "${_case}: invalid CLI command succeeded")
    endif()
    string(FIND "${_stderr}" "${_fragment}" _match)
    if(_match EQUAL -1)
        message(FATAL_ERROR "${_case}: expected '${_fragment}', received '${_stderr}'")
    endif()
endfunction()

expect_rejection(unknown_profile "Unknown processor profile"
    --input "${CHROMASCOPE_TEST_NO_INPUT}" --profile missing_profile)
expect_rejection(preflight_no_directory "Unknown processor profile"
    --input "${CHROMASCOPE_TEST_NO_INPUT}" --profile missing_profile
    --output-dir "${CHROMASCOPE_EXPORT_DIR}/should-not-exist" --output "out.png")
expect_rejection(unknown_option "Unknown option"
    --input "${CHROMASCOPE_TEST_NO_INPUT}" --profile gaussian_blur --set strange=3)
expect_rejection(cross_parameter "Canny"
    --input "${CHROMASCOPE_TEST_NO_INPUT}" --profile canny_edges
    --set low_threshold=200 --set high_threshold=100)
expect_rejection(nonfinite "Non-finite"
    --input "${CHROMASCOPE_INPUT}" --profile gaussian_blur --set sigma=nan)
expect_rejection(duplicate_profile "Repeated or conflicting"
    --input "${CHROMASCOPE_INPUT}" --profile grayscale --profile skittles)
expect_rejection(legacy_conflict "Conflicting legacy"
    --input "${CHROMASCOPE_INPUT}" --accumulator 20 --set accumulator_threshold=19)
expect_rejection(bad_schema "Unsupported JSON schema version"
    --input "${CHROMASCOPE_INPUT}" --json - --json-version 2)
expect_rejection(schema_without_json "requires --json"
    --input "${CHROMASCOPE_INPUT}" --json-version 1)
expect_rejection(input_overwrite "cannot overwrite the input image"
    --input "${CHROMASCOPE_INPUT}" --output "${CHROMASCOPE_INPUT}")
expect_rejection(output_collision "must use distinct paths"
    --input "${CHROMASCOPE_INPUT}" --output "${_out}/same.png" --json "${_out}/same.png")
expect_rejection(directory_as_file "must designate a file"
    --input "${CHROMASCOPE_INPUT}" --json "${_out}")
expect_rejection(mask_wrong_profile "--mask-output requires"
    --input "${CHROMASCOPE_INPUT}" --profile grayscale --mask-output "${_out}/wrong.png")
expect_rejection(output_dir_without_file "--output-dir requires"
    --input "${CHROMASCOPE_INPUT}" --output-dir "${CHROMASCOPE_EXPORT_DIR}/unused")
expect_rejection(bad_legacy_mode "--mode only supports"
    --input "${CHROMASCOPE_INPUT}" --mode grayscale)

file(SHA256 "${CHROMASCOPE_INPUT}" _after)
if(NOT _before STREQUAL _after)
    message(FATAL_ERROR "CLI negative path modified the source photograph")
endif()
if(EXISTS "${_out}/wrong.png" OR EXISTS "${_out}/same.png" OR
   EXISTS "${CHROMASCOPE_EXPORT_DIR}/unused" OR
   EXISTS "${CHROMASCOPE_EXPORT_DIR}/should-not-exist")
    message(FATAL_ERROR "Rejected CLI command created an output")
endif()
message(STATUS "PASS: M3.5 schema v1, profiles, output paths and negative cases")
