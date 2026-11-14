#include "CameraCliCapture.hpp"
#include "CameraCliContract.hpp"

#include "chromascope/AsyncCameraCapture.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace chromascope::cli {
namespace {

void validate_save_directory(const std::filesystem::path& path) {
    if (path.empty()) return;
    namespace fs = std::filesystem;
    std::error_code ec;
    // Reject any existing file, directory, or symlink BEFORE camera activation.
    const auto status = fs::symlink_status(path, ec);
    if (!ec && status.type() != fs::file_type::not_found)
        throw std::invalid_argument("--camera-save-dir must not exist (create-only)");
    if (ec && ec != std::errc::no_such_file_or_directory)
        throw std::invalid_argument("--camera-save-dir cannot be inspected");
    const auto parent = path.has_parent_path() ? path.parent_path() : fs::path(".");
    if (!fs::is_directory(parent))
        throw std::invalid_argument("--camera-save-dir parent must already exist");
}

void write_frame(const std::filesystem::path& directory,
                 const CameraFramePacket& packet, std::uint32_t count) {
    std::ostringstream name;
    name << "camera-frame-" << std::setw(6) << std::setfill('0') << count << ".png";
    const auto filename = directory / name.str();
    // Each saved image is inside a directory created exclusively by this
    // invocation; no pre-existing files are overwritten by intentional reuse.
    write_image(filename, to_cv_mat_copy(packet.image));
}

} // namespace

int run_camera_capture(const CameraCliOptions& options,
                       std::unique_ptr<CameraFrameReader> reader) {
    using clock = std::chrono::steady_clock;
    validate_save_directory(options.save_directory);

    // Camera access is only requested after all CLI and destination validation.
    // Worker is the sole owner of native open/read/close operations.
    AsyncCameraCapture capture;
    const auto start = clock::now();
    const auto generation = reader
        ? capture.start_with_reader(std::move(reader))
        : capture.start(options.device_index, options.backend);
    std::uint32_t frames = 0;
    std::optional<std::chrono::nanoseconds> last_timestamp;
    bool opened = false;
    bool created_directory = false;
    while (camera_cli_within_limits(options, frames, clock::now() - start)) {
        const auto event = capture.poll();
        if (!event) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (event->generation != generation) continue;
        if (event->kind == CameraCaptureEventKind::Error) {
            throw std::runtime_error("Camera capture " +
                std::string(camera_error_name(event->error_code.value_or(CameraErrorCode::ReadFailed))) +
                ": " + event->message);
        }
        if (event->kind == CameraCaptureEventKind::Opened) opened = true;
        if (event->kind != CameraCaptureEventKind::Frame || !event->frame) continue;
        // Retain only this packet at a time; no unbounded frame accumulation.
        const auto& packet = *event->frame;
        const auto& timestamp = packet.image.metadata().timestamp;
        if (!timestamp || timestamp->domain != TimestampDomain::CaptureMonotonic)
            throw std::runtime_error("Camera capture returned an invalid timestamp");
        if (!options.save_directory.empty() && !created_directory) {
            if (!std::filesystem::create_directory(options.save_directory))
                throw std::runtime_error("Camera output directory was created by another process");
            created_directory = true;
        }
        if (created_directory) write_frame(options.save_directory, packet, frames + 1);
        last_timestamp = timestamp->value;
        ++frames;
    }
    capture.stop();
    if (frames == 0) {
        throw std::runtime_error(opened
            ? "Camera capture elapsed without a decoded frame"
            : "Camera did not produce a frame before the elapsed-time limit");
    }
    const auto resolved_backend = options.backend == CameraBackend::PlatformDefault
        ? preferred_camera_backend() : options.backend;
    std::cout << "Camera capture complete (local, explicit request)\n"
              << "Backend: " << camera_backend_name(resolved_backend) << "\n"
              << "Device index: " << options.device_index << "\n"
              << "Frames received: " << frames << "\n"
              << "Frame limit: " << options.max_frames << "\n"
              << "Elapsed-time limit: " << options.max_seconds << " seconds\n"
              << "Latest capture timestamp ns: " << last_timestamp->count() << "\n";
    if (created_directory)
        std::cout << "PNG frames saved in: " << options.save_directory.string() << "\n";
    else std::cout << "Frames not saved (no --camera-save-dir)\n";
    // Resource release is cooperative. AsyncCameraCapture's destructor joins
    // worker; a blocked native open/read is not covered by the elapsed limit.
    return 0;
}

int run_camera_cli(int argc, char* const argv[]) {
    return run_camera_capture(parse_camera_cli(argc, argv));
}

} // namespace chromascope::cli
