#include "CliOutputPaths.hpp"
#include "support/TestSupport.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    return chromascope::testing::run("unit.cli_output_paths", [] {
        using chromascope::cli::OutputPaths;
        using chromascope::cli::validate_output_paths;
        using chromascope::testing::check;

        const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const fs::path root = fs::temp_directory_path() / ("chromascope-cli-paths-" + suffix);
        fs::create_directories(root);
        struct Cleanup {
            fs::path directory;
            ~Cleanup() { std::error_code error; fs::remove_all(directory, error); }
        } cleanup{root};
        const auto input = root / "source.jpg";
        { std::ofstream file(input, std::ios::binary); file << "unchanged"; }
        const auto reject = [&](const OutputPaths& paths) {
            bool thrown = false;
            try { (void)validate_output_paths(input, paths); }
            catch (const std::invalid_argument&) { thrown = true; }
            check(thrown, "unsafe output location must be rejected");
        };
        reject({input, {}, {}, {}});
        reject({root / "./source.jpg", {}, {}, {}});
        reject({root / "report.json", {}, root / "report.json", {}});
        reject({root, {}, {}, {}});
        reject({{}, {}, {}, root / "unused"});
        reject({{}, {}, {}, input});

        const auto outdir = root / "generated";
        const auto resolved = validate_output_paths(input,
            {"preview.png", "binary.png", "results.json", outdir});
        check(resolved.image == outdir / "preview.png", "resolve image under --output-dir");
        check(resolved.mask == outdir / "binary.png", "resolve mask under --output-dir");
        check(resolved.json == outdir / "results.json", "resolve JSON under --output-dir");
        check(!fs::exists(outdir), "path validation must not create directories");
        chromascope::cli::prepare_output_directory(resolved);
        check(fs::is_directory(outdir), "explicit output directory should be created");
        check(validate_output_paths(input, {root / "absolute.png", {}, {}, outdir}).image ==
              root / "absolute.png", "absolute output must ignore directory prefix");
        reject({"../source.jpg", {}, {}, outdir});
        reject({"same.png", {}, "same.png", outdir});

        // Detect hard links even though their names/canonical paths differ.
        const auto linked = root / "linked.jpg";
        fs::create_hard_link(input, linked);
        reject({linked, {}, {}, {}});
        check(fs::file_size(input) == 9, "input contents not touched");
    });
}
