#include "chromascope/Detection.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"
#include "CliOutputPaths.hpp"
#include "VideoCliContract.hpp"
#include "chromascope/VideoSource.hpp"
#include "chromascope/CpuDnnAdapter.hpp"
#include "chromascope/DnnGuiSession.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/NanoDetDecoder.hpp"
#include "chromascope/ObjectFrameInference.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/RecordedVideoAnalysis.hpp"
#include "chromascope/AnnotatedVideoExport.hpp"

#include <opencv2/core/version.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <chrono>
#include <locale>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>
#include <utility>

namespace {
struct Options {
    std::filesystem::path input, output, json, jsonl, mask_output, output_dir;
    int json_version = 1;
    bool json_stdout = false;
    bool jsonl_stdout = false;
    bool labels = true;
    std::string profile_id = "skittles";
    chromascope::Settings settings;
    std::map<std::string, double, std::less<>> profile_options;
    bool video_probe = false;
    bool analyze_video = false;
    bool export_annotated_video = false;
    bool audio_drop_explicit = false;
    bool mjpeg_explicit = false;
    std::optional<double> output_fps;
    chromascope::RecordedVideoAnalysisOptions video_analysis;
    std::optional<std::uint64_t> extract_frame;
    std::optional<std::uint64_t> detect_video_frame;
    chromascope::VideoBackend video_backend = chromascope::VideoBackend::Automatic;
    bool video_backend_explicit = false;
    std::filesystem::path model;
    chromascope::DnnPostprocessOptions object_options;
    bool legacy_settings_changed = false;
    std::set<std::string> legacy_keys;
};

void usage(std::ostream& os) {
    os << "ChromascopeIP 0.1.3 - image processing CLI\n"
          "Usage: chromascope-cli --input IMAGE [options]\n"
          "       chromascope-cli --video-probe --input VIDEO [--json FILE|-]\n"
          "       chromascope-cli --extract-frame INDEX --input VIDEO --output FRAME.png\n"
          "       chromascope-cli --detect-video-frame INDEX --input VIDEO --profile object --model ONNX [--json FILE|-]\n"
          "       chromascope-cli --analyze-video --input VIDEO --profile object --model ONNX --jsonl FILE|- [options]\n"
          "       chromascope-cli --export-annotated-video --input VIDEO --profile object --model ONNX\n"
          "                         --output RESULT.avi --video-codec mjpeg --audio-policy drop [options]\n\n"
          "  -i, --input FILE       Input image or recorded-video file (required)\n"
          "      --video-probe     Inspect video metadata and first decoded frame\n"
          "      --extract-frame N  Extract zero-based video frame N to lossless PNG\n"
          "      --detect-video-frame N  Detect COCO objects in recorded frame N (zero-based)\n"
          "      --analyze-video    Sequential video object detection with JSONL frame records\n"
          "      --export-annotated-video  Encode all analyzed frames as silent MJPEG/AVI\n"
          "      --video-codec mjpeg  Explicit export codec (only MJPEG/AVI supported)\n"
          "      --audio-policy drop  Required: discard any source audio, no mux/passthrough\n"
          "      --output-fps N    Constant output FPS [1,240]; default source nominal FPS\n"
          "      --jsonl FILE|-    Required for --analyze-video; stream records or stdout\n"
          "      --max-video-frames N  Cap decoded frames at N [1,100000] (default 100000)\n"
          "      --frame-step N    Analyze every Nth decoded frame [1,10000] (default 1)\n"
          "      --video-backend ID  Video backend: auto (default), opencv-mjpeg\n"
          "  -o, --output FILE      Save annotated image or processed raster\n"
          "      --mask-output FILE Export segmentation/contour mask separately\n"
          "      --json FILE|-     Export machine-readable JSON; - writes to stdout\n"
          "      --json-version N   JSON schema version (supported: 1; default: 1)\n"
          "      --output-dir DIR   Prefix relative output filenames; create DIR if needed\n"
          "      --mode skittles   Legacy alias for --profile skittles\n"
          "      --profile ID      Select a registered processing profile\n"
          "      --set KEY=VALUE   Override a named numeric processor option (repeatable)\n"
          "      --profile object   NanoDet COCO-80 CPU object detection\n"
          "      --model FILE      Required pinned NanoDet ONNX for --profile object\n"
          "      --confidence N    Object score threshold in [0,1], default 0.50\n"
          "      --class-threshold CLASS=N  Per-class cutoff (repeatable; exact name or 0..79)\n"
          "      --uncertain-floor N  Diagnostic-only weak candidate floor (default 0.35)\n"
          "      --list-object-classes  List the 80 supported NanoDet/COCO labels\n"
          "      --nms-iou N       Class-aware NMS IoU in [0,1], default 0.60\n"
          "      --max-detections N Maximum object detections in [1,512], default 100\n"
          "      --list-profiles   List processors, defaults, and option bounds\n"
          "      --accumulator N   Hough sensitivity threshold, default 31\n"
          "      --edge N          Hough edge threshold, default 95\n"
          "      --max-dimension N Processing resolution ceiling, default 1200\n"
          "      --min-confidence N Color confidence in [0,1], default 0.25\n"
          "      --no-labels       Draw circles without text\n"
          "  -h, --help            Show command reference\n"
          "      --version         Print version and OpenCV version\n";
}

std::string next(int& i, int argc, char* argv[], std::string_view flag) {
    if (i + 1 == argc) throw std::invalid_argument("Missing value after " + std::string(flag));
    return argv[++i];
}

std::string required_path(int& i, int argc, char* argv[], std::string_view flag) {
    auto value = next(i, argc, argv, flag);
    if (value.empty())
        throw std::invalid_argument("Empty path after " + std::string(flag));
    return value;
}

// Require complete numeric arguments; stoi/stod otherwise silently accept
// trailing characters such as "1200junk" as apparently valid settings.
int parse_integer(const std::string& text) {
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (consumed != text.size())
        throw std::invalid_argument("Invalid integer argument: " + text);
    return value;
}

double parse_real(const std::string& text) {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size())
        throw std::invalid_argument("Invalid numeric argument: " + text);
    if (!std::isfinite(value))
        throw std::invalid_argument("Non-finite numeric argument: " + text);
    return value;
}

