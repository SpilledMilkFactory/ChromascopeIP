#include "CameraCliDetectionContract.hpp"
#include "support/TestSupport.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace {
using chromascope::testing::check;
using chromascope::cli::parse_camera_detection_cli;

auto parse(std::vector<std::string> args) {
    std::vector<char*> values;
    for (auto& text : args) values.push_back(text.data());
    return parse_camera_detection_cli(static_cast<int>(values.size()), values.data());
}
template<class F> void rejects(F&& call) {
    bool failed = false;
    try { call(); } catch (const std::invalid_argument&) { failed = true; }
    check(failed, "unsafe camera CLI accepted");
}
void exercise() {
    const auto base = std::vector<std::string>{"chromascope-cli", "--camera-detect", "--camera", "2",
        "--camera-backend", "v4l2", "--profile", "person", "--model", "test.onnx", "--jsonl", "-"};
    const auto parsed = parse(base);
    check(parsed.camera.device_index == 2 && parsed.camera.max_frames == 30 &&
        parsed.camera.max_seconds == 5 && parsed.postprocess.person_only && parsed.jsonl.empty(),
        "camera detection CLI defaults changed");
    check(parsed.camera.backend == chromascope::CameraBackend::V4L2, "backend not explicit");
    auto capped = base;
    capped.insert(capped.end(), {"--camera-max-frames", "3", "--camera-max-seconds", "4",
        "--display-threshold", "0.70", "--class-threshold", "person=0.75"});
    const auto custom = parse(capped);
    check(custom.camera.max_frames == 3 && custom.camera.max_seconds == 4 &&
        custom.postprocess.class_thresholds.size() == 1 &&
        custom.postprocess.person_display_threshold == 0.70, "custom profile not parsed");
    auto absent = base;
    absent.erase(absent.begin() + 2, absent.begin() + 4);
    rejects([&]{ (void)parse(absent); });
    auto invalid = base;
    invalid.insert(invalid.end(), {"--input", "foo.mp4"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--camera-save-dir", "out"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--camera-max-frames", "301"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--camera-max-seconds", "0"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--jsonl", "elsewhere.jsonl"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--confidence", "NaN"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--class-threshold", "cat=0.3"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--help"});
    rejects([&]{ (void)parse(invalid); });
    invalid = base;
    invalid.insert(invalid.end(), {"--camera", "1"});
    rejects([&]{ (void)parse(invalid); });
}
} // namespace
int main() { return chromascope::testing::run("M9.6 live CLI contract", exercise); }
