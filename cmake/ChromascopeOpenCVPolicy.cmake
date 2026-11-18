# Require the actual isolated OpenCV dependency to have been built with both
# reviewed Linux security patches. Reconfiguring ChromascopeIP does NOT rebuild
# the already-installed OpenCV shared libraries, even if CMake relinks targets.
function(chromascope_require_linux_opencv_policy opencv_config_dir)
    get_filename_component(_opencv_prefix "${opencv_config_dir}/../../.." ABSOLUTE)
    set(_stamp "${_opencv_prefix}/.chromascope-opencv-videoio-build.txt")
    set(_revision_file "${_opencv_prefix}/.chromascope-opencv-source-revision")
    set(_pinned_revision "e4f78fbcf1a773fbed69e14500e8b734963eb585")
    set(_patch_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../scripts/opencv_linux_patches")
    file(SHA256 "${_patch_dir}/disable-magicyuv.patch" _original_patch_sha)
    file(SHA256 "${_patch_dir}/ffmpeg-final-policy.patch" _final_patch_sha)

    set(_rebuild_hint
        "Rebuild OpenCV into a NEW, empty install prefix before configuring ChromascopeIP.\n"
        "  export OPENCV_INSTALL_PREFIX=\"\$HOME/.local/opt/opencv-5.1.0-m125\"\n"
        "  export OPENCV_BUILD_WORKSPACE=\"\$HOME/.cache/chromascope/opencv-5.1.0-m125\"\n"
        "  ./scripts/bootstrap_opencv_ubuntu_26_04.sh\n"
        "  cmake --preset linux-release -DOpenCV_DIR=\"\$OPENCV_INSTALL_PREFIX/lib/cmake/opencv5\"\n"
        "Use the same -DOpenCV_DIR for linux-cli-release and linux-cli-sanitize.")
    string(JOIN "" _rebuild_hint ${_rebuild_hint})

    if(NOT EXISTS "${_stamp}" OR NOT EXISTS "${_revision_file}")
        message(FATAL_ERROR
            "ChromascopeIP selected an OpenCV installation without verified M12.5 "
            "FFmpeg security-policy provenance: ${_opencv_prefix}\n${_rebuild_hint}")
    endif()

    file(STRINGS "${_revision_file}" _installed_revision LIMIT_COUNT 1)
    if(NOT _installed_revision STREQUAL _pinned_revision)
        message(FATAL_ERROR
            "Selected OpenCV revision mismatch: ${_installed_revision}; expected ${_pinned_revision}.\n"
            "${_rebuild_hint}")
    endif()

    file(STRINGS "${_stamp}" _stamp_lines)
    set(_required_lines
        "commit=${_pinned_revision}"
        "modules=core,imgproc,imgcodecs,videoio,dnn"
        "video_backend=ffmpeg"
        "magicyuv_policy=disabled-before-probe-and-playback"
        "security_patch_sha256=${_original_patch_sha}"
        "ffmpeg_parser_policy=noparse-from-preopen"
        "lcevc_policy=reject-enabled-runtime"
        "hwdec_policy=cpu-only"
        "security_final_patch_sha256=${_final_patch_sha}"
        "consumer_probe=PASS")
    foreach(_required IN LISTS _required_lines)
        list(FIND _stamp_lines "${_required}" _found)
        if(_found EQUAL -1)
            message(FATAL_ERROR
                "Selected OpenCV is stale or lacks required M12.5 security protection.\n"
                "Prefix: ${_opencv_prefix}\nMissing marker: ${_required}\n${_rebuild_hint}")
        endif()
    endforeach()
    message(STATUS "ChromascopeIP: verified M12.5 FFmpeg security provenance at ${_opencv_prefix}")
endfunction()
