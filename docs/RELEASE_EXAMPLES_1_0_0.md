# ChromascopeIP 1.0.0 — reproducible first-use examples

The commands below use an **original synthetic image created locally** by the
first-party generator, not the historical Skittles photographs, downloaded
stock media, or redistributed model weights. These are reproducible command
examples; they do not imply that the synthetic circles are Skittles or produce
specified object-detection results.

## Generate a rights-clear demo PNG

From a cloned source checkout, with Python 3 and no network access:

```sh
python3 scripts/create_release_demo_m127.py --output demo-circles.png
```

The generator creates a deterministic 640×360 RGB PNG with three colored
circles. It refuses to overwrite an existing destination. It does not require
OpenCV, model weights, personal photos or camera access. In a **binary-only**
installation, generate the PNG separately from a source checkout or provide
your own media; the generator is a development example, not a required runtime
component bundled with the v1.0 packages.

## CLI: safe information and image processing

On a Linux system with the `.deb` installed:

```sh
chromascope-cli --version
chromascope-cli --list-profiles
chromascope-cli --privacy-info
chromascope-cli --input demo-circles.png --profile grayscale --output demo-gray.png
chromascope-cli --input demo-circles.png --profile canny_edges --output demo-edges.png
chromascope-cli --input demo-circles.png --profile skittles --json -
```

The Skittles profile may return zero detections on this purely synthetic
non-Skittles picture; that is expected and not evidence of a broken detector.
The `--output` operations write local files; choose unused destination names.
For Windows, invoke `chromascope-cli.exe` from the portable `bin` directory
with the same flags and an explicit path to your generated PNG.

## GUI: no-camera demonstration

Launch `chromascope-gui` (Linux) or `chromascope-gui.exe` (Windows). Open the
original synthetic PNG, try grayscale/edge processing, compare overlays, and
close the application. You do **not** need to start a camera or load an ONNX
model to exercise still-image features. The GUI's built-in buttons for old
sample images are intentionally unavailable when those media files are absent
from a binary installation.

## Recorded video, models and camera: explicitly optional

To use recorded video, select **your own trusted local recording**; the
application supports probing, seeking and bounded analysis, with explicit
silent MJPEG/AVI export. Consult [CLI reference](CLI_REFERENCE.md) for exact
flags, JSONL completion codes, and audio-drop policy.

Object/person inference requires a separate locally supplied, source-verified
NanoDet `.onnx` model. It is neither downloaded nor bundled automatically.
Live camera access and inference must be activated explicitly and require
permission. Do not publish screenshots containing people, private media,
paths or device IDs without the relevant consent.
