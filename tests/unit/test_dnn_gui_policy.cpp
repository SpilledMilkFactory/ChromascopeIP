#include "chromascope/DnnGuiSession.hpp"
#include "support/TestSupport.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void verify(const std::filesystem::path& folder) {
    using chromascope::testing::check;
    using namespace chromascope;
    check(kSelectedNanoDetSha256.size() == 64, "Pinned NanoDet digest length");
    check(kSelectedNanoDetFilename == "object_detection_nanodet_2022nov.onnx",
          "Pinned NanoDet filename");
    validate_dnn_gui_options({});
    validate_dnn_gui_options({0.0, 0.0, 1});
    validate_dnn_gui_options({1.0, 1.0, 512});
    const auto reject = [&](DnnGuiOptions options) {
        try { validate_dnn_gui_options(options); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("Invalid GUI model setting accepted");
    };
    reject({-0.1, 0.6, 100}); reject({1.1, 0.6, 100});
    reject({0.35, -0.1, 100}); reject({0.35, 1.1, 100});
    reject({0.35, 0.6, 0}); reject({0.35, 0.6, 513});
    reject({std::numeric_limits<double>::quiet_NaN(), 0.6, 100});
    reject({0.35, std::numeric_limits<double>::infinity(), 100});
    std::filesystem::create_directories(folder);
    const auto abc=folder/"abc.bin";
    { std::ofstream out(abc, std::ios::binary | std::ios::trunc); out << "abc"; }
    check(dnn_file_sha256(abc) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256 known ABC vector");
    const auto longfile=folder/"long.bin";
    { std::ofstream out(longfile, std::ios::binary | std::ios::trunc);
      out << std::string(1000000, 'a'); }
    check(dnn_file_sha256(longfile) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "SHA-256 million-a vector");
    const auto missing=folder/"absent.onnx";
    try { (void)dnn_file_sha256(missing); throw std::runtime_error("Missing model accepted"); }
    catch (const std::invalid_argument&) {}
    const auto empty=folder/"empty.onnx";
    { std::ofstream out(empty, std::ios::binary | std::ios::trunc); }
    try { (void)dnn_file_sha256(empty); throw std::runtime_error("Empty model accepted"); }
    catch (const std::invalid_argument&) {}
    try { DnnGuiSession bad("INVALID"); throw std::runtime_error("Invalid digest accepted"); }
    catch (const std::invalid_argument&) {}
    DnnGuiSession session;
    check(!session.ready_for(abc), "New session cannot be ready");
    session.cancel();
    check(session.snapshot().stage == DnnGuiStage::Cancelled, "Cancellation immediately visible");
    session.unload();
    check(session.snapshot().stage == DnnGuiStage::Unloaded, "Unload immediately visible");
    std::error_code ec;
    std::filesystem::remove_all(folder,ec);
}
} // namespace
int main(int argc, char** argv) {
    return chromascope::testing::run("M5.5 DNN GUI policy and hash", [&] {
        chromascope::testing::check(argc == 2, "Expected a build-tree test artifact directory");
        verify(std::filesystem::path(argv[1]));
    });
}
