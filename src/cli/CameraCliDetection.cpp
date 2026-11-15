#include "CameraCliDetection.hpp"

#include "chromascope/CpuDnnAdapter.hpp"
#include "chromascope/DnnGuiSession.hpp"
#include "chromascope/StructuredAnnotations.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace chromascope::cli {
namespace {

volatile std::sig_atomic_t interrupt_requested = 0;
extern "C" void receive_sigint(int) noexcept { interrupt_requested = 1; }

class SignalGuard final {
public:
    SignalGuard() {
        interrupt_requested = 0;
        previous_ = std::signal(SIGINT, receive_sigint);
        if (previous_ == SIG_ERR) throw std::runtime_error("Cannot install Ctrl-C handler");
    }
    ~SignalGuard() { (void)std::signal(SIGINT, previous_); }
    SignalGuard(const SignalGuard&) = delete;
    SignalGuard& operator=(const SignalGuard&) = delete;
private:
    using Handler = void (*)(int);
    Handler previous_ = SIG_DFL;
};

std::string json_escape(std::string_view input) {
    std::string output;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : input) {
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 32) { output += "\\u00"; output += hex[c >> 4]; output += hex[c & 15]; }
                else output += static_cast<char>(c);
        }
    }
    return output;
}

void emit(std::ostream& out, std::string_view line) {
    out << line << '\n';
    out.flush();
    if (!out) throw std::runtime_error("Unable to write camera JSONL output");
}

std::string frame_record(const CameraFramePacket& packet, const ObjectFrameInference& inference,
                         double elapsed_ms, bool include_annotations = false) {
    const auto& result = inference.detections;
    const auto timestamp = packet.image.metadata().timestamp;
    if (!timestamp || timestamp->domain != TimestampDomain::CaptureMonotonic ||
        !result.timestamp() || result.timestamp()->domain != TimestampDomain::CaptureMonotonic ||
        result.timestamp()->value != timestamp->value ||
        result.width() != packet.image.width() || result.height() != packet.image.height())
        throw std::runtime_error("Live inference result does not match source frame");
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::fixed << std::setprecision(6);
    s << "{\"type\":\"frame\",\"index\":" << packet.index
      << ",\"capture_timestamp_ns\":" << timestamp->value.count()
      << ",\"width\":" << result.width() << ",\"height\":" << result.height()
      << ",\"inference_ms\":" << elapsed_ms
      << ",\"accepted_detections\":" << inference.summary.accepted_detections
      << ",\"weak_candidates\":" << inference.summary.weak_candidates
      << ",\"detections\":[";
    const auto& objects = result.objects();
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto& object = objects[i];
        if (!object.class_id || !std::holds_alternative<BoxGeometry>(object.geometry))
            throw std::runtime_error("Unsupported live detector output geometry");
        const auto& box = std::get<BoxGeometry>(object.geometry);
        if (i) s << ',';
        s << "{\"class_id\":" << *object.class_id << ",\"label\":\""
          << json_escape(object.label) << "\",\"score\":" << object.score
          << ",\"box\":{\"x\":" << box.x << ",\"y\":" << box.y
          << ",\"width\":" << box.width << ",\"height\":" << box.height << "}}";
    }
    s << ']';
    if (include_annotations)
        s << ",\"annotations\":" << structured_annotations_json(result);
    s << '}';
    return s.str();
}

// Use C11 exclusive create ("x") to prevent overwriting previous camera
// evidence. FILE* remains private; streambuf writes only finite records.
class ExclusiveFileBuffer final : public std::streambuf {
public:
    explicit ExclusiveFileBuffer(const std::filesystem::path& path) {
        if (path.empty()) throw std::invalid_argument("Empty JSONL destination");
        const auto native = path.string();
        file_ = std::fopen(native.c_str(), "wbx");
        if (!file_) throw std::runtime_error("JSONL destination must not exist; parent must exist and be writable");
    }
    ~ExclusiveFileBuffer() override { if (file_) (void)std::fclose(file_); }
    ExclusiveFileBuffer(const ExclusiveFileBuffer&) = delete;
    ExclusiveFileBuffer& operator=(const ExclusiveFileBuffer&) = delete;
protected:
    std::streamsize xsputn(const char* data, std::streamsize n) override {
        if (n <= 0) return 0;
        return static_cast<std::streamsize>(std::fwrite(data, 1, static_cast<std::size_t>(n), file_));
    }
    int overflow(int ch) override {
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        return std::fputc(ch, file_) == EOF ? traits_type::eof() : ch;
    }
    int sync() override { return std::fflush(file_) == 0 ? 0 : -1; }
private:
    std::FILE* file_ = nullptr;
};

} // namespace

