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

# M2.4: bounded worker queue and real OpenCV asynchronous regression.
chromascope_add_cpp_test(chromascope_unit_processing_queue
    unit/test_processing_queue.cpp "unit;core;async;negative" 20)
chromascope_add_cpp_test(chromascope_integration_processing_async
    integration/test_processing_async.cpp "integration;core;async;fixtures" 50)
target_compile_definitions(chromascope_integration_processing_async PRIVATE
    CHROMASCOPE_ASYNC_SAMPLE="${CHROMASCOPE_TEST_FIXTURE_ROOT}/s1.jpg")

# M2.5: detection-to-annotation boundary and pixel-equivalent CPU renderer.
chromascope_add_cpp_test(chromascope_unit_annotation
    unit/test_annotation.cpp "unit;core;annotation;negative" 20)
chromascope_add_cpp_test(chromascope_integration_annotation_renderer
    integration/test_annotation_renderer.cpp "integration;core;annotation;fixtures;regression" 90)
target_compile_definitions(chromascope_integration_annotation_renderer PRIVATE
    CHROMASCOPE_ANNOTATION_SAMPLES="${CHROMASCOPE_TEST_FIXTURE_ROOT}")

# M2.6: predecode image limits, owned-frame budgets and processing guards.
chromascope_add_cpp_test(chromascope_unit_resource_budget
    unit/test_resource_budget.cpp "unit;core;security;budget;negative" 25)
chromascope_add_cpp_test(chromascope_integration_resource_limits
    integration/test_resource_limits.cpp "integration;core;security;budget;negative;fixtures" 90)
target_compile_definitions(chromascope_integration_resource_limits PRIVATE
    CHROMASCOPE_RESOURCE_SAMPLES="${CHROMASCOPE_TEST_FIXTURE_ROOT}")

# CLI parsers must reject trailing garbage rather than accepting numeric prefixes.
add_test(NAME chromascope_cli_bad_dimension_suffix COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --max-dimension 1200junk)
set_tests_properties(chromascope_cli_bad_dimension_suffix PROPERTIES
    WILL_FAIL TRUE LABELS "cli;security;budget;negative" TIMEOUT 15)
add_test(NAME chromascope_cli_bad_threshold_suffix COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --accumulator 31junk)
set_tests_properties(chromascope_cli_bad_threshold_suffix PROPERTIES
    WILL_FAIL TRUE LABELS "cli;security;budget;negative" TIMEOUT 15)

# M2.7: concurrency, lifetime, stored-coordinate and legacy compatibility gates.
# These run headlessly on every supported CI platform; no camera is required.
chromascope_add_cpp_test(chromascope_unit_processing_lifecycle
    unit/test_processing_lifecycle.cpp "unit;core;async;lifecycle;concurrency;negative" 25)
chromascope_add_cpp_test(chromascope_integration_concurrent_requests
    integration/test_concurrent_requests.cpp "integration;core;async;concurrency;lifecycle" 30)
chromascope_add_cpp_test(chromascope_unit_coordinate_contract
    unit/test_coordinate_contract.cpp "unit;core;coordinate;lifecycle" 20)
chromascope_add_cpp_test(chromascope_integration_coordinate_cv
    integration/test_coordinate_cv.cpp "integration;core;coordinate;annotation;frame" 25)
chromascope_add_cpp_test(chromascope_regression_legacy_compatibility
    regression/test_legacy_compatibility.cpp "regression;core;compatibility;fixtures;annotation" 90)
target_compile_definitions(chromascope_regression_legacy_compatibility PRIVATE
    CHROMASCOPE_LEGACY_SAMPLE="${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg")

# M3.1: generalized still-image filters; existing Skittles tests are unchanged.
chromascope_add_cpp_test(chromascope_unit_image_filter_contract
    unit/test_image_filter_contract.cpp "unit;core;processor;filters;negative" 20)
chromascope_add_cpp_test(chromascope_integration_image_filters
    integration/test_image_filters.cpp "integration;core;processor;filters;image" 40)
