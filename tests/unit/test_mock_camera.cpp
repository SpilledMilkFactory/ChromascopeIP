#include "support/MockCamera.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
using namespace chromascope;
using namespace chromascope::test;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main() {
    try {
        auto control = std::make_shared<MockCameraControl>();
        auto reader = mock_reader(control);
        require(control->snapshot().constructed == 1 && control->snapshot().destroyed == 0,
                "mock reader lifetime not observable");
        auto format = reader->active_format();
        require(format && format->width == 8 && format->height == 8 && format->fps == 30.0,
                "mock active format not scripted");
        control->enqueue({MockCameraAction::Frame, 7, 123ms, 0x42});
        auto packet = reader->read();
        require(packet.index == 7 && packet.image.format() == PixelFormat::BGR8 &&
                packet.image.pixels().size() == 192 && packet.image.pixels()[0] == 0x42 &&
                packet.image.metadata().timestamp &&
                packet.image.metadata().timestamp->value == 123ms &&
                packet.image.metadata().timestamp->domain == TimestampDomain::CaptureMonotonic,
                "mock bytes/index/clock mismatch");
        control->enqueue({MockCameraAction::Frame, 8, 124ms, 0x11});
        auto second = reader->read();
        require(packet.image.pixels()[0] == 0x42 && second.image.pixels()[0] == 0x11,
                "mock packets alias memory");
        control->enqueue({MockCameraAction::TypedReadError, 0, 0ns, 0, 8, 8,
                          CameraErrorCode::DeviceUnavailable});
        bool caught = false;
        try { (void)reader->read(); }
        catch (const CameraError& error) { caught = error.code() == CameraErrorCode::DeviceUnavailable; }
        require(caught, "mock typed fault was not thrown");
        control->enqueue({MockCameraAction::StandardReadError});
        caught = false;
        try { (void)reader->read(); }
        catch (const std::runtime_error&) { caught = true; }
        require(caught, "mock standard exception not thrown");
        control->enqueue({MockCameraAction::UnknownReadError});
        caught = false;
        try { (void)reader->read(); }
        catch (...) { caught = true; }
        require(caught, "mock unknown exception not thrown");
        reader.reset();
        require(control->snapshot().destroyed == 1, "mock reader leaked");
        auto denied = std::make_shared<MockCameraControl>();
        denied->fail_open(CameraErrorCode::PermissionDenied);
        auto bad_reader = mock_reader(denied);
        caught = false;
        try { (void)bad_reader->active_format(); }
        catch (const CameraError& error) { caught = error.code() == CameraErrorCode::PermissionDenied; }
        require(caught, "mock open error lost category");
        bad_reader.reset();
        require(denied->snapshot().destroyed == 1, "denied reader leaked");
        std::cout << "PASS: reusable mock owned pixels, capture clock, fault scripts and lifetime\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M8.6 mock camera unit failure: " << error.what() << '\n';
        return 1;
    }
}
