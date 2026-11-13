#include "chromascope/RecordedVideoAnalysis.hpp"
#include "support/RecordedVideoRegression.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {
namespace fs = std::filesystem;
using namespace chromascope;
using chromascope::testing::check;
namespace fixtures = chromascope::testing::video_regression;

void verify(const fs::path& dir) {
    fs::create_directories(dir);
    fixtures::ScopedFile movie{dir / "m6-5-seek-stream.avi"};
    fixtures::make_movie(movie.path, 8);
    auto source = VideoSource::open(movie.path, VideoBackend::OpenCVMJPEG);
    source.seek_to(1);

    std::vector<std::uint64_t> inferred;
    std::vector<std::uint64_t> emitted;
    std::uint64_t accepted = 0;
    std::uint64_t uncertain = 0;
    const auto detect = [&](const VideoFramePacket& packet) {
        inferred.push_back(packet.index);
        return fixtures::infer(packet, packet.index == 4 ? 0.40F : 0.90F);
    };
    const auto emit = [&](const RecordedVideoObjectDetection& item) {
        emitted.push_back(item.frame_index);
        accepted += item.inference.detections.objects().size();
        uncertain += item.inference.summary.weak_candidates;
    };
    const auto summary = analyze_recorded_video(source, detect, emit,
        {.max_decoded_frames = 10, .frame_step = 3});
    check(summary.reached_eof && summary.decoded_frames == 7 &&
          summary.analyzed_frames == 3 && summary.accepted_detections == 2,
          "Seeked stream reported incorrect EOF/counts");
    check((inferred == std::vector<std::uint64_t>{1, 4, 7}) &&
          emitted == inferred && accepted == 2 && uncertain == 1,
          "Frame-step analysis lost absolute indices or weak-candidate reporting");

    // Rewind the same decoder from EOF and ensure frame-step starts at the new
    // seek position, not at a leftover counter from the earlier analysis.
    source.seek_to(0);
    inferred.clear();
    emitted.clear();
    const auto again = analyze_recorded_video(source, detect, emit,
        {.max_decoded_frames = 9, .frame_step = 2});
    check(again.reached_eof && again.decoded_frames == 8 &&
          again.analyzed_frames == 4 && again.accepted_detections == 3,
          "Rewound stream did not reach EOF with the correct totals");
    check((emitted == std::vector<std::uint64_t>{0, 2, 4, 6}) &&
          inferred == emitted, "Rewound streaming selection differs from source frame indexes");

    // A decoded frame cap is explicitly incomplete even if its numerical
    // value equals the known frame count. A future JSONL caller must not
    // mistake frame-limit termination for a successful completion record.
    source.seek_to(0);
    const auto capped = analyze_recorded_video(source, detect,
        [](const RecordedVideoObjectDetection&) {},
        {.max_decoded_frames = 8, .frame_step = 2});
    check(!capped.reached_eof && capped.decoded_frames == 8 &&
          capped.analyzed_frames == 4, "Exact count cap was mistaken for decoder EOF");

    // A failing JSONL consumer must terminate immediately. The decoder must
    // not read ahead and no additional records may be emitted afterward.
    source.seek_to(2);
    int records = 0;
    bool rejected = false;
    try {
        (void)analyze_recorded_video(source, detect,
            [&](const RecordedVideoObjectDetection&) {
                ++records;
                throw std::runtime_error("synthetic downstream disk failure");
            });
    } catch (const std::runtime_error&) { rejected = true; }
    check(rejected && records == 1 && source.next_index() == 3,
          "Streaming output failure did not abort after exactly one decoded frame");

    // A result attached to a different source timestamp is invalid even when
    // its frame index and geometry are correct. Must fail before publication.
    source.seek_to(3);
    int published = 0;
    rejected = false;
    try {
        (void)analyze_recorded_video(source,
            [](const VideoFramePacket& frame) {
                auto result = fixtures::infer(frame);
                const auto& current = result.inference.detections.timestamp();
                if (current) {
                    FrameTimestamp shifted = *current;
                    shifted.value += std::chrono::nanoseconds{1};
                    result.inference.detections = DetectionResult::create(
                        frame.image.width(), frame.image.height(), "nanodet-coco80",
                        std::vector<GeneralDetection>(result.inference.detections.objects()), shifted);
                } else {
                    ++result.frame_index;
                }
                return result;
            },
            [&](const RecordedVideoObjectDetection&) { ++published; });
    } catch (const std::runtime_error&) { rejected = true; }
    check(rejected && published == 0,
          "Frame-associated JSONL record was emitted despite an incorrect timestamp");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M6.5 streamed detection / seek regression", [&] {
        if (argc != 2) throw std::invalid_argument("Expected build-tree artifact directory");
        verify(argv[1]);
    });
}