Options parse(int argc, char* argv[]) {
    Options o;
    std::set<std::string> single_use;
    const auto once = [&](std::string_view flag, std::string_view category) {
        if (!single_use.emplace(category).second)
            throw std::invalid_argument("Repeated or conflicting argument: " + std::string(flag));
    };
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "-i" || flag == "--input") {
            once(flag, "input"); o.input = required_path(i, argc, argv, flag);
        } else if (flag == "-o" || flag == "--output") {
            once(flag, "output"); o.output = required_path(i, argc, argv, flag);
        } else if (flag == "--mask-output") {
            once(flag, "mask"); o.mask_output = required_path(i, argc, argv, flag);
        } else if (flag == "--output-dir") {
            once(flag, "output-dir"); o.output_dir = required_path(i, argc, argv, flag);
        } else if (flag == "--json") {
            once(flag, "json");
            const auto value = required_path(i, argc, argv, flag);
            if (value == "-") o.json_stdout = true;
            else o.json = value;
        } else if (flag == "--jsonl") {
            once(flag, "jsonl");
            const auto value = required_path(i, argc, argv, flag);
            if (value == "-") o.jsonl_stdout = true;
            else o.jsonl = value;
        } else if (flag == "--max-video-frames") {
            once(flag, "video-max");
            const int count = parse_integer(next(i, argc, argv, flag));
            if (count < 1 || count > 100000)
                throw std::invalid_argument("--max-video-frames must be in [1,100000]");
            o.video_analysis.max_decoded_frames = static_cast<std::uint64_t>(count);
        } else if (flag == "--frame-step") {
            once(flag, "frame-step");
            const int count = parse_integer(next(i, argc, argv, flag));
            if (count < 1 || count > 10000)
                throw std::invalid_argument("--frame-step must be in [1,10000]");
            o.video_analysis.frame_step = static_cast<std::uint32_t>(count);
        } else if (flag == "--json-version") {
            once(flag, "json-version");
            o.json_version = parse_integer(next(i, argc, argv, flag));
        } else if (flag == "--video-probe") {
            once(flag, "video-action"); o.video_probe = true;
        } else if (flag == "--analyze-video") {
            once(flag, "video-action"); o.analyze_video = true;
        } else if (flag == "--export-annotated-video") {
            once(flag, "video-action"); o.export_annotated_video = true;
        } else if (flag == "--video-codec") {
            once(flag, "video-codec");
            if (next(i, argc, argv, flag) != "mjpeg")
                throw std::invalid_argument("--video-codec supports only mjpeg (AVI)");
            o.mjpeg_explicit = true;
        } else if (flag == "--audio-policy") {
            once(flag, "audio-policy");
            if (next(i, argc, argv, flag) != "drop")
                throw std::invalid_argument("--audio-policy supports only drop; no audio is exported");
            o.audio_drop_explicit = true;
        } else if (flag == "--output-fps") {
            once(flag, "output-fps");
            const double fps = parse_real(next(i, argc, argv, flag));
            if (fps < 1.0 || fps > 240.0)
                throw std::invalid_argument("--output-fps must be in [1,240]");
            o.output_fps = fps;
        } else if (flag == "--extract-frame") {
            once(flag, "video-action");
            o.extract_frame = chromascope::cli::parse_video_frame_index(next(i, argc, argv, flag));
        } else if (flag == "--detect-video-frame") {
            once(flag, "video-action");
            o.detect_video_frame = chromascope::cli::parse_video_frame_index(next(i, argc, argv, flag));
        } else if (flag == "--video-backend") {
            once(flag, "video-backend");
            o.video_backend_explicit = true;
            const auto value = next(i, argc, argv, flag);
            if (value == "auto") o.video_backend = chromascope::VideoBackend::Automatic;
            else if (value == "opencv-mjpeg") o.video_backend = chromascope::VideoBackend::OpenCVMJPEG;
            else throw std::invalid_argument("Unknown --video-backend (supported: auto, opencv-mjpeg)");
        } else if (flag == "--mode" || flag == "--profile") {
            once(flag, "profile");
            o.profile_id = next(i, argc, argv, flag);
            if (flag == "--mode" && o.profile_id != "skittles")
                throw std::invalid_argument("--mode only supports skittles; use --profile ID");
        } else if (flag == "--model") {
            once(flag, "model"); o.model = required_path(i, argc, argv, flag);
        } else if (flag == "--confidence" || flag == "--confidence-threshold") {
            once(flag, "object-confidence");
            o.object_options.confidence_threshold = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--nms-iou" || flag == "--nms") {
            once(flag, "object-nms");
            o.object_options.nms_iou_threshold = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--class-threshold") {
            const auto value = next(i, argc, argv, flag);
            const auto equals = value.rfind('=');
            if (equals == std::string::npos || equals == 0 || equals + 1 == value.size())
                throw std::invalid_argument("--class-threshold requires CLASS=N");
            const int id = chromascope::nanodet_class_id(
                std::string_view(value).substr(0, equals));
            const double threshold = parse_real(value.substr(equals + 1));
            o.object_options.class_thresholds.push_back({id, threshold});
        } else if (flag == "--uncertain-floor") {
            once(flag, "uncertain-floor");
            o.object_options.uncertainty_floor = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--max-detections") {
            once(flag, "object-max");
            const int value = parse_integer(next(i, argc, argv, flag));
            if (value < 1 || value > 512)
                throw std::invalid_argument("--max-detections must be in [1,512]");
            o.object_options.max_detections = static_cast<std::size_t>(value);
        } else if (flag == "--set") {
            const auto setting = next(i, argc, argv, flag);
            const auto pos = setting.find('=');
            if (pos == 0 || pos == std::string::npos || pos + 1 == setting.size())
                throw std::invalid_argument("--set requires KEY=VALUE");
            const auto key = setting.substr(0, pos);
            const auto value = parse_real(setting.substr(pos + 1));
            if (!o.profile_options.emplace(key, value).second)
                throw std::invalid_argument("Duplicate --set option: " + key);
        } else if (flag == "--accumulator") {
            once(flag, "accumulator");
            o.legacy_settings_changed = true;
            o.legacy_keys.insert("accumulator_threshold");
            o.settings.accumulator_threshold = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--edge") {
            once(flag, "edge");
            o.legacy_settings_changed = true;
            o.legacy_keys.insert("edge_threshold");
            o.settings.edge_threshold = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--max-dimension") {
            once(flag, "max-dimension");
            o.legacy_settings_changed = true;
            o.legacy_keys.insert("max_dimension");
            o.settings.max_dimension = parse_integer(next(i, argc, argv, flag));
        } else if (flag == "--min-confidence") {
            once(flag, "min-confidence");
            o.legacy_settings_changed = true;
            o.legacy_keys.insert("minimum_confidence");
            o.settings.minimum_confidence = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--no-labels") {
            once(flag, "no-labels"); o.labels = false;
        } else {
            throw std::invalid_argument("Unknown argument: " + std::string(flag));
        }
    }
    if (o.input.empty()) throw std::invalid_argument("--input is required");
    if (o.profile_id.empty()) throw std::invalid_argument("--profile requires a nonempty ID");
    const bool video_object = o.detect_video_frame.has_value();
    const bool video = o.video_probe || o.extract_frame.has_value() || video_object ||
                       o.analyze_video || o.export_annotated_video;
    const bool object = o.profile_id == "object";
    const bool object_arguments = single_use.contains("model") ||
        single_use.contains("object-confidence") || single_use.contains("object-nms") ||
        single_use.contains("object-max") ||
        single_use.contains("uncertain-floor") || !o.object_options.class_thresholds.empty();
    if (object) {
        if (o.model.empty()) throw std::invalid_argument("--profile object requires --model FILE");
        if (!o.profile_options.empty() || o.legacy_settings_changed || !o.mask_output.empty())
            throw std::invalid_argument("Object detection does not accept --set, legacy, or --mask-output options");
        chromascope::validate_dnn_postprocess_options(o.object_options);
    } else if (object_arguments) {
        throw std::invalid_argument("--model and object thresholds require --profile object");
    }
    if (!o.analyze_video && (single_use.contains("jsonl") ||
                             (!o.export_annotated_video && single_use.contains("video-max")) ||
                             single_use.contains("frame-step")))
        throw std::invalid_argument("--jsonl and --frame-step require --analyze-video; --max-video-frames also permits --export-annotated-video");
    if (!o.export_annotated_video &&
        (o.audio_drop_explicit || o.mjpeg_explicit || o.output_fps))
        throw std::invalid_argument("--video-codec, --audio-policy and --output-fps require --export-annotated-video");
    if (video) {
        if (o.analyze_video) {
            if (!object || o.model.empty())
                throw std::invalid_argument("--analyze-video requires --profile object and --model");
            if (o.jsonl.empty() && !o.jsonl_stdout)
                throw std::invalid_argument("--analyze-video requires --jsonl FILE|-");
            if ((!o.jsonl.empty() && o.jsonl.extension() != ".jsonl") ||
                !o.json.empty() || o.json_stdout || !o.output.empty() || !o.mask_output.empty() ||
                single_use.contains("no-labels"))
                throw std::invalid_argument("--analyze-video accepts only --jsonl .jsonl|- (no --json, --output, --mask-output, or --no-labels)");
        } else if (o.export_annotated_video) {
            if (!object || o.model.empty() || !o.audio_drop_explicit || !o.mjpeg_explicit ||
                o.output.empty() || o.output.extension() != ".avi" || !o.json.empty() ||
                o.json_stdout || !o.jsonl.empty() || o.jsonl_stdout || !o.mask_output.empty())
                throw std::invalid_argument(
                    "--export-annotated-video requires --profile object --model ONNX --output FILE.avi "
                    "--video-codec mjpeg --audio-policy drop (no --json or --jsonl)");
        } else if (video_object) {
            if (!object)
                throw std::invalid_argument("--detect-video-frame requires --profile object and --model");
            if (!o.output.empty()) chromascope::cli::validate_video_png_path(o.output);
        } else {
            if (single_use.contains("profile") || object_arguments || !o.profile_options.empty() ||
                o.legacy_settings_changed || single_use.contains("no-labels") ||
                !o.mask_output.empty())
                throw std::invalid_argument("Video probe/extraction cannot use still-image processing or object-profile options");
            if (o.video_probe && !o.output.empty())
                throw std::invalid_argument("--video-probe does not accept --output; use --extract-frame");
            if (o.extract_frame) {
                if (o.output.empty())
                    throw std::invalid_argument("--extract-frame requires --output FRAME.png");
                chromascope::cli::validate_video_png_path(o.output);
            }
        }
    } else if (o.video_backend_explicit) {
        throw std::invalid_argument("--video-backend requires a recorded-video command");
    }
    if (o.json_version != 1)
        throw std::invalid_argument("Unsupported JSON schema version (supported: 1)");
    if (single_use.contains("json-version") && o.json.empty() && !o.json_stdout)
        throw std::invalid_argument("--json-version requires --json FILE|-");
    if (!video && o.profile_id == "skittles") chromascope::validate_settings(o.settings);
    else if (!video && o.legacy_settings_changed)
        throw std::invalid_argument("Legacy Skittles flags cannot be used with other profiles; use --set");
    for (const auto& key : o.legacy_keys) {
        if (o.profile_options.contains(key))
            throw std::invalid_argument("Conflicting legacy flag and --set for option: " + key);
    }
    return o;
}

