#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageIO.hpp"
#include "chromascope/ProcessingQueue.hpp"
#include "chromascope/SkittlesProcessor.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace chromascope;
using namespace std::chrono_literals;

int main() {
    try {
        const auto image = read_image(CHROMASCOPE_ASYNC_SAMPLE);
        const auto frame = std::make_shared<const ImageFrame>(
            from_cv_mat(image, PixelFormat::BGR8));
        const auto registry = make_builtin_registry();
        const auto profile = registry.default_profile("skittles");
        const auto expected = registry.run(profile, *frame);
        std::stop_source stopped;
        stopped.request_stop();
        bool rejected_stopped = false;
        try { (void)registry.run(profile, *frame, stopped.get_token()); }
        catch (const std::runtime_error&) { rejected_stopped = true; }
        if (!rejected_stopped) throw std::runtime_error("pre-canceled request ran");
        ProcessingQueue queue([&registry](const ProcessorProfile& p,
                                          const ImageFrame& f, std::stop_token token) {
            return registry.run(p, f, token);
        });
        auto ticket = queue.submit(profile, frame);
        auto deadline = std::chrono::steady_clock::now() + 15s;
        for (;;) {
            if (auto done = queue.poll()) {
                if (done->ticket != ticket || !done->processed || !done->error.empty())
                    throw std::runtime_error("invalid asynchronous result");
                const auto& actual = done->processed->detections;
                const auto& reference = expected.detections;
                if (actual.width() != reference.width() ||
                    actual.height() != reference.height() ||
                    actual.objects().size() != reference.objects().size())
                    throw std::runtime_error("asynchronous regression mismatch");
                for (std::size_t i = 0; i < actual.objects().size(); ++i) {
                    if (actual.objects()[i].label != reference.objects()[i].label ||
                        actual.objects()[i].score != reference.objects()[i].score ||
                        actual.objects()[i].geometry.index() != reference.objects()[i].geometry.index())
                        throw std::runtime_error("asynchronous detection metadata mismatch");
                    const auto* a = std::get_if<CircleGeometry>(&actual.objects()[i].geometry);
                    const auto* b = std::get_if<CircleGeometry>(&reference.objects()[i].geometry);
                    if (!a || !b || a->center.x != b->center.x ||
                        a->center.y != b->center.y || a->radius != b->radius)
                        throw std::runtime_error("asynchronous circle geometry mismatch");
                }
                std::cout << "PASS: asynchronous Skittles detection matches direct path\n";
                return 0;
            }
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("async worker timeout");
            std::this_thread::sleep_for(3ms);
        }
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
