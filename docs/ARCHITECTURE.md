# ChromascopeIP — M0/M0.1 Architecture

## Goal

I am using the Skittles example to create a tested, reusable C++ image-processing foundation. I want a small codebase with clear GUI/CLI separation so that I can add video and detection engines deliberately rather than coupling OpenCV calls to the interface.

## Dependency graph

```text
chromascope-cli ----------> chromascope_core ----> OpenCV 5.1.0
                                  ^
                                  |
chromascope-gui ------------------+
       |                          |
       +-- Dear ImGui 1.93.0      +-- image I/O + classical detection
       +-- GLFW + OpenGL 3.3
```

The core uses `cv::Mat` as the image container and returns plain detection results. It does not depend on Dear ImGui, GLFW, or an OpenGL context. Both front ends invoke `detect_skittles()` and `draw_annotations()`; neither implements a competing classifier.

## Processing pipeline

1. I load and validate a full-color input with `cv::imread`.
2. I downscale an analysis copy to a configurable maximum long edge (1200 pixels by default), preserving aspect ratio. I retain the original source image for exporting annotations.
3. I blur grayscale data and find candidate circles with `cv::HoughCircles` (`HOUGH_GRADIENT`). Radius/spacing constraints scale with the analysis image dimensions rather than being hardcoded to the original 4032×3024 photographs.
4. I convert the working image to HSV and sample each circle's inner disk. I take a **circularly unwrapped hue median** (hue 179 is adjacent to 0), plus ordinary saturation/value medians. This avoids losing dark red candies to hue-wrap artifacts while preserving robustness against printed `S` marks and highlights.
5. I map the median HSV into a five-color palette, rejecting candidates with unrecognized median colors or insufficient per-pixel confidence. The Skittles red/orange boundary is calibrated at hue **7/8** using the original photographs.
6. I translate circle locations/radii into **original-image coordinates** and update counts.
7. I render optional circles/labels onto a copy of the **full-resolution input**. The CLI writes this image directly; the GUI uploads a capped preview to its OpenGL texture.

The dark purple class includes near-black candies from the historical photographs. That heuristic is intentionally profile-specific and should not be reused for arbitrary object recognition.

## Public API

- `read_image(path)`: checked image decoding.
- `detect_skittles(image, settings)`: single-profile analysis with timings and per-object geometry.
- `draw_annotations(image, result, labels)`: no mutation of source image.
- `classify_hsv(pixel, recognized)`: isolated HSV mapping for deterministic tests.
- `validate_settings(settings)`: stable bounds for algorithm controls.

I have reserved a profile argument in the CLI (`--mode skittles`) so that future processing modes can be added without changing established command-line behavior.

## Deliberately excluded from M0

I am not claiming camera access, tracking between frames, face recognition, person detection, deep-learning inference, multi-class general object detection, asynchronous work queues, plugin loading, or distributable binaries. I will evaluate these separately after completing the native baseline qualification.

## Potential architectural next steps (not yet a committed roadmap)

I can next consider an `ImageFrame` abstraction (for stills/video), a processor interface for pluggable recognition modes, a bounded asynchronous work queue, timestamped track IDs, independent annotation layers, model backends, GPU acceleration, capture-device discovery, and opt-in privacy-preserving camera operation. I will choose their order from measured usage needs rather than introducing all of them into this simple baseline.
