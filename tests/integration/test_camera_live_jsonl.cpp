#include "CameraCliDetection.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace chromascope;
using namespace std::chrono_literals;
using chromascope::testing::check;

class FakeReader final : public CameraFrameReader {
public:
    CameraFramePacket read() override {
        std::this_thread::sleep_for(5ms);
        const auto value = ++index_;
        FrameMetadata metadata;
        metadata.timestamp = FrameTimestamp{std::chrono::milliseconds(value * 20), TimestampDomain::CaptureMonotonic};
        return {value, ImageFrame::from_owned_bytes(8, 8, PixelFormat::BGR8, 24,
            std::vector<std::uint8_t>(192, static_cast<std::uint8_t>(value)), metadata)};
    }
private:
    std::uint64_t index_ = 0;
};

std::size_t occurrences(const std::string& text, const std::string& term) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(term, position)) != std::string::npos) {
        ++count; position += term.size();
    }
    return count;
}

ObjectFrameInference detect(const ImageFrame& image) {
    GeneralDetection object;
    object.geometry = BoxGeometry{1.0, 2.0, 3.0, 4.0};
    object.class_id = 0;
    object.label = "person";
    object.score = 0.75;
    auto detections = DetectionResult::create(image.width(), image.height(), "nanodet-coco80",
        {object}, image.metadata().timestamp);
    DnnPostprocessSummary summary;
    summary.accepted_detections = 1;
    return {std::move(detections), summary};
}

void exercise() {
    chromascope::cli::LiveCameraDetectionOptions options;
    options.camera.device_index = 0;
    options.camera.max_frames = 3;
    options.camera.max_seconds = 3;
    options.profile = "person";
    options.postprocess.person_only = true;
    std::ostringstream output;
    const int rc = chromascope::cli::run_camera_detection_stream(
        options, output, detect, std::make_unique<FakeReader>());
    check(rc == 0, "finite stream failed");
    const std::string text = output.str();
    check(occurrences(text, "\"type\":\"start\"") == 1 &&
          occurrences(text, "\"type\":\"frame\"") == 3 &&
          occurrences(text, "\"type\":\"end\"") == 1 &&
          text.find("\"reason\":\"frame_limit\"") != std::string::npos,
          "stream missing deterministic records");
    check(occurrences(text, "\"capture_timestamp_ns\"") == 3 &&
          occurrences(text, "\"label\":\"person\"") == 3,
          "detection records missing source timestamps or COCO labels");
    check(text.find("\"model_sha256\"") != std::string::npos &&
          text.find("\"analyzed_frames\":3") != std::string::npos,
          "metadata and summary missing");
    std::size_t line_count = 0;
    for (const char c : text) if (c == '\n') ++line_count;
    check(line_count == 5, "JSONL must emit one physical line per record");

    // Cancellation follows the signal handler contract without any actual signal.
    std::uint32_t requests = 0;
    options.camera.max_frames = 100;
    std::ostringstream interrupted;
    const int canceled = chromascope::cli::run_camera_detection_stream(
        options, interrupted, detect, std::make_unique<FakeReader>(),
        [&] { return ++requests > 8; });
    check(canceled == 130 && occurrences(interrupted.str(), "\"type\":\"end\"") == 1 &&
          interrupted.str().find("\"reason\":\"interrupted\"") != std::string::npos,
          "interrupt failed to emit an end record");

    // Reject altered source timestamps; fail closed with a final error record.
    std::ostringstream mismatch;
    auto wrong = [](const ImageFrame& image) {
        FrameMetadata metadata;
        metadata.timestamp = FrameTimestamp{10s, TimestampDomain::CaptureMonotonic};
        auto bogus = DetectionResult::create(image.width(), image.height(), "nanodet-coco80", {}, metadata.timestamp);
        return ObjectFrameInference{std::move(bogus), {}};
    };
    const int bad = chromascope::cli::run_camera_detection_stream(
        options, mismatch, wrong, std::make_unique<FakeReader>());
    check(bad == 2 && occurrences(mismatch.str(), "\"type\":\"frame\"") == 0 &&
          mismatch.str().find("\"reason\":\"error\"") != std::string::npos,
          "mismatched source frame leaked into JSONL");
}
} // namespace
int main() { return chromascope::testing::run("M9.6 camera JSONL lifecycle", exercise); }
