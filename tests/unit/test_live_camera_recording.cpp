#include "chromascope/LiveCameraRecording.hpp"

#include <cmath>
#include <memory>
#include <vector>
#include <utility>
#include <chrono>
#include <opencv2/core.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Exception, class Function>
void rejects(Function&& function, const char* message) {
    try { function(); } catch (const Exception&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    using namespace chromascope;
    namespace fs = std::filesystem;
    try {
        LiveCameraRecorder recorder;
        require(recorder.status().stage == LiveRecordingStage::Idle, "unexpected initial stage");
        require(!recorder.accepting(), "recorder started implicitly");
        recorder.stop();
        require(!recorder.accepting(), "idle Stop unexpectedly started recorder");
        require(resolve_live_recording_path("example", LiveRecordingFormat::AVI).extension() == ".avi",
                "AVI extension not appended");
        require(resolve_live_recording_path("example.mp4", LiveRecordingFormat::MP4).extension() == ".mp4",
                "MP4 extension changed");
        for (const auto fmt : {LiveRecordingFormat::AVI, LiveRecordingFormat::MP4}) {
            const auto generated = default_live_recording_path(fmt);
            require(generated.filename().string().find("ChromascopeIP-Live-") == 0,
                    "timestamped default name missing");
            require(generated.extension() == (fmt == LiveRecordingFormat::AVI ? ".avi" : ".mp4"),
                    "generated filename does not match selected format");
        }
        rejects<std::invalid_argument>([] {
            (void)resolve_live_recording_path("example.mp4", LiveRecordingFormat::AVI);
        }, "incorrect extension accepted");
        rejects<std::invalid_argument>([&] {
            recorder.start("not-used.avi", LiveRecordingFormat::AVI, 0.0);
        }, "zero FPS accepted");
        rejects<std::invalid_argument>([&] {
            recorder.start("not-used.avi", LiveRecordingFormat::AVI, std::nan(""));
        }, "NaN FPS accepted");
        const auto missing = fs::temp_directory_path() / "chromascope-recording-missing-folder" / "sample.avi";
        rejects<std::invalid_argument>([&] {
            recorder.start(missing, LiveRecordingFormat::AVI, 30.0);
        }, "missing output directory accepted");
        require(!recorder.accepting(), "invalid recording path activated recorder");
        // Camera-free, pixel-level regression: live recording follows the
        // displayed analyzed image and only burns frame-matched visible boxes.
        auto frame = [](std::uint64_t index, std::uint8_t fill) {
            FrameMetadata meta;
            meta.timestamp = FrameTimestamp{std::chrono::milliseconds(index),
                                            TimestampDomain::CaptureMonotonic};
            return CameraFramePacket{index, ImageFrame::from_owned_bytes(160, 120,
                PixelFormat::BGR8, 480, std::vector<std::uint8_t>(57600, fill), meta)};
        };
        const auto raw_frame = frame(9, 17);
        const auto analyzed = frame(7, 29);
        auto detected = DetectionResult::create(160, 120, "nanodet-coco80",
            {{BoxGeometry{30, 40, 60, 40}, "person", 0.91, 0}},
            analyzed.image.metadata().timestamp);
        auto published = std::make_shared<LiveInferenceResult>(LiveInferenceResult{
            11, 22, analyzed, ObjectFrameInference{std::move(detected), {}}, 2.0});
        LiveRecordingPresentation showing;
        showing.displayed = published;
        showing.visible = {22, 11, 7, 160, 120, std::chrono::milliseconds(7)};
        showing.show_boxes = true;
        showing.show_labels = true;
        showing.minimum_score = 0.5;
        const cv::Mat plain = render_live_recording_pixels(raw_frame);
        require(plain.at<cv::Vec3b>(0, 0)[0] == 17,
                "camera-off/inference-off recording did not preserve raw pixels");
        const cv::Mat painted = render_live_recording_pixels(raw_frame, showing);
        require(painted.at<cv::Vec3b>(0, 0)[0] == 29,
                "recording did not follow currently displayed analyzed pixels");
        const cv::Mat analyzed_plain(painted.size(), painted.type(), cv::Scalar::all(29));
        require(cv::norm(painted, analyzed_plain, cv::NORM_INF) > 0,
                "enabled live inference did not burn any detection pixels");
        require(raw_frame.image.pixels()[0] == 17 &&
                analyzed.image.pixels()[0] == 29,
                "burn-in modified the original owned camera/inference pixels");
        auto no_labels = showing;
        no_labels.show_labels = false;
        const auto outline_only = render_live_recording_pixels(raw_frame, no_labels);
        require(cv::norm(outline_only, analyzed_plain, cv::NORM_INF) > 0,
                "hiding labels also removed the visible box outline");
        require(cv::norm(outline_only, painted, cv::NORM_INF) > 0,
                "label toggle did not change the recorded visual output");
        const auto unannotated = [&] {
            auto hidden = showing;
            hidden.show_boxes = false;
            return render_live_recording_pixels(raw_frame, hidden);
        }();
        require(cv::norm(unannotated, analyzed_plain, cv::NORM_INF) == 0,
                "hiding bounding boxes did not remove burn-in");
        auto gated = showing;
        gated.minimum_score = 0.95;
        require(cv::norm(render_live_recording_pixels(raw_frame, gated),
                         analyzed_plain, cv::NORM_INF) == 0,
                "recording confidence gate not respected");
        auto mislabeled = showing;
        mislabeled.visible.frame_index += 1;
        require(cv::norm(render_live_recording_pixels(raw_frame, mislabeled), plain,
                         cv::NORM_INF) == 0,
                "stale/incorrect source frame leaked an overlay into recording");
        mislabeled = showing;
        mislabeled.visible.inference_session += 1;
        require(cv::norm(render_live_recording_pixels(raw_frame, mislabeled), plain,
                         cv::NORM_INF) == 0,
                "superseded inference session leaked an overlay");
        mislabeled = showing;
        mislabeled.visible.camera_generation += 1;
        require(cv::norm(render_live_recording_pixels(raw_frame, mislabeled), plain,
                         cv::NORM_INF) == 0,
                "wrong camera generation leaked an overlay");
        std::cout << "PASS: opt-in recording destination/format/worker policy\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Live recording policy failed: " << error.what() << '\n';
        return 1;
    }
}
