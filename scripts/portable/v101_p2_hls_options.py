#!/usr/bin/env python3
"""Pinned OpenCV 5.1.0 adapter: explicit per-capture HLS-only FFmpeg probing.

The marker is private to our Debian 12 OpenCV build. Never set a process-wide
OPENCV_FFMPEG_CAPTURE_OPTIONS override: it would affect other media formats.
"""

# Deliberately outside the upstream OpenCV VideoCapture property enum.
# Must match the private P3 call site and test probe.
HLS_CAPTURE_PROPERTY = 65017
HLS_CAPTURE_VALUE = 1
HLS_OPEN_OPTIONS = {
    'input_format': 'mpegts',
    'probesize': '65536',
    'analyzeduration': '1000000',
    'fpsprobesize': '2',
}

_OPEN_ANCHOR = '    int nThreads = 0;\n\n    close();\n'
_PARAM_ANCHOR = (
    '        if (params.has(CAP_PROP_IMAGE_SEQ_START))\n'
    '        {\n'
    '            image_seq_start = params.get<int>(CAP_PROP_IMAGE_SEQ_START);\n'
    '        }\n'
    '        if (params.warnUnusedParameters())\n'
)
_DICT_ANCHOR = (
    '    if (image_seq_start >= 0)\n'
    '    {\n'
    '        av_dict_set_int(&dict, "start_number", image_seq_start, 0);\n'
)
_OPEN_INSERT = (
    '    // ChromascopeIP private P3 contract: opt-in per HLS capture, never global.\n'
    '    bool chromascope_hls_mpegts_probe = false;\n'
)
_PARAM_INSERT = (
    '        if (params.has(65017))\n'
    '        {\n'
    '            const int profile = params.get<int>(65017);\n'
    '            if (profile != 1 || !stream || index >= 0)\n'
    '            {\n'
    '                CV_LOG_ERROR(NULL, "ChromascopeIP HLS probing requires a custom stream reader");\n'
    '                return false;\n'
    '            }\n'
    '            chromascope_hls_mpegts_probe = true;\n'
    '        }\n'
)
_DICT_INSERT = (
    '    if (chromascope_hls_mpegts_probe)\n'
    '    {\n'
    '        // Settings qualified on real HLS; override only this capture.\n'
    '        av_dict_set(&dict, "input_format", "mpegts", 0);\n'
    '        av_dict_set(&dict, "probesize", "65536", 0);\n'
    '        av_dict_set(&dict, "analyzeduration", "1000000", 0);\n'
    '        av_dict_set(&dict, "fpsprobesize", "2", 0);\n'
    '    }\n'
)


def patch_pinned_opencv(original: str) -> str:
    """Only transform a source with the three expected unmodified anchors.

    This is invoked after exact OpenCV source pin and other security patches
    have been verified. Unknown source versions or duplicate markers fail closed.
    """
    for name, anchor in [('open', _OPEN_ANCHOR), ('parameters', _PARAM_ANCHOR),
                         ('dictionary', _DICT_ANCHOR)]:
        if original.count(anchor) != 1:
            raise ValueError(f'Pinned OpenCV HLS {name} anchor is missing or ambiguous')
    if 'chromascope_hls_mpegts_probe' in original or 'params.has(65017)' in original:
        raise ValueError('Private HLS probing hook already present or unexpected')
    result = original.replace(_OPEN_ANCHOR,
                              '    int nThreads = 0;\n' + _OPEN_INSERT + '\n    close();\n', 1)
    result = result.replace(_PARAM_ANCHOR,
                            _PARAM_ANCHOR.replace('        if (params.warnUnusedParameters())\n',
                                                  _PARAM_INSERT + '        if (params.warnUnusedParameters())\n'), 1)
    result = result.replace(_DICT_ANCHOR, _DICT_INSERT + _DICT_ANCHOR, 1)
    return result
