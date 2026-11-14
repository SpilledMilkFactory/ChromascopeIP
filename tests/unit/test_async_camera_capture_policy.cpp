#include "chromascope/AsyncCameraCapture.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<typename Exception, typename F>
void reject(F&& fn, const char* message) {
    try { fn(); } catch (const Exception&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    try {
        using namespace chromascope;
        reject<std::invalid_argument>([] { AsyncCameraCapture{{0, 10}}; }, "zero frame bound");
        reject<std::invalid_argument>([] { AsyncCameraCapture{{17, 10}}; }, "frame bound too large");
        reject<std::invalid_argument>([] { AsyncCameraCapture{{1, 0}}; }, "zero byte bound");
        reject<std::invalid_argument>([] {
            AsyncCameraCapture{{1, 256U * 1024U * 1024U + 1U}};
        }, "byte bound too large");
        AsyncCameraCapture session({2, 384});
        require(session.state() == CameraCaptureState::Idle, "constructor activated camera");
        require(session.generation() == 0 && !session.poll(), "constructor published event");
        reject<std::invalid_argument>([&] { session.start(-1); }, "negative camera index");
        reject<std::invalid_argument>([&] { session.start(256); }, "oversized camera index");
        reject<std::invalid_argument>([&] {
            session.start(0, CameraBackend::PlatformDefault, {640, 0});
        }, "mismatched width height");
        reject<std::invalid_argument>([&] {
            session.start(0, static_cast<CameraBackend>(254));
        }, "bad backend");
        reject<std::invalid_argument>([&] {
            session.start_with_reader(nullptr);
        }, "null synthetic reader");
        require(session.generation() == 0, "rejected request changed session generation");
        require(session.queued_pixel_bytes() == 0 && session.queued_frames() == 0,
                "idle queue is not empty");
        std::cout << "PASS: camera bounded queue limits, no constructor opens, input rejections\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M8.3 capture policy: " << error.what() << '\n';
        return 1;
    }
}
