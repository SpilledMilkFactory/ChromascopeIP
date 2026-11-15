#include "chromascope/GuiPreferenceStorage.hpp"
#include "chromascope/GuiPreferences.hpp"
#include "support/TestSupport.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
using chromascope::testing::check;
void exercise() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("chromascope-m102-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file = root / "preferences-v1.conf";
    try {
        check(!chromascope::read_gui_preferences_file(file), "missing file not optional");
        const auto baseline = chromascope::serialize_gui_preferences(
            chromascope::default_gui_preferences());
        chromascope::write_gui_preferences_file(file, baseline);
        check(chromascope::read_gui_preferences_file(file) == baseline, "saved file mismatch");
        auto updated = chromascope::default_gui_preferences();
        updated.ui["preview"] = 2;
        const auto output = chromascope::serialize_gui_preferences(updated);
        chromascope::write_gui_preferences_file(file, output);
        check(chromascope::read_gui_preferences_file(file) == output, "replace failed");
        check(!fs::exists(fs::path(file.string() + ".tmp")), "temporary output leaked");
        bool rejected = false;
        try { chromascope::write_gui_preferences_file(file, std::string(17000, 'X')); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "oversized write allowed");
        check(chromascope::read_gui_preferences_file(file) == output, "failed write changed file");
#ifndef _WIN32
        auto link = root / "link";
        fs::create_directory_symlink(root, link);
        rejected = false;
        try { (void)chromascope::read_gui_preferences_file(link / "preferences-v1.conf"); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, "symlink parent accepted");
        struct stat info{};
        check(::stat(file.c_str(), &info) == 0 && (info.st_mode & 0077) == 0,
              "preferences file must be owner-only on POSIX");
#endif
    } catch (...) { fs::remove_all(root); throw; }
    fs::remove_all(root);
}
}
int main() { return chromascope::testing::run("M10.2 storage roundtrip", exercise); }