std::string escape_json(std::string_view str) {
    std::string value;
    for (unsigned char c : str) {
        switch (c) {
            case '\\': value += "\\\\"; break;
            case '"': value += "\\\""; break;
            case '\n': value += "\\n"; break;
            case '\r': value += "\\r"; break;
            case '\t': value += "\\t"; break;
            default:
                if (c < 0x20) {
                    constexpr char digits[] = "0123456789abcdef";
                    value += "\\u00";
                    value += digits[c >> 4];
                    value += digits[c & 15];
                } else value += static_cast<char>(c);
        }
    }
    return value;
}

void write_json(std::ostream& stream, const Options& options, const chromascope::Result& r) {
    stream << std::fixed << std::setprecision(3);
    stream << "{\n  \"schema_version\": " << options.json_version
           << ",\n  \"application\": \"ChromascopeIP\",\n"
              "  \"profile\": \"skittles\",\n"
           << "  \"input\": \"" << escape_json(options.input.string()) << "\",\n"
           << "  \"width\": " << r.width << ", \"height\": " << r.height << ",\n"
           << "  \"elapsed_ms\": " << r.elapsed_ms << ",\n"
           << "  \"total\": " << r.objects.size() << ",\n"
              "  \"counts\": {";
    for (std::size_t i = 0; i < chromascope::kColorCount; ++i) {
        const auto color = static_cast<chromascope::CandyColor>(i);
        stream << (i ? ", " : "") << "\"" << chromascope::color_name(color) << "\": "
               << r.counts[i];
    }
    stream << "},\n  \"detections\": [\n";
    for (std::size_t i = 0; i < r.objects.size(); ++i) {
        const auto& obj = r.objects[i];
        stream << "    {\"color\": \"" << chromascope::color_name(obj.color)
               << "\", \"x\": " << obj.center.x << ", \"y\": " << obj.center.y
               << ", \"radius\": " << obj.radius
               << ", \"confidence\": " << obj.confidence << "}"
               << (i + 1 == r.objects.size() ? "" : ",") << "\n";
    }
    stream << "  ]\n}\n";
}
// Schema v1 adds only a top-level schema_version to the legacy layout. Generic processors
// report owned detection geometry without assigning semantic class names.
void write_general_json(std::ostream& stream, const Options& options,
                        const chromascope::ProcessedFrame& processed) {
    const auto& result = processed.detections;
    stream << std::fixed << std::setprecision(3);
    stream << "{\n  \"schema_version\": " << options.json_version
           << ",\n  \"application\": \"ChromascopeIP\",\n"
           << "  \"profile\": \"" << escape_json(result.processor_id()) << "\",\n"
           << "  \"input\": \"" << escape_json(options.input.string()) << "\",\n"
           << "  \"width\": " << result.width() << ", \"height\": " << result.height() << ",\n"
           << "  \"elapsed_ms\": " << processed.elapsed_ms << ",\n"
           << "  \"total\": " << result.objects().size() << ",\n  \"detections\": [";
    if (result.objects().empty()) {
        stream << "]\n}\n";
        return;
    }
    for (std::size_t index = 0; index < result.objects().size(); ++index) {
        const auto& object = result.objects()[index];
        stream << (index == 0 ? "\n" : ",\n")
               << "    {\"label\": \"" << escape_json(object.label)
               << "\", \"score\": " << object.score;
        std::visit([&stream](const auto& shape) {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, chromascope::BoxGeometry>) {
                stream << ", \"geometry\": {\"type\": \"box\", \"x\": " << shape.x
                       << ", \"y\": " << shape.y << ", \"width\": " << shape.width
                       << ", \"height\": " << shape.height << "}";
            } else if constexpr (std::is_same_v<T, chromascope::PolygonGeometry>) {
                stream << ", \"geometry\": {\"type\": \"polygon\", \"vertices\": [";
                for (std::size_t i = 0; i < shape.vertices.size(); ++i) {
                    if (i) stream << ", ";
                    stream << "[" << shape.vertices[i].x << ", " << shape.vertices[i].y << "]";
                }
                stream << "]}";
            } else {
                stream << ", \"geometry\": {\"type\": \"circle\", \"x\": "
                       << shape.center.x << ", \"y\": " << shape.center.y
                       << ", \"radius\": " << shape.radius << "}";
            }
        }, object.geometry);
        stream << "}";
    }
    stream << "\n  ]\n}\n";
}

