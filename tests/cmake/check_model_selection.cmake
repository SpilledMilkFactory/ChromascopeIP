# M5.1 (#31): offline, cross-platform model manifest/license release guard.
# This deliberately never downloads or executes third-party model weights.
if(NOT DEFINED CHROMASCOPE_SOURCE_ROOT OR CHROMASCOPE_SOURCE_ROOT STREQUAL "")
    message(FATAL_ERROR "CHROMASCOPE_SOURCE_ROOT not supplied")
endif()
set(_manifest "${CHROMASCOPE_SOURCE_ROOT}/models/selected_model.json")
set(_license "${CHROMASCOPE_SOURCE_ROOT}/licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt")
set(_notices "${CHROMASCOPE_SOURCE_ROOT}/models/NANODET_NOTICES.md")
foreach(_file IN ITEMS "${_manifest}" "${_license}" "${_notices}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "M5.1 model policy missing required file: ${_file}")
    endif()
endforeach()
file(READ "${_manifest}" _json)
function(model_field name expected)
    string(JSON _value ERROR_VARIABLE _error GET "${_json}" "${name}")
    if(NOT _error STREQUAL "NOTFOUND" OR NOT _value STREQUAL "${expected}")
        message(FATAL_ERROR "M5.1 invalid manifest ${name}: ${_value} (${_error})")
    endif()
endfunction()
model_field(schema_version "1")
model_field(model_id "opencv-zoo-nanodet-plus-m-1.5x-416-fp32-2022nov")
model_field(file_name "object_detection_nanodet_2022nov.onnx")
model_field(artifact_sha256 "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186")
model_field(license_spdx "Apache-2.0")
model_field(license_text_path "licenses/OpenCV-Zoo-NanoDet-Apache-2.0.txt")
model_field(notice_path "models/NANODET_NOTICES.md")
model_field(format "ONNX")
model_field(precision "fp32")
model_field(class_count "80")
model_field(artifact_identity_policy "verify_sha256; upstream main is a mutable locator")
model_field(release_redistribution_status "requires_artifact_notices_and_final_release_review")
model_field(inference_runtime_status "not_implemented_in_m5_1")
model_field(artifact_source_url "https://huggingface.co/opencv/opencv_zoo/resolve/main/models/object_detection_nanodet/object_detection_nanodet_2022nov.onnx")
string(JSON _height ERROR_VARIABLE _height_err GET "${_json}" input_size_hw 0)
string(JSON _width ERROR_VARIABLE _width_err GET "${_json}" input_size_hw 1)
if(NOT _height STREQUAL "416" OR NOT _width STREQUAL "416" OR
   NOT _height_err STREQUAL "NOTFOUND" OR NOT _width_err STREQUAL "NOTFOUND")
    message(FATAL_ERROR "M5.1 input size must be 416x416")
endif()
string(JSON _bundled ERROR_VARIABLE _bundle_err GET "${_json}" model_bundled)
if(NOT _bundle_err STREQUAL "NOTFOUND" OR _bundled)
    message(FATAL_ERROR "M5.1 must not bundle model weights")
endif()
file(READ "${_license}" _license_text)
if(NOT _license_text MATCHES "Apache License" OR
   NOT _license_text MATCHES "Version 2.0, January 2004" OR
   NOT _license_text MATCHES "4. Redistribution" OR
   NOT _license_text MATCHES "END OF TERMS AND CONDITIONS")
    message(FATAL_ERROR "M5.1 required full Apache License 2.0 text missing or incomplete")
endif()
file(READ "${_notices}" _notice_text)
foreach(_required IN ITEMS "OpenCV" "RangiLyu" "Sri Siddarth Chakaravarthy" "Apache" "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186")
    string(FIND "${_notice_text}" "${_required}" _index)
    if(_index LESS 0)
        message(FATAL_ERROR "M5.1 attribution missing: ${_required}")
    endif()
endforeach()
# M5.6 clarification: a local, ignored and SHA-256-verified checkpoint may
# live beside the manifest. "model_bundled=false" forbids Git-tracked weights,
# not a developer's untracked working-copy file. Fail closed for source
# archives that contain binaries without Git metadata.
set(_approved_file "${CHROMASCOPE_SOURCE_ROOT}/models/object_detection_nanodet_2022nov.onnx")
file(GLOB _weights LIST_DIRECTORIES FALSE
    "${CHROMASCOPE_SOURCE_ROOT}/models/*.onnx"
    "${CHROMASCOPE_SOURCE_ROOT}/models/*.pt"
    "${CHROMASCOPE_SOURCE_ROOT}/models/*.pth"
    "${CHROMASCOPE_SOURCE_ROOT}/models/*.bin")
set(_git_dir "${CHROMASCOPE_SOURCE_ROOT}/.git")
if(EXISTS "${_git_dir}")
    find_program(_git_executable NAMES git)
    if(NOT _git_executable)
        message(FATAL_ERROR "M5.1 policy cannot verify Git-tracked model files: git missing")
    endif()
    # Debian containers can run as root against a checkout owned by the host.
    # Trust this exact checkout for this command only, not every directory.
    execute_process(
        COMMAND "${_git_executable}" -c "safe.directory=${CHROMASCOPE_SOURCE_ROOT}"
            -C "${CHROMASCOPE_SOURCE_ROOT}" ls-files --cached -- models
        RESULT_VARIABLE _git_result OUTPUT_VARIABLE _tracked_files ERROR_VARIABLE _git_error)
    if(NOT _git_result EQUAL 0)
        message(FATAL_ERROR "M5.1 cannot inspect Git-tracked model files: ${_git_error}")
    endif()
    string(REPLACE "\n" ";" _tracked_list "${_tracked_files}")
    foreach(_tracked IN LISTS _tracked_list)
        if(_tracked MATCHES "\\.(onnx|pt|pth|bin)$")
            message(FATAL_ERROR "M5.1 model weights must not be Git-tracked: ${_tracked}")
        endif()
    endforeach()
elseif(_weights)
    message(FATAL_ERROR "M5.1 source archive contains model weights without Git provenance: ${_weights}")
endif()
foreach(_weight IN LISTS _weights)
    if(NOT _weight STREQUAL "${_approved_file}")
        message(FATAL_ERROR "M5.1 unapproved local model artifact: ${_weight}")
    endif()
    execute_process(
        COMMAND "${_git_executable}" -c "safe.directory=${CHROMASCOPE_SOURCE_ROOT}"
            -C "${CHROMASCOPE_SOURCE_ROOT}" check-ignore --quiet --
            models/object_detection_nanodet_2022nov.onnx
        RESULT_VARIABLE _ignored_result)
    if(NOT _ignored_result EQUAL 0)
        message(FATAL_ERROR "M5.1 local ONNX is not Git-ignored: ${_weight}")
    endif()
    file(SHA256 "${_weight}" _local_sha256)
    if(NOT _local_sha256 STREQUAL "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186")
        message(FATAL_ERROR "M5.1 local ONNX checksum mismatch: ${_weight}")
    endif()
endforeach()
message(STATUS "PASS: M5.1 NanoDet identity, Apache-2.0 notices and no-bundling policy")
