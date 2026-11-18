"""Test that both pinned OpenCV security patches compose in the required order.

The source revision/hash checks in apply_opencv_security_patch.py are separate.
This small synthetic OpenCV source contains the exact two patch contexts, so a
missing-context mistake is detected without downloading or compiling OpenCV.
"""

from pathlib import Path
import hashlib
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch as mock_patch
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import apply_opencv_security_patch as security

ROOT = Path(__file__).resolve().parents[2]
PATCH_DIR = ROOT / 'scripts/opencv_linux_patches'
SOURCE_NAME = Path('modules/videoio/src/cap_ffmpeg_impl.hpp')


def fixture_source():
    """Represent the original OpenCV locations modified by the first patch."""
    lines = ['// unrelated pinned OpenCV source\n'] * 1450
    lines[1207:1213] = [
        '        CV_Assert(avio_context);\n',
        '        ic->pb = avio_context;\n',
        '    }\n',
        '    int err = avformat_open_input(&ic, _filename, input_format, &dict);\n',
        '#ifdef HAVE_FFMPEG_LIBAVDEVICE\n',
        '    if (device_list)\n',
    ]
    lines[1213:1225] = [
        '    {\n',
        '        avdevice_free_list_devices(&device_list);\n',
        '        device_list = nullptr;\n',
        '    }\n',
        '#endif\n',
        '    if (err < 0)\n',
        '    {\n',
        '        CV_WARN("Error opening file");\n',
        '        CV_WARN(_filename);\n',
        '        goto exit_func;\n',
        '    }\n',
        '    err = avformat_find_stream_info(ic, NULL);\n',
    ]
    lines[1345:1351] = [
        '#endif\n',
        '                    continue;\n',
        '                }\n',
        '                context->thread_count = nThreads;\n',
        '                fill_codec_context(context, dict);\n',
        '#ifdef CV_FFMPEG_CODECPAR\n',
    ]
    return ''.join(lines).encode()


class OpenCVSecurityPatchCompositionTests(unittest.TestCase):
    def test_first_and_second_patches_apply_and_reverse_in_order(self):
        if not shutil.which('git'):
            self.skipTest('git is required to apply the OpenCV security patches')

        with tempfile.TemporaryDirectory(prefix='chromascope-m125-patch-') as temp:
            root = Path(temp)
            source = root / SOURCE_NAME
            source.parent.mkdir(parents=True)
            source.write_bytes(fixture_source())

            def apply(patch, *, reverse=False, check=False):
                cmd = ['git', '-C', str(root), 'apply']
                if reverse:
                    cmd.append('--reverse')
                if check:
                    cmd.append('--check')
                cmd.append(str(patch))
                result = subprocess.run(cmd, capture_output=True, text=True,
                                        check=False)
                self.assertEqual(result.returncode, 0, result.stderr)

            first = PATCH_DIR / 'disable-magicyuv.patch'
            second = PATCH_DIR / 'ffmpeg-final-policy.patch'
            apply(first, check=True)
            apply(first)
            intermediate = source.read_bytes()
            self.assertIn(b'// Stream probing and the selected playback decoder are separate',
                          intermediate)

            # The previously shipped M12.5 patch failed here because its final
            # hunk omitted the two comment lines inserted by the first patch.
            apply(second, check=True)
            apply(second)
            final = source.read_bytes()
            self.assertIn(b'AVFMT_FLAG_NOPARSE', final)
            self.assertIn(b'AV_CODEC_ID_JPEGXL', final)
            self.assertIn(b'"_cuvid"', final)

            apply(second, reverse=True, check=True)
            apply(second, reverse=True)
            self.assertEqual(hashlib.sha256(source.read_bytes()).digest(),
                             hashlib.sha256(intermediate).digest())
            apply(first, reverse=True)
            self.assertEqual(source.read_bytes(), fixture_source())


    def test_failed_second_patch_can_resume_from_verified_first_patch(self):
        if not shutil.which('git'):
            self.skipTest('git is required to apply the OpenCV security patches')

        with tempfile.TemporaryDirectory(prefix='chromascope-m125-resume-') as temp:
            root = Path(temp)
            source = root / SOURCE_NAME
            source.parent.mkdir(parents=True)
            original = fixture_source()
            source.write_bytes(original)

            for args in (['init', '-q'],
                         ['config', 'user.name', 'Test'],
                         ['config', 'user.email', 'test@example.invalid'],
                         ['add', str(SOURCE_NAME)],
                         ['commit', '-qm', 'Synthetic pinned OpenCV fixture']):
                result = subprocess.run(['git', '-C', str(root)] + args,
                                        capture_output=True, text=True, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
            revision = subprocess.check_output(
                ['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
            first_patch = PATCH_DIR / 'disable-magicyuv.patch'
            result = subprocess.run(['git', '-C', str(root), 'apply', str(first_patch)],
                                    capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            intermediate_hash = hashlib.sha256(source.read_bytes()).hexdigest()

            # Reproduce a bootstrap which stopped after applying only the
            # original patch. The helper must recover from this verified state.
            with mock_patch.object(security, 'COMMIT', revision), \
                 mock_patch.object(security, 'SOURCE_HASH', hashlib.sha256(original).hexdigest()), \
                 mock_patch.object(security, 'PATCHED_HASH', intermediate_hash):
                security.apply(root)
                completed = source.read_bytes()
                self.assertIn(b'AVFMT_FLAG_NOPARSE', completed)
                self.assertIn(b'AV_CODEC_ID_JPEGXL', completed)
                # A second bootstrap run must neither duplicate modifications
                # nor accept any unreviewed source state.
                security.apply(root)
                self.assertEqual(source.read_bytes(), completed)


if __name__ == '__main__':
    unittest.main()
