#include "chromascope/InstallResources.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    const auto require = [](bool value, const char* reason) {
        if (!value) throw std::runtime_error(reason);
    };
    require(chromascope::installed_data_directory({}).empty(), "empty executable");
    const auto root = fs::current_path() / "test-artifacts" / "m111-resource-fixture";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "stage" / "bin");
    fs::create_directories(root / "checkout" / "samples" / "skittles");
    const auto exe = root / "stage" / "bin" / "chromascope-gui";
    const auto data = chromascope::installed_data_directory(exe);
    require(data.is_absolute(), "installed data path is absolute when executable is");
    require(data.string().find("stage") != std::string::npos, "layout-relative data path");
    require(!chromascope::find_legacy_sample_directory(exe, root), "missing samples not found");
    require(!chromascope::find_installed_nanodet_model(exe), "missing packaged model");
    const auto model_dir = data / "models";
    fs::create_directories(model_dir);
    const auto model = model_dir / "object_detection_nanodet_2022nov.onnx";
    std::ofstream(model) << "model fixture";
    require(chromascope::find_installed_nanodet_model(exe) == model,
            "installed NanoDet path discovered from executable");
    fs::remove(model);
    fs::create_symlink(root / "missing.onnx", model);
    require(!chromascope::find_installed_nanodet_model(exe),
            "refuse symlinked bundled weights");
    fs::remove(model);

    auto fixture = root / "checkout" / "samples" / "skittles";
    for (unsigned n = 1; n <= 4; ++n)
        std::ofstream(fixture / ("s" + std::to_string(n) + ".jpg")) << "mock";
    require(chromascope::find_legacy_sample_directory(exe, root / "checkout") == fixture,
            "developer checkout sample discovery");
    const auto installed = data / "samples" / "skittles";
    fs::create_directories(installed);
    for (unsigned n = 1; n <= 4; ++n)
        std::ofstream(installed / ("s" + std::to_string(n) + ".jpg")) << "mock";
    require(chromascope::find_legacy_sample_directory(exe, root / "checkout") == installed,
            "installed resource takes precedence");
    fs::remove(installed / "s4.jpg");
    fs::remove(fixture / "s4.jpg");
    require(!chromascope::find_legacy_sample_directory(exe, root / "checkout"),
            "incomplete samples are unavailable");
    fs::remove_all(root, error);
    return 0;
}
