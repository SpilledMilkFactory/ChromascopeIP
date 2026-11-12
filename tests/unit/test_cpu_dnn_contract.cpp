#include "chromascope/CpuDnnAdapter.hpp"

#include <string>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
template <typename Exception, typename Fn>
void rejected(Fn&& fn) {
    bool failed = false;
    try { fn(); } catch (const Exception&) { failed = true; }
    if (!failed) throw std::runtime_error("Expected DNN contract rejection");
}
} // namespace

int main() {
    using namespace chromascope;
    if (std::string(CpuDnnAdapter::backend()) != "opencv-cpu")
        throw std::runtime_error("Invalid CPU DNN backend");
    DnnTensor valid{{1, 3, 2, 2}, std::vector<float>(12, 0.5F)};
    validate_dnn_input(valid);
    auto missing = valid;
    missing.values.pop_back();
    rejected<std::invalid_argument>([&] { validate_dnn_input(missing); });
    auto negative = valid;
    negative.shape[2] = -1;
    rejected<std::invalid_argument>([&] { validate_dnn_input(negative); });
    auto bad_rank = valid;
    bad_rank.shape = {1, 12};
    rejected<std::invalid_argument>([&] { validate_dnn_input(bad_rank); });
    auto bad_batch = valid;
    bad_batch.shape[0] = 2;
    rejected<std::invalid_argument>([&] { validate_dnn_input(bad_batch); });
    auto nonfinite = valid;
    nonfinite.values[0] = std::numeric_limits<float>::infinity();
    rejected<std::invalid_argument>([&] { validate_dnn_input(nonfinite); });
    auto huge = valid;
    huge.shape[2] = std::numeric_limits<int>::max();
    rejected<std::length_error>([&] { validate_dnn_input(huge); });
}
