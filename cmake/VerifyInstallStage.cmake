# Hardware-free install/stage/relocate smoke. Do not execute an incomplete
# distribution: OpenCV/codec DLL and .so bundling is an explicit later gate.
foreach(_name CHROMASCOPE_BUILD_DIR CHROMASCOPE_SOURCE_DIR CHROMASCOPE_BINDIR
              CHROMASCOPE_DATADIR CHROMASCOPE_DOCDIR CHROMASCOPE_CONFIG)
    if(NOT DEFINED ${_name} OR "${${_name}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${_name}")
    endif()
endforeach()
set(_stage "${CHROMASCOPE_BUILD_DIR}/test-artifacts/m111-install")
set(_relocated "${CHROMASCOPE_BUILD_DIR}/test-artifacts/m111-relocated")
file(REMOVE_RECURSE "${_stage}" "${_relocated}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${CHROMASCOPE_BUILD_DIR}"
    --config "${CHROMASCOPE_CONFIG}" --prefix "${_stage}"
    RESULT_VARIABLE _install_status OUTPUT_VARIABLE _install_out ERROR_VARIABLE _install_err)
if(NOT _install_status EQUAL 0)
    message(FATAL_ERROR "M11.1 installation failed: ${_install_out}\n${_install_err}")
endif()
file(RENAME "${_stage}" "${_relocated}")
if(WIN32)
    set(_exe ".exe")
else()
    set(_exe "")
endif()
foreach(_file IN ITEMS "${CHROMASCOPE_BINDIR}/chromascope-cli${_exe}"
        "${CHROMASCOPE_DOCDIR}/LICENSE"
        "${CHROMASCOPE_DOCDIR}/README.md"
        "${CHROMASCOPE_DOCDIR}/docs/USER_GUIDE.md"
        "${CHROMASCOPE_DOCDIR}/docs/CLI_REFERENCE.md"
        "${CHROMASCOPE_DATADIR}/models/selected_model.json"
        "${CHROMASCOPE_DATADIR}/models/NANODET_NOTICES.md")
    if(NOT EXISTS "${_relocated}/${_file}")
        message(FATAL_ERROR "Missing staged file after relocation: ${_file}")
    endif()
endforeach()
# Packaging policy: only the explicitly approved current user guides are
# installed under docs/. The presence of one extra .md/.json is a failure,
# including maintainer sign-off, historical notebooks and provenance fixtures.
set(_expected_user_guides
    "INSTALLATION_1_0_0.md"
    "USER_GUIDE.md"
    "CLI_REFERENCE.md"
    "PRIVACY_GUIDE.md"
    "TROUBLESHOOTING.md"
    "RELEASE_EXAMPLES_1_0_0.md"
    "RELEASE_NOTES_1_0_0.md"
    "CHANGELOG.md"
    "IMAGE_FILTER_PROFILES.md"
    "SEGMENTATION_PROFILES.md"
    "MEDIA_RIGHTS.md"
    "RESOURCE_BUDGETS.md"
)
if(CHROMASCOPE_V101_PORTABLE_STAGE)
    list(REMOVE_ITEM _expected_user_guides
        "INSTALLATION_1_0_0.md" "RELEASE_EXAMPLES_1_0_0.md" "RELEASE_NOTES_1_0_0.md")
endif()
list(SORT _expected_user_guides)
set(_installed_doc_root "${_relocated}/${CHROMASCOPE_DOCDIR}/docs")
file(GLOB_RECURSE _doc_files RELATIVE "${_installed_doc_root}"
    LIST_DIRECTORIES FALSE "${_installed_doc_root}/*")
list(SORT _doc_files)
if(NOT "${_doc_files}" STREQUAL "${_expected_user_guides}")
    message(FATAL_ERROR
        "Installed user-documentation allowlist mismatch.\n"
        "Expected: ${_expected_user_guides}\nActual: ${_doc_files}")
endif()
if(EXISTS "${_relocated}/${CHROMASCOPE_DOCDIR}/docs/history" OR
   EXISTS "${_relocated}/${CHROMASCOPE_DOCDIR}/docs/M12_5_RELEASE_SECURITY_SIGNOFF.md" OR
   EXISTS "${_relocated}/${CHROMASCOPE_DOCDIR}/docs/fixture_provenance.json")
    message(FATAL_ERROR "Source-only engineering documents leaked into installation")
endif()
file(GLOB_RECURSE _staged_entries LIST_DIRECTORIES FALSE "${_relocated}/*")
set(_expected_model_path
    "${_relocated}/${CHROMASCOPE_DATADIR}/models/object_detection_nanodet_2022nov.onnx")