// Video JSON is schema-versioned independently of still-image detection JSON.
// Optional decoder metadata is emitted as JSON null, never as invented zeroes.
void write_video_json(std::ostream& stream, const Options& options,
                      const chromascope::VideoSource& source,
                      const chromascope::VideoFramePacket& frame) {
    stream.imbue(std::locale::classic());
    stream << "{\n  \"schema_version\": 1,\n"
              "  \"application\": \"ChromascopeIP\",\n"
           << "  \"operation\": \"" << (options.video_probe ? "video_probe" : "frame_extract")
           << "\",\n  \"input\": \"" << escape_json(options.input.string()) << "\",\n"
           << "  \"backend\": \""
           << (options.video_backend == chromascope::VideoBackend::OpenCVMJPEG
               ? "opencv-mjpeg" : "auto") << "\",\n"
           << "  \"fps\": ";
    if (const auto fps = source.frames_per_second())
        stream << std::fixed << std::setprecision(6) << *fps;
    else stream << "null";
    stream << ",\n  \"reported_frame_count\": ";
    if (const auto count = source.reported_frame_count()) stream << *count;
    else stream << "null";
    stream << ",\n  \"frame\": {\n"
           << "    \"index\": " << frame.index << ",\n"
           << "    \"width\": " << frame.image.width() << ",\n"
           << "    \"height\": " << frame.image.height() << ",\n"
           << "    \"timestamp_ns\": ";
    const auto& timestamp = frame.image.metadata().timestamp;
    if (timestamp) stream << timestamp->value.count();
    else stream << "null";
    stream << ",\n    \"timestamp_source\": \"";
    switch (frame.timestamp_source) {
        case chromascope::VideoTimestampSource::DecoderReported:
            stream << "decoder_reported"; break;
        case chromascope::VideoTimestampSource::EstimatedFromFPS:
            stream << "fps_estimate"; break;
        case chromascope::VideoTimestampSource::Unavailable:
            stream << "unavailable"; break;
    }
    stream << "\"\n  },\n  \"rotation_clockwise_degrees\": ";
    const auto rotation = source.rotation_info();
    if (rotation.clockwise_degrees) stream << *rotation.clockwise_degrees;
    else stream << "null";
    stream << ",\n  \"rotation_handling\": \"";
    switch (rotation.handling) {
        case chromascope::VideoRotationHandling::UprightOrUnreported:
            stream << "upright_or_unreported"; break;
        case chromascope::VideoRotationHandling::AppliedFromMetadata:
            stream << "applied_from_metadata"; break;
        case chromascope::VideoRotationHandling::DecoderManaged:
            stream << "decoder_managed"; break;
        case chromascope::VideoRotationHandling::InvalidMetadata:
            stream << "invalid_metadata"; break;
    }
    stream << "\",\n  \"audio_handling\": \"ignored\"";
    if (options.extract_frame)
        stream << ",\n  \"output\": \"" << escape_json(options.output.string()) << "\"";
    stream << "\n}\n";
}

