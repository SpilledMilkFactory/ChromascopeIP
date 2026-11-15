#pragma once

#include "CameraCliContract.hpp"
#include "chromascope/DnnPostprocessing.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace chromascope::cli {

// Explicit opt-in camera detection; independent of both recorded-video JSONL
// and the M8.5 capture-only CLI. Always finite, including on Ctrl-C.
struct LiveCameraDetectionOptions {
    CameraCliOptions camera;
    std::filesystem::path model;
    std::filesystem::path jsonl; // empty: stdout ('-'); output path: create only
    DnnPostprocessOptions postprocess;
    std::string profile = "object";
};

inline double live_cli_real(std::string_view text, std::string_view flag) {
    const std::string value(text);
    std::size_t consumed = 0;
    const double result = std::stod(value, &consumed);
    if (consumed != value.size() || !std::isfinite(result))
        throw std::invalid_argument(std::string(flag) + " requires a finite number");
    return result;
}

inline LiveCameraDetectionOptions parse_camera_detection_cli(int argc, char* const argv[]) {
    LiveCameraDetectionOptions result;
    std::set<std::string> seen;
    bool enabled = false;
    bool camera = false;
    bool has_jsonl = false;
    bool has_profile = false;
    bool has_display = false;
    const auto once = [&](std::string_view flag) {
        if (!seen.emplace(flag).second)
            throw std::invalid_argument("Duplicate camera detection option: " + std::string(flag));
    };
    const auto next = [&](int& i, std::string_view flag) -> std::string_view {
        if (++i >= argc) throw std::invalid_argument("Missing value after " + std::string(flag));
        return argv[i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        if (flag == "--camera-detect") {
            once(flag); enabled = true;
        } else if (flag == "--camera") {
            once(flag); camera = true;
            result.camera.device_index = static_cast<int>(camera_cli_unsigned(next(i, flag), flag, 255));
        } else if (flag == "--camera-backend") {
            once(flag);
            const auto v = next(i, flag);
            if (v == "default") result.camera.backend = CameraBackend::PlatformDefault;
            else if (v == "v4l2") result.camera.backend = CameraBackend::V4L2;
            else if (v == "msmf") result.camera.backend = CameraBackend::MediaFoundation;
            else if (v == "dshow") result.camera.backend = CameraBackend::DirectShow;
            else throw std::invalid_argument("Unknown camera backend");
        } else if (flag == "--camera-max-frames") {
            once(flag);
            result.camera.max_frames = camera_cli_unsigned(next(i, flag), flag, 300);
            if (!result.camera.max_frames) throw std::invalid_argument("Frame limit cannot be zero");
        } else if (flag == "--camera-max-seconds") {
            once(flag);
            result.camera.max_seconds = camera_cli_unsigned(next(i, flag), flag, 60);
            if (!result.camera.max_seconds) throw std::invalid_argument("Time limit cannot be zero");
        } else if (flag == "--jsonl") {
            once(flag); has_jsonl = true;
            const auto v = next(i, flag);
            if (v.empty()) throw std::invalid_argument("--jsonl needs a destination");
            if (v != "-") result.jsonl = std::string(v);
        } else if (flag == "--profile" || flag == "--person-only") {
            if (has_profile) throw std::invalid_argument("Camera detection profile repeated");
            has_profile = true;
            result.profile = flag == "--person-only" ? "person" : std::string(next(i, flag));
            if (result.profile != "person" && result.profile != "object")
                throw std::invalid_argument("Live camera requires object or person profile");
        } else if (flag == "--model") {
            once(flag);
            const auto v = next(i, flag);
            if (v.empty()) throw std::invalid_argument("Empty model path");
            result.model = std::string(v);
        } else if (flag == "--confidence") {
            once(flag);
            result.postprocess.confidence_threshold = live_cli_real(next(i, flag), flag);
        } else if (flag == "--display-threshold") {
            once(flag); has_display = true;
            result.postprocess.person_display_threshold = live_cli_real(next(i, flag), flag);
        } else if (flag == "--nms-iou") {
            once(flag);
            result.postprocess.nms_iou_threshold = live_cli_real(next(i, flag), flag);
        } else if (flag == "--max-detections") {
            once(flag);
            result.postprocess.max_detections = camera_cli_unsigned(next(i, flag), flag, 512);
        } else if (flag == "--uncertain-floor") {
            once(flag);
            result.postprocess.uncertainty_floor = live_cli_real(next(i, flag), flag);
        } else if (flag == "--class-threshold") {
            const auto v = next(i, flag);
            const auto at = v.rfind('=');
            if (at == std::string_view::npos || at == 0 || at + 1 == v.size())
                throw std::invalid_argument("--class-threshold requires CLASS=NUMBER");
            result.postprocess.class_thresholds.push_back({nanodet_class_id(v.substr(0, at)),
                live_cli_real(v.substr(at + 1), flag)});
        } else {
            throw std::invalid_argument("Unexpected option with --camera-detect: " + std::string(flag));
        }
    }
    if (!enabled || !camera || !has_jsonl || !has_profile || result.model.empty())
        throw std::invalid_argument("--camera-detect requires --camera INDEX --profile object|person --model FILE --jsonl FILE|-");
    if (result.model.extension() != ".onnx") throw std::invalid_argument("--model must be a local .onnx file");
    if (result.jsonl != std::filesystem::path{} && result.jsonl.extension() != ".jsonl")
        throw std::invalid_argument("--jsonl file must end in .jsonl");
    result.postprocess.person_only = result.profile == "person";
    if (has_display && !result.postprocess.person_only)
        throw std::invalid_argument("--display-threshold requires person mode");
    if (result.postprocess.person_only) {
        for (const auto& threshold : result.postprocess.class_thresholds)
            if (threshold.class_id != 0)
                throw std::invalid_argument("Person mode only permits class 0 thresholds");
    }
    validate_dnn_postprocess_options(result.postprocess);
    return result;
}

} // namespace chromascope::cli
