#include "chromascope/DnnGuiSession.hpp"
#include "support/SyntheticReluOnnx.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace chromascope;
using chromascope::testing::check;

DnnGuiSnapshot await(DnnGuiSession& session, DnnGuiStage wanted) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = session.snapshot();
        if (result.stage == wanted) return result;
        if (result.stage == DnnGuiStage::Failed && wanted != DnnGuiStage::Failed)
            throw std::runtime_error("Unexpected GUI DNN failure: " + result.message);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("Timed out waiting for GUI DNN stage");
}
void verify(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    const auto model = directory / "synthetic-relu.onnx";
    {
        std::ofstream output(model, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(testing::kSyntheticReluOnnx.data()),
                     static_cast<std::streamsize>(testing::kSyntheticReluOnnx.size()));
        check(static_cast<bool>(output), "Synthetic model fixture write failed");
    }
    auto image = std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        2, 2, PixelFormat::BGR8, 6, {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120}));
    DnnGuiSession untrusted;
    untrusted.load(model);
    const auto rejected = await(untrusted, DnnGuiStage::Failed);
    check(rejected.person_detections == nullptr && !untrusted.ready_for(model),
          "Untrusted model cannot provide person boxes");

    // Test-only anchor: never used in the actual GUI. The tiny Relu model
    // intentionally has no NanoDet heads and must FAIL instead of inventing boxes.
    DnnGuiSession session(dnn_file_sha256(model));
    DnnPostprocessOptions policy;
    policy.person_only = true;
    try {
        session.run_person(nullptr, policy);
        throw std::runtime_error("Null image accepted");
    } catch (const std::invalid_argument&) {}
    session.load(model);
    (void)await(session, DnnGuiStage::Ready);
    policy.person_display_threshold = std::numeric_limits<double>::quiet_NaN();
    try {
        session.run_person(image, policy);
        throw std::runtime_error("Non-finite person display threshold accepted");
    } catch (const std::invalid_argument&) {}
    check(session.ready_for(model) && session.snapshot().stage == DnnGuiStage::Ready,
          "Invalid settings must be rejected without queuing model inference");
    policy.person_display_threshold = .75;
    policy.person_only = false;
    try {
        session.run_person(image, policy);
        throw std::runtime_error("General profile accepted as person-only");
    } catch (const std::invalid_argument&) {}
    policy.person_only = true;
    session.run_person(image, policy);
    check(session.snapshot().stage == DnnGuiStage::Running,
          "Person request should run on bounded DNN worker");
    const auto failed = await(session, DnnGuiStage::Failed);
    check(!failed.person_detections && !session.ready_for(model),
          "Unsupported DNN outputs must not publish fabricated person boxes");
    session.load(model);
    (void)await(session, DnnGuiStage::Ready);
    session.run_person(image, policy);
    session.clear_results();
    const auto cleared = session.snapshot();
    check(cleared.stage == DnnGuiStage::Ready && !cleared.person_detections &&
          session.ready_for(model), "Changing a setting must clear stale boxes without unloading model");
    session.run_person(image, policy);
    session.cancel();
    check(session.snapshot().stage == DnnGuiStage::Cancelled &&
          !session.snapshot().person_detections && !session.ready_for(model),
          "Cancelled detection must invalidate previous boxes and model state");
    session.unload();
    check(session.snapshot().stage == DnnGuiStage::Unloaded &&
          !session.snapshot().person_detections,
          "Unloaded model must not leave visible person boxes");
    std::error_code ec;
    std::filesystem::remove(model, ec);
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M7.3 still person GUI session and lifecycle", [&] {
        check(argc == 2, "Expected test build-tree output directory");
        verify(std::filesystem::path(argv[1]));
    });
}