int run_video_command(Options options) {
    // Validate paths before touching video data or creating any output files.
    const auto paths = chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, options.json, options.output_dir}, "video");
    options.output = paths.image;
    options.json = paths.json;
    chromascope::VideoSource source = chromascope::VideoSource::open(
        options.input, options.video_backend);
    if (options.extract_frame && *options.extract_frame != 0)
        source.seek_to(*options.extract_frame);
    auto result = source.read_next();
    if (result.status != chromascope::VideoReadStatus::Frame || !result.frame)
        throw std::runtime_error("Video does not contain the requested decodable frame");
    if (options.extract_frame && result.frame->index != *options.extract_frame)
        throw std::runtime_error("Video decoder returned an unexpected frame index");
    // Only commit output paths after decoding succeeded. Revalidate after the
    // explicit directory is created, matching the still-image CLI contract.
    chromascope::cli::prepare_output_directory(paths);
    (void)chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, options.json, {}}, "video");
    if (options.extract_frame)
        chromascope::write_image(options.output,
            chromascope::to_cv_mat_copy(result.frame->image));
    if (options.json_stdout) {
        write_video_json(std::cout, options, source, *result.frame);
    } else {
        std::cout << "Video: " << options.input.string() << "\n"
                  << "Frame: " << result.frame->index << "\n"
                  << "Dimensions: " << result.frame->image.width() << "x"
                  << result.frame->image.height() << "\n";
        if (auto fps = source.frames_per_second())
            std::cout << "FPS: " << std::fixed << std::setprecision(3) << *fps << "\n";
        else std::cout << "FPS: unknown\n";
        if (auto count = source.reported_frame_count())
            std::cout << "Reported frames: " << *count << "\n";
        else std::cout << "Reported frames: unknown\n";
        if (!options.output.empty()) std::cout << "Saved: " << options.output.string() << "\n";
        std::cout << "Audio: ignored (not decoded or synchronized)\n";
    }
    if (!options.json.empty()) {
        std::ofstream output(options.json, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot open JSON file: " + options.json.string());
        write_video_json(output, options, source, *result.frame);
        if (!output) throw std::runtime_error("Cannot write JSON file: " + options.json.string());
    }
    return 0;
}

void write_object_json(std::ostream& stream, const Options& options,
                       const chromascope::DetectionResult& result, double elapsed_ms,
                       const chromascope::DnnPostprocessSummary& summary,
                       const chromascope::RecordedVideoObjectDetection* video = nullptr) {
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6);
    stream << "{\n  \"schema_version\": 1,\n"
              "  \"application\": \"ChromascopeIP\",\n"
              "  \"profile\": \"object\",\n"
              "  \"processor\": \"nanodet-coco80\",\n"
           << "  \"input\": \"" << escape_json(options.input.string()) << "\",\n";
    if (video) {
        stream << "  \"source_kind\": \"recorded_video\",\n"
                  "  \"operation\": \"video_frame_detect\",\n"
               << "  \"frame_index\": " << video->frame_index << ",\n"
                  "  \"timestamp_ns\": ";
        if (const auto& stamp = video->inference.detections.timestamp())
            stream << stamp->value.count();
        else stream << "null";
        stream << ",\n  \"timestamp_source\": \"";
        switch (video->timestamp_source) {
            case chromascope::VideoTimestampSource::DecoderReported:
                stream << "decoder_reported"; break;
            case chromascope::VideoTimestampSource::EstimatedFromFPS:
                stream << "estimated_from_fps"; break;
            case chromascope::VideoTimestampSource::Unavailable:
                stream << "unavailable"; break;
        }
        stream << "\",\n";
    }
    stream << "  \"model\": \"" << escape_json(options.model.string()) << "\",\n"
           << "  \"model_sha256\": \"" << chromascope::kSelectedNanoDetSha256 << "\",\n"
              "  \"backend\": \"opencv-cpu\",\n"
           << "  \"width\": " << result.width() << ", \"height\": " << result.height() << ",\n"
           << "  \"elapsed_ms\": " << elapsed_ms << ",\n"
           << "  \"confidence_threshold\": " << options.object_options.confidence_threshold << ",\n"
           << "  \"nms_iou_threshold\": " << options.object_options.nms_iou_threshold << ",\n"
           << "  \"max_detections\": " << options.object_options.max_detections << ",\n"
           << "  \"uncertainty\": {\"status\": \"" << summary.status()
           << "\", \"diagnostic_floor\": " << options.object_options.uncertainty_floor
           << ", \"candidate_count\": " << summary.examined_candidates
           << ", \"weak_candidate_count\": " << summary.weak_candidates
           << ", \"note\": \"Weak candidates are not identified unknown objects\"},\n"
           << "  \"class_thresholds\": [";
    for (std::size_t i = 0; i < options.object_options.class_thresholds.size(); ++i) {
        const auto& entry = options.object_options.class_thresholds[i];
        stream << (i ? ", " : "") << "{\"class_id\": " << entry.class_id
               << ", \"label\": \""
               << escape_json(chromascope::nanodet_class_label(entry.class_id))
               << "\", \"threshold\": " << entry.threshold << "}";
    }
    stream << "],\n"
           << "  \"total\": " << result.objects().size() << ",\n"
              "  \"detections\": [";
    const auto& detections = result.objects();
    for (std::size_t i = 0; i < detections.size(); ++i) {
        const auto& obj = detections[i];
        const auto& box = std::get<chromascope::BoxGeometry>(obj.geometry);
        stream << (i ? ",\n" : "\n")
               << "    {\"class_id\": " << *obj.class_id
               << ", \"label\": \"" << escape_json(obj.label)
               << "\", \"score\": " << obj.score
               << ", \"geometry\": {\"type\": \"box\", \"x\": " << box.x
               << ", \"y\": " << box.y << ", \"width\": " << box.width
               << ", \"height\": " << box.height << "}}";
    }
    stream << (detections.empty() ? "" : "\n") << "  ]\n}\n";
}