add_test(NAME chromascope_cli_image_filters COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/image-filters"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_image_filters.cmake")
set_tests_properties(chromascope_cli_image_filters PROPERTIES
    LABELS "cli;integration;filters;fixtures" TIMEOUT 90)
add_test(NAME chromascope_cli_bad_filter_option COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --profile gaussian_blur --set kernel_size=4)
set_tests_properties(chromascope_cli_bad_filter_option PROPERTIES
    WILL_FAIL TRUE LABELS "cli;filters;negative" TIMEOUT 15)
add_test(NAME chromascope_cli_bad_filter_setting COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --profile grayscale --set not_an_option=1)
set_tests_properties(chromascope_cli_bad_filter_setting PROPERTIES
    WILL_FAIL TRUE LABELS "cli;filters;negative" TIMEOUT 15)

# M3.2: HSV color segmentation and contour geometry, CLI/GUI shared contract.
chromascope_add_cpp_test(chromascope_unit_segmentation_contract
    unit/test_segmentation_contract.cpp "unit;core;processor;segmentation;negative" 25)
chromascope_add_cpp_test(chromascope_integration_hsv_segmentation
    integration/test_hsv_segmentation.cpp "integration;core;segmentation;annotation" 40)
chromascope_add_cpp_test(chromascope_integration_contour_detection
    integration/test_contour_detection.cpp "integration;core;segmentation;annotation" 40)
add_test(NAME chromascope_cli_segmentation COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s1.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/segmentation"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_segmentation.cmake")
set_tests_properties(chromascope_cli_segmentation PROPERTIES
    LABELS "cli;integration;segmentation;fixtures" TIMEOUT 90)
add_test(NAME chromascope_cli_bad_hsv_range COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --profile hsv_segmentation
    --set min_saturation=220 --set max_saturation=120)
set_tests_properties(chromascope_cli_bad_hsv_range PROPERTIES
    WILL_FAIL TRUE LABELS "cli;negative;segmentation" TIMEOUT 15)
add_test(NAME chromascope_cli_bad_contour_kernel COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --profile contour_detection
    --set blur_kernel=4)
set_tests_properties(chromascope_cli_bad_contour_kernel PROPERTIES
    WILL_FAIL TRUE LABELS "cli;negative;segmentation" TIMEOUT 15)
add_test(NAME chromascope_cli_bad_mask_profile COMMAND chromascope-cli
    --input "${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg" --profile grayscale
    --mask-output "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/should-not-be-created.png")
set_tests_properties(chromascope_cli_bad_mask_profile PROPERTIES
    WILL_FAIL TRUE LABELS "cli;negative;segmentation" TIMEOUT 15)

# M3.3: overlay visibility, independent selection and source/processed GUI views.
chromascope_add_cpp_test(chromascope_unit_annotation_view
    unit/test_annotation_view.cpp "unit;core;annotation;overlay;gui-contract" 20)
chromascope_add_cpp_test(chromascope_integration_annotation_view_render
    integration/test_annotation_view_render.cpp "integration;core;annotation;overlay;gui-contract" 40)

# M3.4: DPI-independent original-pixel viewport mapping (no GUI dependency).
chromascope_add_cpp_test(chromascope_unit_image_viewport
    unit/test_image_viewport.cpp "unit;core;viewport;coordinate;gui-contract" 20)
chromascope_add_cpp_test(chromascope_integration_viewport_comparison
    integration/test_viewport_comparison.cpp "integration;core;viewport;coordinate;gui-contract" 20)

# M3.5: CLI output isolation and end-to-end JSON/profile/argument contract.
chromascope_add_cpp_test(chromascope_unit_cli_output_paths
    unit/test_cli_output_paths.cpp "unit;cli;security;negative" 20)
target_include_directories(chromascope_unit_cli_output_paths PRIVATE
    "${PROJECT_SOURCE_DIR}/src/cli")
add_test(NAME chromascope_cli_m3_5_contract COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_TEST_NO_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/does-not-exist.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/cli-m3-5"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_m3_5.cmake")
set_tests_properties(chromascope_cli_m3_5_contract PROPERTIES
    LABELS "cli;integration;negative;security;regression;fixtures" TIMEOUT 120)

