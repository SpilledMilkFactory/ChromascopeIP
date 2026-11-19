#include "chromascope/LiveCameraRecording.hpp"

#include <cmath>
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
        std::cout << "PASS: opt-in recording destination/format/worker policy\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Live recording policy failed: " << error.what() << '\n';
        return 1;
    }
}