int run_object_command(Options options) {
    // Validate outputs before decoding untrusted media or loading a model.
    const auto paths = chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, options.json, options.output_dir});
    options.output = paths.image;
    options.json = paths.json;
    if (!options.model.has_extension() || options.model.extension() != ".onnx")
        throw std::invalid_argument("--model must identify a local .onnx file");
    // Protect the model against --output/--json aliases, including hard links.
    (void)chromascope::cli::validate_output_paths(
        options.model, {options.output, {}, options.json, {}} , "model");
    if (chromascope::dnn_file_sha256(options.model) != chromascope::kSelectedNanoDetSha256)
        throw std::runtime_error("ONNX SHA-256 mismatch: expected the selected NanoDet artifact");

    const auto start = std::chrono::steady_clock::now();
    chromascope::CpuDnnAdapter adapter(options.model);
    if (chromascope::dnn_file_sha256(options.model) != chromascope::kSelectedNanoDetSha256)
        throw std::runtime_error("ONNX model changed while loading");
    const auto image = chromascope::read_image(options.input);
    const auto frame = chromascope::from_cv_mat(image, chromascope::PixelFormat::BGR8);
    const auto outcome = chromascope::infer_object_frame(frame, adapter, options.object_options);
    const auto& detected = outcome.detections;
    const auto& detection_summary = outcome.summary;
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    chromascope::cli::prepare_output_directory(paths);
    (void)chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, options.json, {}});
    (void)chromascope::cli::validate_output_paths(
        options.model, {options.output, {}, options.json, {}}, "model");
    if (!options.output.empty())
        chromascope::write_image(options.output, chromascope::render_annotations(
            image, chromascope::make_object_confidence_annotations(detected),
            {.show_outlines = true, .show_labels = options.labels}));
    if (options.json_stdout) {
        write_object_json(std::cout, options, detected, elapsed, detection_summary);
    } else {
        std::cout << "Profile: object (NanoDet COCO-80)\nInput: " << options.input.string()
                  << "\nModel: " << options.model.string()
                  << "\nDetected: " << detected.objects().size()
                  << "\nWeak candidates (not recognized objects): " << detection_summary.weak_candidates
                  << "\nElapsed: " << std::fixed << std::setprecision(2) << elapsed << " ms\n";
        if (!options.output.empty()) std::cout << "Saved: " << options.output.string() << "\n";
    }
    if (!options.json.empty()) {
        std::ofstream file(options.json, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot open JSON file: " + options.json.string());
        write_object_json(file, options, detected, elapsed, detection_summary);
        if (!file) throw std::runtime_error("Cannot write JSON file: " + options.json.string());
    }
    return 0;
}



// Make the existing, well-tested schema-v1 single-frame object result a
// JSONL payload without changing its fields or mishandling whitespace inside
// user-controlled JSON strings. The emitter writes exactly one physical line.
std::string compact_json(std::string_view value) {
    std::string compact;
    compact.reserve(value.size());
    bool quoted = false;
    bool escaped = false;
    for (const char c : value) {
        if (quoted) {
            compact.push_back(c);
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') {
            quoted = true;
            compact.push_back(c);
        } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            compact.push_back(c);
        }
    }
    if (quoted || escaped)
        throw std::runtime_error("Invalid unterminated JSON serialization");
    return compact;
}

void write_jsonl_line(std::ostream& stream, const std::string& line) {
    stream << line << '\n';
    stream.flush(); // Stream records immediately; do not buffer an entire video.
    if (!stream) throw std::runtime_error("Cannot write video JSONL output");
}

// Timings are stage-local CPU wall-clock measurements, not source media PTS.
// Report counts explicitly; unmeasured stages are null rather than zero ms.
std::string video_stage_json(const chromascope::VideoStageStatistics& stage) {
    if (!stage.measured()) return "null";
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(6)
       << "{\"samples\":" << stage.samples
       << ",\"total_ms\":" << stage.total_ms()
       << ",\"mean_ms\":" << stage.mean_ms()
       << ",\"max_ms\":" << static_cast<double>(stage.max_ns) / 1'000'000.0 << '}';
    return os.str();
}

std::string video_performance_json(const chromascope::VideoPerformanceMetrics& perf) {
    return "{\"decode\":" + video_stage_json(perf.decode) +
           ",\"inference\":" + video_stage_json(perf.inference) +
           ",\"overlay\":" + video_stage_json(perf.overlay) +
           ",\"encode\":" + video_stage_json(perf.encode) + '}';
}

std::string video_frame_timing_json(const chromascope::VideoFrameTiming& sample) {
    // One selected frame; decode excludes skipped frames in this record.
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(6)
       << "{\"decode_ms\":" << static_cast<double>(sample.decode_ns) / 1'000'000.0
       << ",\"inference_ms\":" << static_cast<double>(sample.inference_ns) / 1'000'000.0 << '}';
    return os.str();
}

