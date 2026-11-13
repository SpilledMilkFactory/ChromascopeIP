#pragma once

#include "chromascope/RecordedVideoObjectDetection.hpp"
#include "chromascope/VideoPerformance.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace chromascope {

// Progress is measured in decoded AND inferred frames, not frame-read attempts.
// Unknown totals are represented honestly instead of deriving them from FPS.
enum class VideoDetectionStage : std::uint8_t {
    Idle, Queued, Processing, Completed, Canceled, Failed
};

struct VideoDetectionProgress {
    VideoDetectionStage stage = VideoDetectionStage::Idle;
    std::uint64_t generation = 0;
    std::uint64_t processed_frames = 0;
    std::optional<std::uint64_t> reported_total_frames;
    std::size_t cached_frames = 0;
    std::string message = "No video detection job.";
    VideoPerformanceMetrics performance; // decode + inference only; no encoding
};

// A test seam: production constructs a SHA-256-verified CPU NanoDet network;
// tests supply a synthetic, deterministic frame detector without ONNX weights.
using RecordedFrameDetector =
    std::function<RecordedVideoObjectDetection(const VideoFramePacket&)>;
using RecordedDetectorFactory = std::function<RecordedFrameDetector(
    const std::filesystem::path&, const DnnPostprocessOptions&)>;

// A single persistent worker, one replaceable queued job, and a bounded result
// cache. Decoding/inference never runs on the GUI thread. A cancel/new job
// supersedes an in-flight inference when it RETURNS; OpenCV forward() cannot
// be interrupted. Polling a result never waits for the inference operation.
class AsyncRecordedVideoDetection final {
public:
    static constexpr std::size_t kMaxCachedFrames = 256;
    static constexpr std::uint64_t kMaxProcessedFrames = 100'000;

    explicit AsyncRecordedVideoDetection(RecordedDetectorFactory factory = {});
    ~AsyncRecordedVideoDetection();
    AsyncRecordedVideoDetection(const AsyncRecordedVideoDetection&) = delete;
    AsyncRecordedVideoDetection& operator=(const AsyncRecordedVideoDetection&) = delete;

    [[nodiscard]] std::uint64_t start(const std::filesystem::path& video,
        const std::filesystem::path& model, DnnPostprocessOptions options = {},
        VideoBackend backend = VideoBackend::Automatic);
    void cancel() noexcept;
    [[nodiscard]] VideoDetectionProgress snapshot() const;
    // Returns a result only for the SAME frame identity, pixel dimensions and
    // timestamp provenance. Never draws stale results after seek/source changes.
    [[nodiscard]] std::shared_ptr<const RecordedVideoObjectDetection> overlay_for(
        const VideoFramePacket& frame) const;

private:
    struct Job {
        std::uint64_t generation;
        std::filesystem::path video;
        std::filesystem::path model;
        DnnPostprocessOptions options;
        VideoBackend backend;
    };

    void run(std::stop_token token);
    void process(const Job& job, std::stop_token token);
    [[nodiscard]] bool current(std::uint64_t generation, std::stop_token token) const;

    RecordedDetectorFactory factory_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<Job> pending_;
    std::uint64_t generation_ = 0;
    VideoDetectionProgress progress_;
    std::map<std::uint64_t, std::shared_ptr<const RecordedVideoObjectDetection>> results_;
    std::jthread worker_; // destroyed before the mutex and condition variable
};

} // namespace chromascope
