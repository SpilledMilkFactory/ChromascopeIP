#include "chromascope/CameraSource.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}
template<typename Exception, typename Function>
void expect_throws(Function&& call, const char* description) {
    try { call(); } catch (const Exception&) { return; }
    throw std::runtime_error(description);
}
} // namespace

int main() {
    try {
        using namespace chromascope;
        static_assert(!std::is_copy_constructible_v<CameraSource>);
        static_assert(!std::is_copy_assignable_v<CameraSource>);
        static_assert(std::is_nothrow_move_constructible_v<CameraSource>);
        static_assert(std::is_nothrow_move_assignable_v<CameraSource>);
        require(camera_backend_name(CameraBackend::V4L2) == "v4l2", "V4L2 name");
        require(camera_backend_name(CameraBackend::MediaFoundation) == "msmf", "MSMF name");
        require(camera_backend_name(CameraBackend::DirectShow) == "dshow", "DSHOW name");
        expect_throws<std::invalid_argument>([] {
            static_cast<void>(camera_backend_name(static_cast<CameraBackend>(255)));
        }, "unknown enum accepted");
        const auto infos = camera_backend_info();
        require(infos.size() == 3, "exact known backends missing");
        require(std::count_if(infos.begin(), infos.end(), [](const auto& info) {
            return info.preferred;
        }) <= 1, "more than one primary backend");
        for (const auto& info : infos) {
            require(!info.registered_for_camera || info.supported_on_host,
                    "unsupported backend advertised as registered");
        }
#if defined(__linux__)
        require(preferred_camera_backend() == CameraBackend::V4L2, "Linux must select V4L2");
#elif defined(_WIN32)
        require(preferred_camera_backend() == CameraBackend::MediaFoundation,
                "Windows must select MSMF");
#endif
        // All negative tests must fail before touching any physical device.
        expect_throws<std::invalid_argument>([] { static_cast<void>(CameraSource::open(-1)); },
                                              "negative device index accepted");
        expect_throws<std::invalid_argument>([] { static_cast<void>(CameraSource::open(256)); },
                                              "out-of-range camera index accepted");
        expect_throws<std::invalid_argument>([] {
            static_cast<void>(CameraSource::open(0, static_cast<CameraBackend>(255)));
        }, "unknown camera backend accepted");
#if defined(__linux__)
        expect_throws<std::invalid_argument>([] {
            static_cast<void>(CameraSource::open(0, CameraBackend::MediaFoundation));
        }, "Windows camera backend opened on Linux");
#elif defined(_WIN32)
        expect_throws<std::invalid_argument>([] {
            static_cast<void>(CameraSource::open(0, CameraBackend::V4L2));
        }, "Linux camera backend opened on Windows");
#endif
        std::cout << "PASS: camera backend selection and camera-free negative paths\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M8.1 camera backend policy failed: " << exception.what() << '\n';
        return 1;
    }
}
