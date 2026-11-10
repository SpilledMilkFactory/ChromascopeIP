# Keep each verification layer independently runnable in CTest and CI.
# The historical JPEGs remain in samples/skittles; no duplicate media copies.
set(CHROMASCOPE_TEST_FIXTURE_ROOT "${PROJECT_SOURCE_DIR}/samples/skittles")

function(chromascope_add_cpp_test name source labels timeout_seconds)
    add_executable(${name} "${source}")
    target_link_libraries(${name} PRIVATE ChromascopeIP::Core)
    chromascope_apply_quality(${name})
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${labels}" TIMEOUT ${timeout_seconds})
endfunction()

chromascope_add_cpp_test(chromascope_unit_color
    unit/test_color.cpp "unit;core" 20)
chromascope_add_cpp_test(chromascope_unit_validation
    unit/test_validation.cpp "unit;core;negative" 20)
chromascope_add_cpp_test(chromascope_unit_image_frame
    unit/test_image_frame.cpp "unit;core;frame;negative" 20)
chromascope_add_cpp_test(chromascope_unit_detection_result
    unit/test_detection_result.cpp "unit;core;detection;negative" 20)
chromascope_add_cpp_test(chromascope_integration_detection_result
    integration/test_detection_result.cpp "integration;core;detection" 40)
chromascope_add_cpp_test(chromascope_integration_frame_cv
    integration/test_frame_cv.cpp "integration;core;frame" 40)
chromascope_add_cpp_test(chromascope_integration_synthetic
    integration/test_synthetic.cpp "integration;core;synthetic" 40)
chromascope_add_cpp_test(chromascope_regression_skittles
    regression/test_skittles.cpp "regression;core;fixtures;samples" 90)
target_compile_definitions(chromascope_regression_skittles PRIVATE
    CHROMASCOPE_FIXTURES_DIR="${CHROMASCOPE_TEST_FIXTURE_ROOT}")

add_test(NAME chromascope_fixture_integrity COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_FIXTURE_DIR=${CHROMASCOPE_TEST_FIXTURE_ROOT}"
    "-DCHROMASCOPE_FIXTURE_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/skittles.sha256"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_fixtures.cmake")
set_tests_properties(chromascope_fixture_integrity PROPERTIES
    LABELS "fixtures;regression" TIMEOUT 15)

# Preserve the established CLI smoke and negative-path tests with stable names.
add_test(NAME chromascope_cli_help COMMAND chromascope-cli --help)
set_tests_properties(chromascope_cli_help PROPERTIES
    LABELS "cli;smoke" TIMEOUT 15
    PASS_REGULAR_EXPRESSION "Usage: chromascope-cli")

add_test(NAME chromascope_cli_sample COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg")
set_tests_properties(chromascope_cli_sample PROPERTIES
    LABELS "cli;integration;fixtures" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "Detected: [0-9]+")

add_test(NAME chromascope_cli_bad_input COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/absent.jpg")
set_tests_properties(chromascope_cli_bad_input PROPERTIES
    WILL_FAIL TRUE LABELS "cli;negative" TIMEOUT 15)

add_test(NAME chromascope_cli_exports COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/cli-exports"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_exports.cmake")
set_tests_properties(chromascope_cli_exports PROPERTIES
    LABELS "cli;integration;fixtures" TIMEOUT 90)

# M2.3 registered processing interface, including CLI profile discovery.
chromascope_add_cpp_test(chromascope_unit_processor_registry
    unit/test_processor_registry.cpp "unit;core;processor;negative" 20)
chromascope_add_cpp_test(chromascope_integration_processor_skittles
    integration/test_processor_skittles.cpp "integration;core;processor" 50)

add_test(NAME chromascope_cli_list_profiles COMMAND chromascope-cli --list-profiles)
set_tests_properties(chromascope_cli_list_profiles PROPERTIES
    LABELS "cli;processor;smoke" TIMEOUT 15
    PASS_REGULAR_EXPRESSION "skittles - Skittles")

add_test(NAME chromascope_cli_bad_profile COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --profile unknown)
set_tests_properties(chromascope_cli_bad_profile PROPERTIES
    WILL_FAIL TRUE LABELS "cli;processor;negative" TIMEOUT 15)