# M3.6: independent header, real-codec, archival-fixture, CLI/error and
# integrity-gate regression checks. Test-generated media stays in build/.
chromascope_add_cpp_test(chromascope_unit_still_image_headers
    unit/test_still_image_headers.cpp "unit;core;formats;security;negative" 25)
add_executable(chromascope_integration_still_image_formats
    integration/test_still_image_formats.cpp)
target_link_libraries(chromascope_integration_still_image_formats PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_still_image_formats)
target_include_directories(chromascope_integration_still_image_formats PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME chromascope_integration_still_image_formats
    COMMAND chromascope_integration_still_image_formats
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m3-6-formats")
set_tests_properties(chromascope_integration_still_image_formats PROPERTIES
    LABELS "integration;core;formats;security;negative" TIMEOUT 45)
chromascope_add_cpp_test(chromascope_regression_still_image_fixtures
    regression/test_still_image_fixtures.cpp "regression;core;formats;fixtures" 90)
target_compile_definitions(chromascope_regression_still_image_fixtures PRIVATE
    CHROMASCOPE_FIXTURES_DIR="${CHROMASCOPE_TEST_FIXTURE_ROOT}")
add_test(NAME chromascope_cli_still_image_errors COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m3-6-cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_still_images.cmake")
set_tests_properties(chromascope_cli_still_image_errors PROPERTIES
    LABELS "cli;integration;formats;negative;fixtures;security" TIMEOUT 60)
add_test(NAME chromascope_fixture_tamper_regression COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_FIXTURE_DIR=${CHROMASCOPE_TEST_FIXTURE_ROOT}"
    "-DCHROMASCOPE_FIXTURE_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/skittles.sha256"
    "-DCHROMASCOPE_FIXTURE_VERIFY=${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_fixtures.cmake"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m3-6-manifest"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_fixture_regressions.cmake")
set_tests_properties(chromascope_fixture_tamper_regression PROPERTIES
    LABELS "regression;formats;fixtures;security;negative" TIMEOUT 45)

# M4.1: independent codec-free-sample OpenCV videoio smoke test.
# The same translation unit also verifies isolated installed packages.
chromascope_add_cpp_test(chromascope_integration_videoio_backend
    integration/test_videoio_backend.cpp "integration;videoio;dependency;formats;regression" 50)

# M4.2: sequential recorded-file source, owned BGR8 frames, presentation
# timestamps, indexed delivery, explicit and repeatable end-of-stream.
chromascope_add_cpp_test(chromascope_unit_video_source_contract
    unit/test_video_source_contract.cpp "unit;core;video;frame;negative" 20)
add_executable(chromascope_integration_video_source
    integration/test_video_source.cpp)
target_link_libraries(chromascope_integration_video_source PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_video_source)
add_test(NAME chromascope_integration_video_source
    COMMAND chromascope_integration_video_source
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-2-video-source")
set_tests_properties(chromascope_integration_video_source PROPERTIES
    LABELS "integration;core;video;videoio;formats;negative;regression" TIMEOUT 60)

# M4.3: background recorded-file decoding with a bounded frame/byte queue,
# latest-generation seek and cancellation, and terminal EOF/error signaling.
chromascope_add_cpp_test(chromascope_unit_async_video_decoder
    unit/test_async_video_decoder.cpp "unit;core;video;async;negative;budget" 25)
add_executable(chromascope_integration_async_video_decoder
    integration/test_async_video_decoder.cpp)
target_link_libraries(chromascope_integration_async_video_decoder PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_async_video_decoder)
add_test(NAME chromascope_integration_async_video_decoder
    COMMAND chromascope_integration_async_video_decoder
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-3-async-video")
set_tests_properties(chromascope_integration_async_video_decoder PROPERTIES
    LABELS "integration;core;video;videoio;async;concurrency;lifecycle;budget;regression;negative"
    TIMEOUT 75)

# M4.4: headless GUI-player transport contract and real MJPEG recorded playback.
chromascope_add_cpp_test(chromascope_unit_video_player_contract
    unit/test_video_player_contract.cpp "unit;core;video;gui-contract;negative;lifecycle" 25)
add_executable(chromascope_integration_video_player integration/test_video_player.cpp)
target_link_libraries(chromascope_integration_video_player PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_video_player)
add_test(NAME chromascope_integration_video_player
    COMMAND chromascope_integration_video_player
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-4-video-player")
set_tests_properties(chromascope_integration_video_player PROPERTIES
    LABELS "integration;core;video;videoio;gui-contract;regression;lifecycle;async;negative"
    TIMEOUT 90)

# M4.5: strict video frame-index and lossless-PNG CLI contracts; real
# recorded-file probe and pixel-exact indexed extraction via synthetic MJPEG.
chromascope_add_cpp_test(chromascope_unit_cli_video_contract
    unit/test_cli_video_contract.cpp "unit;core;cli;video;negative" 20)
target_include_directories(chromascope_unit_cli_video_contract PRIVATE
    "${PROJECT_SOURCE_DIR}/src/cli")
add_executable(chromascope_cli_video_fixture integration/video_cli_fixture.cpp)
target_link_libraries(chromascope_cli_video_fixture PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_cli_video_fixture)
add_test(NAME chromascope_cli_video_probe_extract COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_VIDEO_FIXTURE=$<TARGET_FILE:chromascope_cli_video_fixture>"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-5-cli-video"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_video.cmake")
set_tests_properties(chromascope_cli_video_probe_extract PROPERTIES
    LABELS "cli;video;videoio;integration;regression;negative;security;formats"
    TIMEOUT 90)

# M4.6: rotation metadata, VFR position pacing, absent FPS and explicit
# video-only audio handling; no third-party media assets are distributed.
chromascope_add_cpp_test(chromascope_unit_video_media_policy
    unit/test_video_media_policy.cpp "unit;core;video;timing;rotation;negative" 20)
add_executable(chromascope_integration_video_media integration/test_video_media.cpp)
target_link_libraries(chromascope_integration_video_media PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_video_media)
target_include_directories(chromascope_integration_video_media PRIVATE
    "${PROJECT_SOURCE_DIR}/src/core")
add_test(NAME chromascope_integration_video_media
    COMMAND chromascope_integration_video_media
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-6-media")
set_tests_properties(chromascope_integration_video_media PROPERTIES
    LABELS "integration;core;video;videoio;rotation;timing;regression" TIMEOUT 50)
add_test(NAME chromascope_cli_video_media_contract COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_VIDEO_FIXTURE=$<TARGET_FILE:chromascope_cli_video_fixture>"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-6-cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_video_media.cmake")
set_tests_properties(chromascope_cli_video_media_contract PROPERTIES
    LABELS "cli;integration;video;videoio;rotation;timing;regression" TIMEOUT 60)

# M4.7 (#30): in-tree, generated video fixtures and deterministic error-path
# regressions. Test artifacts are isolated in each CTest build tree.
chromascope_add_cpp_test(chromascope_unit_video_timing_boundaries
    unit/test_video_timing_boundaries.cpp
    "unit;core;video;timing;rotation;regression;negative" 20)

add_executable(chromascope_regression_video_decoder_resilience
    regression/test_video_decoder_resilience.cpp)
target_link_libraries(chromascope_regression_video_decoder_resilience
    PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_regression_video_decoder_resilience)
add_test(NAME chromascope_regression_video_decoder_resilience
    COMMAND chromascope_regression_video_decoder_resilience
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-7-video-resilience")
set_tests_properties(chromascope_regression_video_decoder_resilience PROPERTIES
    LABELS "regression;integration;video;videoio;formats;negative;lifecycle;timing;fixtures"
    TIMEOUT 60)

add_test(NAME chromascope_cli_video_error_paths COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_VIDEO_FIXTURE=$<TARGET_FILE:chromascope_cli_video_fixture>"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m4-7-cli-errors"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_video_error_paths.cmake")
set_tests_properties(chromascope_cli_video_error_paths PROPERTIES
    LABELS "cli;integration;regression;video;videoio;negative;security;fixtures"
    TIMEOUT 90)

# M5.1 (#31): research/attribution only. Offline CMake policy gate: no
# model binaries or optional Python/ONNX runtime are required by CTest.
add_test(NAME chromascope_model_selection_policy COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_model_selection.cmake")
set_tests_properties(chromascope_model_selection_policy PROPERTIES
    LABELS "model;license;security;regression;negative" TIMEOUT 15)

# M5.2 (#32): strict tensor/invalid-model contract and real CPU DNN forward
# using a tiny generated weight-free ONNX graph in the build tree.
chromascope_add_cpp_test(chromascope_unit_cpu_dnn_contract
    unit/test_cpu_dnn_contract.cpp "unit;model;dnn;cpu;security;negative" 30)
add_executable(chromascope_integration_cpu_dnn_adapter
    integration/test_cpu_dnn_adapter.cpp)
target_link_libraries(chromascope_integration_cpu_dnn_adapter PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_cpu_dnn_adapter)
add_test(NAME chromascope_integration_cpu_dnn_adapter
    COMMAND chromascope_integration_cpu_dnn_adapter
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m5-2-cpu-dnn")
set_tests_properties(chromascope_integration_cpu_dnn_adapter PROPERTIES
    LABELS "integration;model;dnn;cpu;security;regression;negative" TIMEOUT 120)

# M5.3 (#33): deterministic geometry and actual OpenCV resize/normalize/letterbox.
chromascope_add_cpp_test(chromascope_unit_dnn_preprocess_geometry
    unit/test_dnn_preprocess_geometry.cpp "unit;model;dnn;preprocessing;negative" 30)
add_executable(chromascope_integration_dnn_preprocessing
    integration/test_dnn_preprocessing.cpp)
target_link_libraries(chromascope_integration_dnn_preprocessing PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_dnn_preprocessing)
add_test(NAME chromascope_integration_dnn_preprocessing
    COMMAND chromascope_integration_dnn_preprocessing)
set_tests_properties(chromascope_integration_dnn_preprocessing PROPERTIES
    LABELS "integration;model;dnn;preprocessing;regression;negative" TIMEOUT 40)

# M5.4 (#34): contiguous NanoDet COCO80 labels, bounded confidence gating,
# deterministic class-aware NMS, and owned source-space DetectionResult output.
chromascope_add_cpp_test(chromascope_unit_dnn_postprocess_policy
    unit/test_dnn_postprocess_policy.cpp
    "unit;core;model;dnn;postprocessing;negative;regression" 30)
chromascope_add_cpp_test(chromascope_integration_dnn_postprocess_result
    integration/test_dnn_postprocess_result.cpp
    "integration;core;model;dnn;postprocessing;regression;negative" 40)

# M5.5 (#35): model integrity, GUI inference-state lifecycle and raw CPU
# inference controls. No pretrained model weights or GUI context are needed.
add_executable(chromascope_unit_dnn_gui_policy unit/test_dnn_gui_policy.cpp)
target_link_libraries(chromascope_unit_dnn_gui_policy PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_unit_dnn_gui_policy)
target_include_directories(chromascope_unit_dnn_gui_policy PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME chromascope_unit_dnn_gui_policy
    COMMAND chromascope_unit_dnn_gui_policy
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m5-5-hash-vectors")
set_tests_properties(chromascope_unit_dnn_gui_policy PROPERTIES
    LABELS "unit;gui-contract;model;dnn;security;negative;regression" TIMEOUT 30)
add_executable(chromascope_integration_dnn_gui_session
    integration/test_dnn_gui_session.cpp)
target_link_libraries(chromascope_integration_dnn_gui_session PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_dnn_gui_session)
target_include_directories(chromascope_integration_dnn_gui_session PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME chromascope_integration_dnn_gui_session
    COMMAND chromascope_integration_dnn_gui_session
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m5-5-gui-session")
set_tests_properties(chromascope_integration_dnn_gui_session PROPERTIES
    LABELS "integration;gui-contract;model;dnn;cpu;security;regression;negative"
    TIMEOUT 90)

# M5.6 (#36): real COCO-80 NanoDet box decode and CLI safety/validation.
chromascope_add_cpp_test(chromascope_unit_nanodet_decoder
    unit/test_nanodet_decoder.cpp
    "unit;core;model;dnn;postprocessing;regression;negative" 30)
add_test(NAME chromascope_cli_object_error_paths COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m5-6-object-errors"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_object.cmake")
set_tests_properties(chromascope_cli_object_error_paths PROPERTIES
    LABELS "cli;integration;model;dnn;security;negative;regression" TIMEOUT 90)

# M5.7 (#37): pinned model provenance, original synthetic image/error fixtures,
# and bounded adversarial decoder behavior. No pretrained weights needed.
add_test(NAME chromascope_model_object_fixture_provenance COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_object_fixtures.cmake")
set_tests_properties(chromascope_model_object_fixture_provenance PROPERTIES
    LABELS "model;fixtures;security;provenance;regression" TIMEOUT 20)
chromascope_add_cpp_test(chromascope_unit_object_adversarial
    unit/test_object_adversarial.cpp
    "unit;core;model;dnn;security;negative;regression" 40)
add_executable(chromascope_integration_object_fixtures
    integration/test_object_fixtures.cpp)
target_link_libraries(chromascope_integration_object_fixtures PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_object_fixtures)
add_test(NAME chromascope_integration_object_fixtures
    COMMAND chromascope_integration_object_fixtures
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/object_detection")
set_tests_properties(chromascope_integration_object_fixtures PROPERTIES
    LABELS "integration;model;fixtures;security;negative;regression" TIMEOUT 45)

# M5.8: conservative COCO confidence policy, per-class overrides, explicit
# weak-candidate diagnostics, annotation scores, and CLI rejection paths.
chromascope_add_cpp_test(chromascope_unit_dnn_confidence_policy
    unit/test_dnn_confidence_policy.cpp
    "unit;model;dnn;postprocessing;security;negative;regression" 30)
add_test(NAME chromascope_cli_object_confidence_contract COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m5-8-cli-policy"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_object_confidence.cmake")
set_tests_properties(chromascope_cli_object_confidence_contract PROPERTIES
    LABELS "cli;model;dnn;postprocessing;negative;regression" TIMEOUT 90)

# M6.1 (#38): identical still/video NanoDet postprocessing, owned per-frame
# identity/timing, and explicit single-frame CLI command (no model weights).
chromascope_add_cpp_test(chromascope_unit_recorded_object_policy
    unit/test_recorded_object_policy.cpp
    "unit;model;dnn;video;postprocessing;negative;regression" 40)
add_executable(chromascope_integration_recorded_object_frames
    integration/test_recorded_object_frames.cpp)
target_link_libraries(chromascope_integration_recorded_object_frames PRIVATE ChromascopeIP::Core)
chromascope_apply_quality(chromascope_integration_recorded_object_frames)
target_include_directories(chromascope_integration_recorded_object_frames PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME chromascope_integration_recorded_object_frames
    COMMAND chromascope_integration_recorded_object_frames
        "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m6-1-recorded-objects")
set_tests_properties(chromascope_integration_recorded_object_frames PROPERTIES
    LABELS "integration;model;dnn;video;fixtures;timing;postprocessing;regression" TIMEOUT 60)
add_test(NAME chromascope_cli_video_object_contract COMMAND "${CMAKE_COMMAND}"
    "-DCHROMASCOPE_CLI=$<TARGET_FILE:chromascope-cli>"
    "-DCHROMASCOPE_INPUT=${CHROMASCOPE_TEST_FIXTURE_ROOT}/s2.jpg"
    "-DCHROMASCOPE_EXPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/m6-1-cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_cli_video_object.cmake")
set_tests_properties(chromascope_cli_video_object_contract PROPERTIES
    LABELS "cli;video;model;dnn;negative;security;regression" TIMEOUT 60)
