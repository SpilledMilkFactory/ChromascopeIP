#include "chromascope/Detection.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"

#include <opencv2/core/version.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <type_traits>

namespace {
struct Options {
    std::filesystem::path input, output, json, mask_output;
    bool json_stdout = false;
    bool labels = true;
    std::string profile_id = "skittles";
    chromascope::Settings settings;
    std::map<std::string, double, std::less<>> profile_options;
    bool legacy_settings_changed = false;
};

void usage(std::ostream& os) {
    os << "ChromascopeIP 0.1.3 - image processing CLI\n"
          "Usage: chromascope-cli --input IMAGE [options]\n\n"
          "  -i, --input FILE       Input JPG/PNG/BMP/TIFF image (required)\n"
          "  -o, --output FILE      Save annotated image or processed raster\n"
          "      --mask-output FILE Export segmentation/contour mask separately\n"
          "      --json FILE|-     Export machine-readable JSON; - writes to stdout\n"
          "      --mode skittles   Legacy alias for --profile skittles\n"
          "      --profile ID      Select a registered processing profile\n"
          "      --set KEY=VALUE   Override a named numeric processor option (repeatable)\n"
          "      --list-profiles   List registered processors and default options\n"
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
    return value;
}

Options parse(int argc, char* argv[]) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "-i" || flag == "--input") o.input = next(i, argc, argv, flag);
        else if (flag == "-o" || flag == "--output") o.output = next(i, argc, argv, flag);
        else if (flag == "--mask-output") o.mask_output = next(i, argc, argv, flag);
        else if (flag == "--json") {
            const auto value = next(i, argc, argv, flag);
            if (value == "-") o.json_stdout = true;
            else o.json = value;
        } else if (flag == "--mode" || flag == "--profile") {
            o.profile_id = next(i, argc, argv, flag);
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
            o.legacy_settings_changed = true;
            o.settings.accumulator_threshold = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--edge") {
            o.legacy_settings_changed = true;
            o.settings.edge_threshold = parse_real(next(i, argc, argv, flag));
        } else if (flag == "--max-dimension") {
            o.legacy_settings_changed = true;
            o.settings.max_dimension = parse_integer(next(i, argc, argv, flag));
        } else if (flag == "--min-confidence") {
            o.legacy_settings_changed = true;
            o.settings.minimum_confidence = parse_real(next(i, argc, argv, flag));
        }
        else if (flag == "--no-labels") o.labels = false;
        else throw std::invalid_argument("Unknown argument: " + std::string(flag));
    }
    if (o.input.empty()) throw std::invalid_argument("--input is required");
    if (o.profile_id == "skittles") chromascope::validate_settings(o.settings);
    else if (o.legacy_settings_changed)
        throw std::invalid_argument("Legacy Skittles flags cannot be used with other profiles; use --set");
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
    stream << "{\n  \"application\": \"ChromascopeIP\",\n"
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
// The legacy Skittles JSON schema above is unchanged. Generic processors
// report owned detection geometry without assigning semantic class names.
void write_general_json(std::ostream& stream, const Options& options,
                        const chromascope::ProcessedFrame& processed) {
    const auto& result = processed.detections;
    stream << std::fixed << std::setprecision(3);
    stream << "{\n  \"application\": \"ChromascopeIP\",\n"
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
                    for (const auto& option : descriptor.options)
                        std::cout << "  " << option.key << "=" << option.default_value << "\n";
                }
                return 0;
            }
        }
        const Options options = parse(argc, argv);
        const cv::Mat image = chromascope::read_image(options.input);
        chromascope::ProcessorProfile profile = options.profile_id == "skittles"
            ? chromascope::skittles_profile_from_settings(options.settings)
            : registry.default_profile(options.profile_id);
        for (const auto& [key, value] : options.profile_options)
            profile.numeric_options[key] = value;
        const auto processed = registry.run(profile,
            chromascope::from_cv_mat(image, chromascope::PixelFormat::BGR8));
        const bool legacy = options.profile_id == "skittles";
        const bool geometric = options.profile_id == "hsv_segmentation" ||
                               options.profile_id == "contour_detection";
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
