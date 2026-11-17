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
file(GLOB_RECURSE _staged_entries LIST_DIRECTORIES FALSE "${_relocated}/*")
foreach(_entry IN LISTS _staged_entries)
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
