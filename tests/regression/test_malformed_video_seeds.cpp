#include "chromascope/VideoSource.hpp"
#include "support/MalformedSeedCorpus.hpp"
#include "support/TestSupport.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
using chromascope::testing::check;

void test(const fs::path& directory) {
    fs::create_directories(directory);
    const auto seeds = chromascope::testing::read_malformed_seeds(CHROMASCOPE_M106_SEED_MANIFEST);
    std::size_t tested = 0;
    for (const auto& seed : seeds) {
        if (seed.outcome != "video") continue;
        const auto path = directory / (seed.name + ".avi");
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            check(static_cast<bool>(file), "create synthetic malformed recording");
            if (!seed.bytes.empty()) {
                file.write(reinterpret_cast<const char*>(seed.bytes.data()),
                           static_cast<std::streamsize>(seed.bytes.size()));
            }
            check(static_cast<bool>(file), "write synthetic malformed recording");
        }
        bool refused = false;
        try {
            // The existing deterministic MJPEG test backend is selected
            // explicitly. This does not fuzz all installed video decoders.
            (void)chromascope::VideoSource::open(path,
                                                  chromascope::VideoBackend::OpenCVMJPEG);
        } catch (const std::exception&) { refused = true; }
        check(refused, "incomplete video accepted: " + seed.name);
        std::error_code error;
        fs::remove(path, error);
        check(!error, "remove synthetic video after test");
        ++tested;
    }
    check(tested == 5, "expected five malicious/truncated video seeds");
}
} // namespace
int main(int argc, char** argv) {
    return chromascope::testing::run("regression.malformed_video_seeds", [&] {
        if (argc != 2) throw std::invalid_argument("expected build-tree artifact directory");
        test(argv[1]);
    });
}