if(CHROMASCOPE_V101_PORTABLE_STAGE)
    if(NOT EXISTS "${_expected_model_path}" OR IS_SYMLINK "${_expected_model_path}")
        message(FATAL_ERROR "Missing approved v1.0.1 NanoDet model")
    endif()
    file(SHA256 "${_expected_model_path}" _model_sha256)
    if(NOT _model_sha256 STREQUAL
       "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186")
        message(FATAL_ERROR "v1.0.1 staged model SHA-256 mismatch")
    endif()
    foreach(_notice IN ITEMS LICENSE NANODET_NOTICES.md)
        if(NOT EXISTS "${_relocated}/${CHROMASCOPE_DATADIR}/models/${_notice}")
            message(FATAL_ERROR "Missing portable model license/notice: ${_notice}")
        endif()
    endforeach()
endif()
# Exact four-file v1.0.1 photo exception. Historical v1.0.0 still rejects
# all packaged photos, and other media remain prohibited.
set(_approved_skittles_paths "")
if(CHROMASCOPE_V101_PORTABLE_STAGE)
    set(_skittles_names s1.jpg s2.jpg s3.jpg s4.jpg)
    set(_skittles_hashes
        "515454a4a2fd8625f831baea9c2ca39d614877fbfb489be3c55333c8ce56b2aa"
        "9589016e0b67b5bfb40bf1a8bd8738651d0337a7777f20796c6ff3df53985a6e"
        "1b928939929d46cd6dc94489c4c16c94361c77c5d72d15eec500884bdec69491"
        "9d44d9c1b66dd748804620a610188bc1dff6ca1465cafaff1a3b06316e4f633c")
    foreach(_index RANGE 0 3)
        list(GET _skittles_names ${_index} _filename)
        list(GET _skittles_hashes ${_index} _expected_sha)
        set(_photo "${_relocated}/${CHROMASCOPE_DATADIR}/samples/skittles/${_filename}")
        if(NOT EXISTS "${_photo}" OR IS_SYMLINK "${_photo}")
            message(FATAL_ERROR "Missing or symlinked portable Skittles sample: ${_filename}")
        endif()
        file(SHA256 "${_photo}" _photo_sha)
        if(NOT _photo_sha STREQUAL _expected_sha)
            message(FATAL_ERROR "Modified portable Skittles sample: ${_filename}")
        endif()
        list(APPEND _approved_skittles_paths "${_photo}")
    endforeach()
endif()
foreach(_entry IN LISTS _staged_entries)
    if(CHROMASCOPE_V101_PORTABLE_STAGE AND _entry STREQUAL "${_expected_model_path}")
        continue()
    endif()
    if(CHROMASCOPE_V101_PORTABLE_STAGE AND _entry IN_LIST _approved_skittles_paths)
        continue()
    endif()
    if(_entry MATCHES "\\.(jpg|jpeg|png|mp4|avi|onnx|pth|pt)$")
        message(FATAL_ERROR "Unapproved or unevaluated media/model artifact staged: ${_entry}")
    endif()
endforeach()
if(EXISTS "${CHROMASCOPE_BUILD_DIR}/chromascope-gui${_exe}" AND
   NOT EXISTS "${_relocated}/${CHROMASCOPE_BINDIR}/chromascope-gui${_exe}")
    message(FATAL_ERROR "GUI target was built but not installed")
endif()
# This is a functional CLI startup check after the directory has moved, not
# evidence that the loader dependency closure is in the package.
set(_cli "${_relocated}/${CHROMASCOPE_BINDIR}/chromascope-cli${_exe}")
if(UNIX AND NOT APPLE)
    set(_loader_env "LD_LIBRARY_PATH=${CHROMASCOPE_SMOKE_LIBDIR}:$ENV{LD_LIBRARY_PATH}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "${_loader_env}" "${_cli}" --help
        WORKING_DIRECTORY "${_relocated}"
        RESULT_VARIABLE _cli_result OUTPUT_VARIABLE _cli_out ERROR_VARIABLE _cli_err)
else()
    execute_process(COMMAND "${_cli}" --help
        WORKING_DIRECTORY "${_relocated}"
        RESULT_VARIABLE _cli_result OUTPUT_VARIABLE _cli_out ERROR_VARIABLE _cli_err)
endif()
if(NOT _cli_result STREQUAL "0" OR NOT _cli_out MATCHES "Usage: chromascope-cli")
    message(FATAL_ERROR "Relocated CLI startup failed: ${_cli_result} / ${_cli_err} / ${_cli_out}")
endif()
message(STATUS "M11.1: relocate+CLI-help+layout/rights PASS (runtime bundling NOT qualified)")
