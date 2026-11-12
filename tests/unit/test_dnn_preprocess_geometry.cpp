#include "chromascope/DnnPreprocessing.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<typename Error, typename F>
void rejects(F&& f) {
    try { f(); } catch (const Error&) { return; }
    throw std::runtime_error("Expected DNN geometry validation failure");
}
void close(double a, double b) {
    require(std::abs(a - b) < 1e-10, "DNN inverse geometry mismatch");
}
} // namespace

int main() {
    using namespace chromascope;
    DnnPreprocessOptions options;
    auto g = make_dnn_preprocess_geometry(640, 320, options);
    require(g.resized_width == 416 && g.resized_height == 208 &&
            g.pad_left == 0 && g.pad_top == 104 && g.pad_bottom == 104,
            "wide letterbox geometry failed");
    auto p = g.source_to_model(320., 160.);
    close(p.first, 208.); close(p.second, 208.);
    auto q = g.model_to_source(p.first, p.second);
    close(q.first, 320.); close(q.second, 160.);

    auto tall = make_dnn_preprocess_geometry(320, 640, options);
    require(tall.pad_left == 104 && tall.pad_right == 104 &&
            tall.pad_top == 0 && tall.resized_width == 208, "tall letterbox geometry failed");
    auto tiny = make_dnn_preprocess_geometry(1, 1000000, options);
    require(tiny.resized_width == 1 && tiny.resized_height == 416 &&
            tiny.pad_left == 207 && tiny.pad_right == 208, "extreme aspect ratio not bounded");
    auto odd = make_dnn_preprocess_geometry(4, 2, DnnPreprocessOptions{.width=5, .height=4});
    require(odd.resized_width == 5 && odd.resized_height == 2 &&
            odd.pad_top == 1 && odd.pad_bottom == 1, "odd letterbox dimensions failed");
    options.resize = DnnResizeMode::Stretch;
    g = make_dnn_preprocess_geometry(640, 320, options);
    require(g.resized_height == 416 && g.pad_top == 0, "stretch geometry failed");
    options.width = 0;
    rejects<std::invalid_argument>([&] { (void)make_dnn_preprocess_geometry(4, 4, options); });
    options.width = 4096; options.height = 4096;
    rejects<std::length_error>([&] { (void)make_dnn_preprocess_geometry(4, 4, options); });
    options.width = 416; options.height = 416;
    options.stddev[0] = 0.0F;
    rejects<std::invalid_argument>([&] { (void)make_dnn_preprocess_geometry(4, 4, options); });
    options.stddev[0] = 1.0F;
    options.mean[0] = std::numeric_limits<float>::quiet_NaN();
    rejects<std::invalid_argument>([&] { (void)make_dnn_preprocess_geometry(4, 4, options); });
    options.mean[0] = 0.0F;
    rejects<std::invalid_argument>([&] { (void)make_dnn_preprocess_geometry(0, 4, options); });
    options.channels = static_cast<DnnChannelOrder>(255);
    rejects<std::invalid_argument>([&] { (void)make_dnn_preprocess_geometry(4, 4, options); });
}
