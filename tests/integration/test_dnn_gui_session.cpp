#include "chromascope/DnnGuiSession.hpp"
#include "support/SyntheticReluOnnx.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace chromascope;
using chromascope::testing::check;

DnnGuiSnapshot wait_for(DnnGuiSession& session, DnnGuiStage wanted) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (std::chrono::steady_clock::now()<deadline) {
        auto state=session.snapshot();
        if (state.stage == wanted) return state;
        if (state.stage == DnnGuiStage::Failed)
            throw std::runtime_error("DNN GUI session failed: "+state.message);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("DNN GUI session timed out");
}

void verify(const std::filesystem::path& folder) {
    std::filesystem::create_directories(folder);
    const auto model=folder/"synthetic-relu.onnx";
    { std::ofstream out(model,std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(testing::kSyntheticReluOnnx.data()),
                static_cast<std::streamsize>(testing::kSyntheticReluOnnx.size()));
      check(static_cast<bool>(out),"Synthetic ONNX write failed"); }
    DnnGuiSession invalid;
    invalid.load(model);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (std::chrono::steady_clock::now()<deadline) {
        if (invalid.snapshot().stage == DnnGuiStage::Failed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(invalid.snapshot().stage == DnnGuiStage::Failed,
          "Non-selected ONNX must fail pinned SHA-256 verification");
    check(!invalid.ready_for(model),"Tampered model must not be ready");

    const auto digest=dnn_file_sha256(model);
    DnnGuiSession session(digest); // Synthetic test trust anchor only, never GUI.
    session.load(model);
    const auto loaded=wait_for(session,DnnGuiStage::Ready);
    check(loaded.message.find("verified") != std::string::npos,"Ready state must report digest verification");
    check(session.ready_for(model),"Verified model is ready for its exact path");
    check(!session.ready_for(folder/"other.onnx"),"Different paths do not inherit readiness");
    const auto image=std::make_shared<const ImageFrame>(ImageFrame::from_owned_bytes(
        2,2,PixelFormat::BGR8,6,{10,20,30,40,50,60,70,80,90,100,110,120}));
    DnnPreprocessOptions preprocessing;
    preprocessing.width=2; preprocessing.height=2;
    preprocessing.mean={0.F,0.F,0.F};
    preprocessing.stddev={1.F,1.F,1.F};
    session.run(image,preprocessing);
    const auto result=wait_for(session,DnnGuiStage::Completed);
    check(result.output_shapes.size()==1 &&
          result.output_shapes[0]==std::vector<int>({1,3,2,2}),
          "Raw CPU forward should publish the tensor shape, not objects");
    check(result.elapsed_ms>=0.0,"Raw CPU inference duration");
    check(session.ready_for(model),"Loaded model remains ready after a complete forward");
    session.load(model);
    session.cancel();
    check(session.snapshot().stage==DnnGuiStage::Cancelled,
          "Cancellation invalidates the latest generation");
    check(!session.ready_for(model),"Cancellation removes readiness");
    session.load(model);
    (void)wait_for(session,DnnGuiStage::Ready);
    session.unload();
    check(!session.ready_for(model),"Unload immediately invalidates verification");
    check(session.snapshot().stage==DnnGuiStage::Unloaded,"Unloaded visible");
}
} // namespace

int main(int argc,char** argv) {
    return chromascope::testing::run("M5.5 asynchronous DNN GUI session", [&] {
        check(argc==2,"Expected a build-tree artifact directory");
        verify(std::filesystem::path(argv[1]));
    });
}
