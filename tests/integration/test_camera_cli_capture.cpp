#include "CameraCliCapture.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
using namespace chromascope;

class FakeCamera final : public CameraFrameReader {
public:
    explicit FakeCamera(bool fail = false) : fail_(fail) {}
    CameraFramePacket read() override {
        std::this_thread::sleep_for(4ms);
        if (fail_) throw CameraError(CameraErrorCode::ReadFailed, "synthetic unplug");
        const auto frame_index = index_++;
        FrameMetadata metadata;
        metadata.timestamp = FrameTimestamp{std::chrono::milliseconds(frame_index),
                                             TimestampDomain::CaptureMonotonic};
        return {frame_index, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
                    std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(frame_index)),
                    metadata)};
    }
private:
    bool fail_ = false;
    std::uint64_t index_ = 0;
};
void require(bool ok, const char* detail) {
    if (!ok) throw std::runtime_error(detail);
}
} // namespace

int main() {
    try {
        namespace fs = std::filesystem;
        auto opts = chromascope::cli::CameraCliOptions{};
        opts.device_index = 0;
        opts.max_frames = 3;
        opts.max_seconds = 3;
        require(chromascope::cli::run_camera_capture(opts, std::make_unique<FakeCamera>()) == 0,
                "synthetic in-memory capture failed");

        const auto root = fs::current_path() / "m8-5-cli-test-output";
        fs::remove_all(root);
        opts.save_directory = root;
        require(chromascope::cli::run_camera_capture(opts, std::make_unique<FakeCamera>()) == 0,
                "synthetic PNG capture failed");
        require(fs::is_regular_file(root / "camera-frame-000001.png") &&
                fs::is_regular_file(root / "camera-frame-000002.png") &&
                fs::is_regular_file(root / "camera-frame-000003.png"),
                "explicit PNG output incomplete");
        require(!fs::exists(root / "camera-frame-000004.png"), "capture exceeded max frames");

        bool rejected = false;
        try { (void)chromascope::cli::run_camera_capture(opts, std::make_unique<FakeCamera>()); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "existing output directory was not rejected");
        fs::remove_all(root);
        rejected = false;
        try { (void)chromascope::cli::run_camera_capture(opts, std::make_unique<FakeCamera>(true)); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected && !fs::exists(root),
                "read failure must not create output directory");
        return 0;
    } catch (const std::exception& error) {
        // CTest includes the program error text on failure.
        (void)error;
        return 1;
    }
}
