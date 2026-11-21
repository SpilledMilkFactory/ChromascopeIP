# Separate, fail-closed build-policy path for P3 engineering candidates only.
# The v1.0.0 security policy remains unchanged for normal application builds.
function(chromascope_require_p3_private_runtime config_dir summary)
    if(NOT CHROMASCOPE_V101_P3_PORTABLE OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "P3 provenance can only be used in explicit Linux portable mode")
    endif()
    if(NOT EXISTS "${summary}")
        message(FATAL_ERROR "P3 requires a same-job completed P2 summary JSON")
    endif()
    file(READ "${summary}" _json)
    string(JSON _status GET "${_json}" status)
    string(JSON _platform GET "${_json}" platform)
    string(JSON _approved GET "${_json}" release_approved)
    string(JSON _ff GET "${_json}" ffmpeg_commit)
    string(JSON _cv GET "${_json}" opencv_commit)
    string(JSON _decode GET "${_json}" phases decode_probes)
    string(JSON _pts GET "${_json}" phases pts_timeline)
    string(JSON _hls GET "${_json}" phases hls_probe_runtime)
    if(NOT _status STREQUAL "P2_PRIVATE_RUNTIME_FUNCTIONAL_PASS_NOT_RELEASE_APPROVAL"
       OR NOT _platform STREQUAL "debian12-x86_64"
       OR NOT _approved STREQUAL "OFF"
       OR NOT _ff STREQUAL "1041abdc962f4cc4f394aa8de9dc5236c0c3b9e7"
       OR NOT _cv STREQUAL "e4f78fbcf1a773fbed69e14500e8b734963eb585"
       OR NOT _decode STREQUAL "276_FRAMES_FILE_AND_MEMORY_VERIFIED"
       OR NOT _pts STREQUAL "VARIABLE_SOURCE_PTS_SPAN_VERIFIED"
       OR NOT _hls STREQUAL "276_FRAMES_PER_CAPTURE_HLS_OPTIONS_VERIFIED")
        message(FATAL_ERROR "Incomplete or unexpected P2 runtime provenance; reject P3")
    endif()
    get_filename_component(_cvprefix "${config_dir}/../../.." ABSOLUTE)
    get_filename_component(_p2base "${summary}/../.." ABSOLUTE)
    if(NOT _cvprefix STREQUAL "${_p2base}/private/opencv"
       OR NOT EXISTS "${_p2base}/private/ffmpeg/lib/libavcodec.so"
       OR NOT EXISTS "${_p2base}/private/ffmpeg/lib/libavformat.so"
       OR NOT EXISTS "${_p2base}/private/ffmpeg/lib/libavutil.so")
        message(FATAL_ERROR "OpenCV/FFmpeg must originate from the verified same-job P2 prefix")
    endif()
    message(STATUS "P3 private runtime candidate provenance accepted for build ONLY; NOT release approved")
endfunction()
