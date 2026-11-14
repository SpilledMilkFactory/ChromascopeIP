#include "chromascope/AnnotatedVideoExport.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/RecordedVideoAnalysis.hpp"
#include "support/RecordedVideoRegression.hpp"
#include "support/TestSupport.hpp"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace {
using namespace chromascope;
namespace fs = std::filesystem;
namespace fixtures = chromascope::testing::video_regression;
using chromascope::testing::check;

RecordedVideoObjectDetection synthetic(const VideoFramePacket& packet, const DnnPostprocessOptions& p) {
    constexpr float confidence[] = {.91F, .96F, .65F, .78F, .95F};
    constexpr int classes[] = {0, 33, 0, 0, 33};
    if (packet.index >= 5) throw std::runtime_error("Invalid fixture frame index");
    const auto preprocessed = preprocess_dnn_frame(packet.image);
    return finalize_recorded_video_detection(packet, preprocessed.geometry,
        fixtures::tensors(confidence[packet.index], classes[packet.index]), p);
}
void run(const fs::path& directory) {
    fs::create_directories(directory);
    fixtures::ScopedFile video{directory / "m7-2-person-source.avi"};
    fixtures::ScopedFile annotated{directory / "m7-2-person-export.avi"};
    fixtures::make_movie(video.path, 5);
    DnnPostprocessOptions p;
    p.person_only = true;
    p.person_display_threshold = .75;
    p.uncertainty_floor = .35;
    auto source = VideoSource::open(video.path, VideoBackend::OpenCVMJPEG);
    std::uint64_t observed = 0;
    const auto streaming = analyze_recorded_video(source,
        [&](const VideoFramePacket& frame) { return synthetic(frame, p); },
        [&](const RecordedVideoObjectDetection& result) {
            check(result.frame_index == observed++, "person profile reordered video frames");
            const bool accepted = result.frame_index == 0 || result.frame_index == 3;
            check(result.inference.detections.objects().size() == (accepted ? 1U : 0U),
                  "person stream accepted nonperson or rejected matching person");
            if (accepted) {
                const auto& found = result.inference.detections.objects().front();
                check(found.class_id == 0 && found.label == "person",
                      "person stream returned a nonperson label");
            }
            check(result.inference.summary.weak_candidates == (result.frame_index == 2 ? 1U : 0U),
                  "person video diagnostics must exclude all nonperson classes");
        });
    check(streaming.reached_eof && observed == 5 && streaming.accepted_detections == 2 &&
          streaming.confidence.weak_candidates == 1 &&
          streaming.confidence.uncertain_only_frames == 1,
          "person streaming summaries inconsistent");

    auto export_source = VideoSource::open(video.path, VideoBackend::OpenCVMJPEG);
    const auto exported = export_annotated_video(export_source, annotated.path,
        [&](const VideoFramePacket& frame) { return synthetic(frame, p); });
    check(exported.frames_written == 5 && exported.accepted_detections == 2 &&
          exported.confidence.weak_candidates == 1 && fs::exists(annotated.path),
          "person-only AVI export diverged from streaming results");

    source.seek_to(0);
    p.person_display_threshold = .60;
    auto replay = source.read_next();
    check(replay.frame && replay.frame->index == 0, "replay seek failed");
    source.seek_to(2);
    auto selected = source.read_next();
    check(selected.frame && selected.frame->index == 2, "nonsequential frame seek failed");
    const auto lowered = synthetic(*selected.frame, p);
    check(lowered.inference.detections.objects().size() == 1 &&
          lowered.inference.detections.objects()[0].class_id == 0,
          "lowered person display cutoff did not admit candidate in seeked frame");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M7.2 person-only streaming and AVI", [&] {
        if (argc != 2) throw std::invalid_argument("Expected synthetic fixture directory");
        run(argv[1]);
    });
}
