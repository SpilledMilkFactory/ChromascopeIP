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
    std::filesystem::path input, output, json, mask_output, output_dir;
    int json_version = 1;
    bool json_stdout = false;
    bool labels = true;
    std::string profile_id = "skittles";
    chromascope::Settings settings;
    std::map<std::string, double, std::less<>> profile_options;
    bool video_probe = false;
    std::optional<std::uint64_t> extract_frame;
    chromascope::VideoBackend video_backend = chromascope::VideoBackend::Automatic;
    bool video_backend_explicit = false;
    bool legacy_settings_changed = false;
    std::set<std::string> legacy_keys;
};

void usage(std::ostream& os) {
    os << "ChromascopeIP 0.1.3 - image processing CLI\n"
          "Usage: chromascope-cli --input IMAGE [options]\n"
          "       chromascope-cli --video-probe --input VIDEO [--json FILE|-]\n"
          "       chromascope-cli --extract-frame INDEX --input VIDEO --output FRAME.png\n\n"
          "  -i, --input FILE       Input image or recorded-video file (required)\n"
          "      --video-probe     Inspect video metadata and first decoded frame\n"
          "      --extract-frame N  Extract zero-based video frame N to lossless PNG\n"
          "      --video-backend ID  Video backend: auto (default), opencv-mjpeg\n"
          "  -o, --output FILE      Save annotated image or processed raster\n"
          "      --mask-output FILE Export segmentation/contour mask separately\n"
          "      --json FILE|-     Export machine-readable JSON; - writes to stdout\n"
          "      --json-version N   JSON schema version (supported: 1; default: 1)\n"
          "      --output-dir DIR   Prefix relative output filenames; create DIR if needed\n"
          "      --mode skittles   Legacy alias for --profile skittles\n"
          "      --profile ID      Select a registered processing profile\n"
          "      --set KEY=VALUE   Override a named numeric processor option (repeatable)\n"
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
        } else if (flag == "--json-version") {
            once(flag, "json-version");
            o.json_version = parse_integer(next(i, argc, argv, flag));
        } else if (flag == "--video-probe") {
            once(flag, "video-action"); o.video_probe = true;
        } else if (flag == "--extract-frame") {
            once(flag, "video-action");
            o.extract_frame = chromascope::cli::parse_video_frame_index(next(i, argc, argv, flag));
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
    const bool video = o.video_probe || o.extract_frame.has_value();
    if (video) {
        if (single_use.contains("profile") || !o.profile_options.empty() ||
            o.legacy_settings_changed || single_use.contains("no-labels") ||
            !o.mask_output.empty())
            throw std::invalid_argument("Video commands cannot use still-image profile or annotation options");
        if (o.video_probe && !o.output.empty())
            throw std::invalid_argument("--video-probe does not accept --output; use --extract-frame");
        if (o.extract_frame) {
            if (o.output.empty())
                throw std::invalid_argument("--extract-frame requires --output FRAME.png");
            chromascope::cli::validate_video_png_path(o.output);
        }
    } else if (o.video_backend_explicit) {
        throw std::invalid_argument("--video-backend requires --video-probe or --extract-frame");
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
                return 0;
            }
        }
        Options options = parse(argc, argv);
        if (options.video_probe || options.extract_frame)
            return run_video_command(std::move(options));
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
