#include "chromascope/AnnotatedVideoExport.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AsyncRecordedVideoDetection.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/RecordedVideoAnalysis.hpp"
#include "support/RecordedVideoRegression.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace chromascope;
using chromascope::testing::check;
namespace fixtures = chromascope::testing::video_regression;

// Real pinned NanoDet head decoder, but synthetic forward outputs; no third-
// party weight bytes or downloaded video needed for offline Linux/Windows CI.
RecordedVideoObjectDetection classify(const VideoFramePacket& packet,
                                      const DnnPostprocessOptions& policy) {
    constexpr float scores[] = {0.92F, 0.42F, 0.91F, 0.68F, 0.92F, 0.10F};
    constexpr int classes[] = {0, 0, 33, 33, 0, 33};
    if (packet.index >= 6) throw std::runtime_error("Unexpected synthetic frame");
    const auto prepared = preprocess_dnn_frame(packet.image);
    return finalize_recorded_video_detection(packet, prepared.geometry,
        fixtures::tensors(scores[packet.index], classes[packet.index]), policy);
}

void wait_for(const std::function<bool()>& done) {
    using namespace std::chrono_literals;
    for (int n = 0; n < 400; ++n) {
        if (done()) return;
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("Timed out awaiting synthetic video confidence analysis");
}

void run(const fs::path& directory) {
    fs::create_directories(directory);
    fixtures::ScopedFile input{directory / "m6-7-confidence-input.avi"};
    fixtures::ScopedFile output{directory / "m6-7-confidence-output.avi"};
    fixtures::make_movie(input.path, 6);
    DnnPostprocessOptions policy;
    policy.class_thresholds = {{33, 0.93}}; // Kite is deliberately conservative.
    policy.confidence_threshold = 0.50;
    policy.uncertainty_floor = 0.35;
    validate_dnn_postprocess_options(policy);

    AsyncRecordedVideoDetection analyzer([](const fs::path&, const DnnPostprocessOptions& p) {
        return RecordedFrameDetector([p](const VideoFramePacket& packet) {
            return classify(packet, p);
        });
    });
    (void)analyzer.start(input.path, "synthetic-not-a-model.onnx", policy, VideoBackend::OpenCVMJPEG);
    wait_for([&] { return analyzer.snapshot().stage == VideoDetectionStage::Completed; });
    const auto progress = analyzer.snapshot();
    check(progress.processed_frames == 6 && progress.confidence.observed_frames == 6 &&
          progress.confidence.weak_candidates == 3 &&
          progress.confidence.uncertain_only_frames == 3,
          "GUI asynchronous analysis did not count weak/uncertain frames correctly");
    auto source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    for (std::uint64_t i = 0; i < 6; ++i) {
        auto decoded = source.read_next();
        check(decoded.status == VideoReadStatus::Frame && decoded.frame &&
              decoded.frame->index == i, "Synthetic frame identity mismatch");
        const auto matched = analyzer.overlay_for(*decoded.frame);
        check(matched && matched->frame_index == i, "Missing frame-matched diagnostic snapshot");
        const auto& detections = matched->inference.detections;
        const auto& summary = matched->inference.summary;
        check(summary.weak_candidates == ((i == 1 || i == 2 || i == 3) ? 1U : 0U),
              "Per-class cutoff, confidence floor, or weak candidate count changed");
        check(detections.objects().size() == ((i == 0 || i == 4) ? 1U : 0U),
              "Only strong person candidates should survive video policy");
        check(summary.status() == ((i == 0 || i == 4) ? "accepted" :
                (i == 5 ? "no_accepted_detection" : "uncertain")),
              "Frame-specific status differs from M5.8 policy");
        if (i == 0) {
            const auto overlay = make_object_confidence_annotations(detections);
            check(overlay.items().size() == 1 && overlay.items()[0].text == "person (92%)" &&
                  overlay.items()[0].text == object_confidence_label(detections.objects()[0]),
                  "GUI confidence label must agree with encoded AVI annotation");
        }
    }

    auto stream_source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    const auto streamed = analyze_recorded_video(stream_source,
        [&](const VideoFramePacket& frame) { return classify(frame, policy); },
        [](const RecordedVideoObjectDetection&) {});
    check(streamed.reached_eof && streamed.analyzed_frames == 6 &&
          streamed.confidence.observed_frames == 6 &&
          streamed.confidence.weak_candidates == 3 &&
          streamed.confidence.uncertain_only_frames == 3,
          "Streaming JSONL policy diagnostics must match GUI frame analysis");

    auto exporter_source = VideoSource::open(input.path, VideoBackend::OpenCVMJPEG);
    auto summary = export_annotated_video(exporter_source, output.path,
        [&](const VideoFramePacket& frame) { return classify(frame, policy); },
        {.max_decoded_frames = 100});
    check(fs::exists(output.path) && summary.frames_written == 6 &&
          summary.accepted_detections == 2 &&
          summary.confidence.observed_frames == 6 &&
          summary.confidence.weak_candidates == 3 &&
          summary.confidence.uncertain_only_frames == 3,
          "AVI export diagnostics do not match GUI analysis policy");
    analyzer.cancel();
    const auto canceled = analyzer.snapshot();
    check(canceled.confidence.observed_frames == 0 &&
          canceled.confidence.weak_candidates == 0 &&
          canceled.confidence.uncertain_only_frames == 0,
          "Canceled analysis retained previous recording diagnostics");
    // Changing diagnostic floor should change weak reporting, not accepted boxes.
    policy.uncertainty_floor = 0.70;
    (void)analyzer.start(input.path, "synthetic-not-a-model.onnx", policy, VideoBackend::OpenCVMJPEG);
    wait_for([&] { return analyzer.snapshot().stage == VideoDetectionStage::Completed; });
    const auto revised = analyzer.snapshot();
    check(revised.confidence.observed_frames == 6 &&
          revised.confidence.weak_candidates == 1 &&
          revised.confidence.uncertain_only_frames == 1,
          "Diagnostic floor must not alter accepted detections or retain old evidence");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.7 recorded-video confidence parity", [&] {
        if (argc != 2) throw std::invalid_argument("Expected build-tree fixture directory");
        run(argv[1]);
    });
}