int run_camera_detection_stream(const LiveCameraDetectionOptions& options, std::ostream& output,
                                LiveDetectFrame detect, std::unique_ptr<CameraFrameReader> reader,
                                LiveStopRequested stop_requested) {
    if (!detect) throw std::invalid_argument("Live detection needs a detector");
    if (options.camera.device_index < 0 || options.camera.device_index > 255 ||
        options.camera.max_frames == 0 || options.camera.max_frames > 300 ||
        options.camera.max_seconds == 0 || options.camera.max_seconds > 60)
        throw std::invalid_argument("Live capture limits are invalid");
    validate_dnn_postprocess_options(options.postprocess);
    if (options.profile != "object" && options.profile != "person")
        throw std::invalid_argument("Live detection profile must be object or person");
    if (options.postprocess.person_only != (options.profile == "person"))
        throw std::invalid_argument("Live detection profile mismatch");

    using Clock = std::chrono::steady_clock;
    AsyncCameraCapture capture;
    const auto begin = Clock::now();
    const auto generation = reader ? capture.start_with_reader(std::move(reader)) :
        capture.start(options.camera.device_index, options.camera.backend);
    const auto resolved = options.camera.backend == CameraBackend::PlatformDefault
        ? preferred_camera_backend() : options.camera.backend;
    emit(output, "{\"type\":\"start\",\"schema_version\":1,\"operation\":\"camera_detect\",\"backend\":\"" +
        std::string(camera_backend_name(resolved)) + "\",\"device_index\":" +
        std::to_string(options.camera.device_index) + ",\"profile\":\"" + options.profile +
        "\",\"model_sha256\":\"" + std::string(kSelectedNanoDetSha256) +
        "\",\"max_frames\":" + std::to_string(options.camera.max_frames) +
        ",\"max_seconds\":" + std::to_string(options.camera.max_seconds) + "}");
    std::uint32_t analyzed = 0;
    bool opened = false;
    std::string reason = "time_limit";
    int result_code = 0;
    try {
        while (camera_cli_within_limits(options.camera, analyzed, Clock::now() - begin)) {
            if (stop_requested && stop_requested()) { reason = "interrupted"; result_code = 130; break; }
            const auto event = capture.poll();
            if (!event) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (event->generation != generation) continue;
            if (event->kind == CameraCaptureEventKind::Error)
                throw std::runtime_error("Camera read/open error: " + event->message);
            if (event->kind == CameraCaptureEventKind::Opened) opened = true;
            if (event->kind != CameraCaptureEventKind::Frame || !event->frame) continue;
            const auto& frame = *event->frame;
            const auto start_forward = Clock::now();
            const auto prediction = detect(frame.image);
            const auto infer_ms = std::chrono::duration<double, std::milli>(Clock::now() - start_forward).count();
            if (stop_requested && stop_requested()) { reason = "interrupted"; result_code = 130; break; }
            if (options.include_annotations)
                emit(output, frame_record(frame, prediction, infer_ms, true));
            else
                emit(output, frame_record(frame, prediction, infer_ms));
            ++analyzed;
        }
        if (result_code == 0 && analyzed == options.camera.max_frames) reason = "frame_limit";
        if (result_code == 0 && analyzed == 0) {
            reason = "error";
            result_code = 2;
            std::cerr << (opened ? "Camera yielded no analyzable frames\n" :
                          "Camera failed to open before the capture deadline\n");
        }
    } catch (const std::exception& error) {
        reason = "error";
        result_code = 2;
        std::cerr << "Camera live detection failed: " << error.what() << '\n';
    }
    capture.stop(); // signal is cooperative; a native blocked open/read may outlast this call
    emit(output, "{\"type\":\"end\",\"complete\":" + std::string(result_code == 0 ? "true" : "false") +
        ",\"reason\":\"" + reason + "\",\"analyzed_frames\":" + std::to_string(analyzed) + "}");
    return result_code;
}

int run_camera_detection_cli(int argc, char* const argv[]) {
    auto options = parse_camera_detection_cli(argc, argv);
    // Verification and destination validation MUST finish before camera open.
    if (dnn_file_sha256(options.model) != kSelectedNanoDetSha256)
        throw std::runtime_error("Pinned NanoDet SHA-256 mismatch");
    CpuDnnAdapter model(options.model);
    if (dnn_file_sha256(options.model) != kSelectedNanoDetSha256)
        throw std::runtime_error("NanoDet model modified during load");
    if (!options.jsonl.empty()) {
        // Prevent model path and JSONL output from being the same file.
        std::error_code ec;
        if (std::filesystem::equivalent(options.model, options.jsonl, ec) && !ec)
            throw std::invalid_argument("JSONL output aliases model");
    }
    SignalGuard signals;
    const auto infer = [&](const ImageFrame& frame) {
        return infer_object_frame(frame, model, options.postprocess);
    };
    if (options.jsonl.empty()) {
        std::cout.imbue(std::locale::classic());
        return run_camera_detection_stream(options, std::cout, infer, nullptr,
            [] { return interrupt_requested != 0; });
    }
    ExclusiveFileBuffer file(options.jsonl);
    std::ostream output(&file);
    output.imbue(std::locale::classic());
    return run_camera_detection_stream(options, output, infer, nullptr,
        [] { return interrupt_requested != 0; });
}

} // namespace chromascope::cli