int run_analyze_video_command(Options options) {
    // JSONL is the sole output for this operation. Resolve aliases against
    // both the input video and pinned model before decoding/writing anything.
    const auto paths = chromascope::cli::validate_output_paths(
        options.input, {{}, {}, options.jsonl, options.output_dir}, "video");
    options.jsonl = paths.json;
    (void)chromascope::cli::validate_output_paths(
        options.model, {{}, {}, options.jsonl, {}}, "model");
    // Fail before creating an output file on bad model/source inputs.
    chromascope::RecordedVideoObjectDetector detector(options.model, options.object_options);
    auto source = chromascope::VideoSource::open(options.input, options.video_backend);
    chromascope::cli::prepare_output_directory(paths);
    (void)chromascope::cli::validate_output_paths(
        options.input, {{}, {}, options.jsonl, {}}, "video");
    (void)chromascope::cli::validate_output_paths(
        options.model, {{}, {}, options.jsonl, {}}, "model");
    std::ofstream output;
    if (!options.jsonl_stdout) {
        output.open(options.jsonl, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot open JSONL file: " + options.jsonl.string());
    }
    std::ostream& sink = options.jsonl_stdout ? std::cout : output;
    sink.imbue(std::locale::classic());
    write_jsonl_line(sink,
        "{\"type\":\"start\",\"schema_version\":1,\"operation\":\"video_analyze\",\"input\":\"" +
        escape_json(options.input.string()) + "\",\"model_sha256\":\"" +
        std::string(chromascope::kSelectedNanoDetSha256) +
        "\",\"max_video_frames\":" + std::to_string(options.video_analysis.max_decoded_frames) +
        ",\"frame_step\":" + std::to_string(options.video_analysis.frame_step) + "}");

    chromascope::VideoFrameTiming last_sample{};
    std::optional<std::uint64_t> timed_frame;
    options.video_analysis.timing = [&](std::uint64_t index,
                                        const chromascope::VideoFrameTiming& sample) {
        last_sample = sample;
        timed_frame = index;
    };
    const auto summary = chromascope::analyze_recorded_video(
        source,
        [&](const chromascope::VideoFramePacket& packet) { return detector.detect(packet); },
        [&](const chromascope::RecordedVideoObjectDetection& result) {
            if (!timed_frame || *timed_frame != result.frame_index)
                throw std::runtime_error("Video analysis missing matched timing sample");
            std::ostringstream single;
            write_object_json(single, options, result.inference.detections,
                              result.elapsed_ms, result.inference.summary, &result);
            write_jsonl_line(sink, "{\"type\":\"frame\",\"record\":" +
                                   compact_json(single.str()) +
                                   ",\"timing_ms\":" + video_frame_timing_json(last_sample) + "}");
            timed_frame.reset();
        }, options.video_analysis);
    write_jsonl_line(sink,
        "{\"type\":\"end\",\"complete\":" +
        std::string(summary.reached_eof ? "true" : "false") +
        ",\"reason\":\"" + (summary.reached_eof ? "eof" : "frame_limit_reached") +
        "\",\"decoded_frames\":" + std::to_string(summary.decoded_frames) +
        ",\"analyzed_frames\":" + std::to_string(summary.analyzed_frames) +
        ",\"accepted_detections\":" + std::to_string(summary.accepted_detections) +
        ",\"weak_candidate_count\":" + std::to_string(summary.confidence.weak_candidates) +
        ",\"uncertain_only_frames\":" + std::to_string(summary.confidence.uncertain_only_frames) +
        ",\"performance\":" + video_performance_json(summary.performance) + "}");
    return summary.reached_eof ? 0 : 2; // Explicitly report that the scan was truncated.
}

int run_annotated_video_command(Options options) {
    const auto paths = chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, {}, options.output_dir}, "video");
    options.output = paths.image;
    (void)chromascope::cli::validate_output_paths(
        options.model, {options.output, {}, {}, {}}, "model");
    // Validate and load the approved model before creating output artifacts.
    chromascope::RecordedVideoObjectDetector detector(options.model, options.object_options);
    auto source = chromascope::VideoSource::open(options.input, options.video_backend);
    chromascope::cli::prepare_output_directory(paths);
    (void)chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, {}, {}}, "video");
    (void)chromascope::cli::validate_output_paths(
        options.model, {options.output, {}, {}, {}}, "model");
    const auto summary = chromascope::export_annotated_video(
        source, options.output,
        [&](const chromascope::VideoFramePacket& packet) { return detector.detect(packet); },
        {.max_decoded_frames = options.video_analysis.max_decoded_frames,
         .output_fps = options.output_fps, .show_labels = options.labels});
    std::cout << "Saved: " << options.output.string() << "\nFrames: " << summary.frames_written
              << "\nDetections: " << summary.accepted_detections
              << "\nWeak candidates (not accepted): " << summary.confidence.weak_candidates
              << "\nFrames with only weak candidates: " << summary.confidence.uncertain_only_frames
              << "\nCodec: MJPEG/AVI\nAudio: dropped (silent output)"
              << "\nTiming: constant-frame-rate " << summary.output_fps
              << " FPS (source VFR timing not preserved)\n"
              << "Stage timings (CPU wall-clock totals/means; no model loading, writer open/close or publication):\n";
    const auto print_stage = [](const char* name, const chromascope::VideoStageStatistics& stats) {
        std::cout << "  " << name << ": ";
        if (!stats.measured()) std::cout << "not measured\n";
        else std::cout << std::fixed << std::setprecision(3) << stats.total_ms()
                       << " ms total, " << stats.mean_ms() << " ms/frame ("
                       << stats.samples << " samples)\n";
    };
    print_stage("decode", summary.performance.decode);
    print_stage("inference", summary.performance.inference);
    print_stage("overlay", summary.performance.overlay);
    print_stage("encode", summary.performance.encode);
    return 0;
}

int run_video_object_command(Options options) {
    // Match the still-image CLI's output protection, plus the video source.
    const auto paths = chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, options.json, options.output_dir}, "video");
    options.output = paths.image;
    options.json = paths.json;
    (void)chromascope::cli::validate_output_paths(
        options.model, {options.output, {}, options.json, {}}, "model");

    // Exactly one trusted model load for this command; repeated frame processing
    // by callers of RecordedVideoObjectDetector reuses the same CPU network.
    chromascope::RecordedVideoObjectDetector detector(options.model, options.object_options);
    auto source = chromascope::VideoSource::open(options.input, options.video_backend);
    const auto index = *options.detect_video_frame;
    if (index != 0) source.seek_to(index);
    auto decoded = source.read_next();
    if (decoded.status != chromascope::VideoReadStatus::Frame || !decoded.frame ||
        decoded.frame->index != index)
        throw std::runtime_error("Video does not contain the requested decodable frame");
    const auto result = detector.detect(*decoded.frame);

    // Create outputs only AFTER successful inference; repeat path validation
    // after directory creation to avoid symlink/alias overwrites.
    chromascope::cli::prepare_output_directory(paths);
    (void)chromascope::cli::validate_output_paths(
        options.input, {options.output, {}, options.json, {}}, "video");
    (void)chromascope::cli::validate_output_paths(
        options.model, {options.output, {}, options.json, {}}, "model");
    if (!options.output.empty()) {
        const auto image = chromascope::to_cv_mat_copy(decoded.frame->image);
        chromascope::write_image(options.output, chromascope::render_annotations(
            image, chromascope::make_object_confidence_annotations(result.inference.detections),
            {.show_outlines = true, .show_labels = options.labels}));
    }
    if (options.json_stdout) {
        write_object_json(std::cout, options, result.inference.detections,
                          result.elapsed_ms, result.inference.summary, &result);
    } else {
        std::cout << "Recorded frame: " << result.frame_index
                  << "\nProfile: object (NanoDet COCO-80)\nDetected: "
                  << result.inference.detections.objects().size()
                  << "\nWeak candidates: " << result.inference.summary.weak_candidates
                  << "\nElapsed inference: " << result.elapsed_ms << " ms\n";
        if (!options.output.empty()) std::cout << "Saved: " << options.output.string() << "\n";
    }
    if (!options.json.empty()) {
        std::ofstream file(options.json, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot open JSON file: " + options.json.string());
        write_object_json(file, options, result.inference.detections,
                          result.elapsed_ms, result.inference.summary, &result);
        if (!file) throw std::runtime_error("Cannot write JSON file: " + options.json.string());
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--help" || std::string_view(argv[i]) == "-h") {
                usage(std::cout);
                return 0;
            }
            if (std::string_view(argv[i]) == "--version") {
                std::cout << "ChromascopeIP 0.1.3 (OpenCV " << CV_VERSION << ")\n";
                return 0;
            }
        }
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--list-object-classes") {
                if (argc != 2)
                    throw std::invalid_argument("--list-object-classes must be used alone");
                std::cout << "NanoDet COCO-80 supported classes (0..79):\n";
                for (int id = 0; id < chromascope::kNanoDetClassCount; ++id)
                    std::cout << id << "\t" << chromascope::nanodet_class_label(id) << "\n";
                std::cout << "Other objects are not recognized classes.\n";
                return 0;
            }
        }
        const auto registry = chromascope::make_builtin_registry();
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--list-profiles") {
                if (argc != 2)
                    throw std::invalid_argument("--list-profiles must be used without other arguments");
                for (const auto& descriptor : registry.descriptors()) {
                    std::cout << descriptor.id << " - " << descriptor.display_name << "\n"
                              << "  " << descriptor.description << "\n";
                    for (const auto& option : descriptor.options) {
                        std::cout << "  " << option.key << "=" << option.default_value
                                  << " " << (option.minimum_exclusive ? "(" : "[")
                                  << option.minimum << ", " << option.maximum
                                  << (option.maximum_exclusive ? ")" : "]")
                                  << (option.integral ? " integer" : "") << "\n";
                    }
                }
                std::cout << "object - NanoDet COCO-80 CPU detections (requires --model ONNX)\n"
                             "  --confidence=0.50 [0,1]\n  --nms-iou=0.60 [0,1]\n"
                             "  --max-detections=100 [1,512] integer\n";
                return 0;
            }
        }
        Options options = parse(argc, argv);
        if (options.analyze_video)
            return run_analyze_video_command(std::move(options));
        if (options.export_annotated_video)
            return run_annotated_video_command(std::move(options));
        if (options.detect_video_frame)
            return run_video_object_command(std::move(options));
        if (options.video_probe || options.extract_frame)
            return run_video_command(std::move(options));
        if (options.profile_id == "object")
            return run_object_command(std::move(options));
        // Resolve and validate profile settings before touching image data or outputs.
        chromascope::ProcessorProfile profile = options.profile_id == "skittles"
            ? chromascope::skittles_profile_from_settings(options.settings)
            : registry.default_profile(options.profile_id);
        for (const auto& [key, value] : options.profile_options)
            profile.numeric_options[key] = value;
        (void)registry.create(profile); // Includes cross-option processor validation.
        const bool legacy = options.profile_id == "skittles";
        const bool geometric = options.profile_id == "hsv_segmentation" ||
                               options.profile_id == "contour_detection";
        if (!options.mask_output.empty() && !geometric)
            throw std::invalid_argument("--mask-output requires a segmentation or contour profile");
        const auto paths = chromascope::cli::validate_output_paths(
            options.input, {options.output, options.mask_output, options.json, options.output_dir});
        options.output = paths.image;
        options.mask_output = paths.mask;
        options.json = paths.json;
        const cv::Mat image = chromascope::read_image(options.input);
        const auto processed = registry.run(profile,
            chromascope::from_cv_mat(image, chromascope::PixelFormat::BGR8));
        chromascope::cli::prepare_output_directory(paths);
        // Recheck after directory creation (e.g. a newly resolved path alias).
        (void)chromascope::cli::validate_output_paths(options.input,
            {options.output, options.mask_output, options.json, {}});
        if (!options.mask_output.empty()) {
            if (!geometric || !processed.output_frame)
                throw std::invalid_argument("--mask-output requires a segmentation or contour profile");
            chromascope::write_image(options.mask_output,
                                     chromascope::to_cv_mat_copy(*processed.output_frame));
        }
        if (!options.output.empty()) {
            const cv::Mat base = geometric ? image : (processed.output_frame
                ? chromascope::to_cv_mat_copy(*processed.output_frame) : image);
            chromascope::write_image(options.output,
                chromascope::render_annotations(
                    base, chromascope::make_annotation_layer(processed.detections),
                    {.show_outlines = true, .show_labels = options.labels}));
        }
        if (legacy) {
            const auto result = chromascope::to_legacy_skittles_result(
                processed.detections, processed.elapsed_ms);
            if (options.json_stdout) write_json(std::cout, options, result);
            else {
                std::cout << "Profile: skittles\nInput: " << options.input.string() << "\n"
                          << "Image: " << result.width << "x" << result.height << "\n"
                          << "Detected: " << result.objects.size() << "\n";
                for (std::size_t i = 0; i < chromascope::kColorCount; ++i) {
                    const auto color = static_cast<chromascope::CandyColor>(i);
                    std::cout << "  " << chromascope::color_name(color) << ": "
                              << result.counts[i] << "\n";
                }
                std::cout << std::fixed << std::setprecision(2)
                          << "Elapsed: " << result.elapsed_ms << " ms\n";
            }
            if (!options.json.empty()) {
                std::ofstream file(options.json, std::ios::binary);
                if (!file) throw std::runtime_error("Cannot open JSON file: " + options.json.string());
                write_json(file, options, result);
                if (!file) throw std::runtime_error("Cannot write JSON file: " + options.json.string());
            }
        } else {
            if (options.json_stdout) write_general_json(std::cout, options, processed);
            else {
                std::cout << "Profile: " << options.profile_id << "\nInput: " << options.input.string()
                          << "\nImage: " << image.cols << "x" << image.rows
                          << "\nDetected: " << processed.detections.objects().size() << "\n"
                          << std::fixed << std::setprecision(2)
                          << "Elapsed: " << processed.elapsed_ms << " ms\n";
            }
            if (!options.json.empty()) {
                std::ofstream file(options.json, std::ios::binary);
                if (!file) throw std::runtime_error("Cannot open JSON file: " + options.json.string());
                write_general_json(file, options, processed);
                if (!file) throw std::runtime_error("Cannot write JSON file: " + options.json.string());
            }
        }
        if (!options.output.empty() && !options.json_stdout)
            std::cout << "Saved: " << options.output.string() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "chromascope-cli: " << e.what() << "\n";
        return 1;
    }
}
