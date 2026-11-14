#include "chromascope/CameraSource.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Error, class Func>
void reject(Func&& function, const char* message) {
    try { function(); } catch (const Error&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    using namespace chromascope;
    try {
        validate_camera_format({});
        validate_camera_format({1280, 720, 30.0, CameraTransport::MJPEG});
        require(camera_format_matches({}, {640, 480, 0.0, CameraTransport::Auto}),
                "driver defaults rejected");
        const CameraFormatRequest requested{1280, 720, 30.0, CameraTransport::MJPEG};
        require(camera_format_matches(requested, {1280, 720, 29.97, CameraTransport::MJPEG}),
                "nominal 29.97 fps rejected");
        require(!camera_format_matches(requested, {640, 480, 30, CameraTransport::MJPEG}),
                "wrong resolution accepted");
        require(!camera_format_matches(requested, {1280, 720, 25, CameraTransport::MJPEG}),
                "wrong fps accepted");
        require(!camera_format_matches(requested, {1280, 720, 30, CameraTransport::YUYV}),
                "wrong transport accepted");
        require(!camera_format_matches({}, {0, 720, 30, CameraTransport::Auto}),
                "invalid actual geometry accepted");
        require(!camera_format_matches({}, {9000, 9000, 30, CameraTransport::Auto}),
                "budget breach accepted");
        reject<std::invalid_argument>([] { validate_camera_format({640, 0, 0, CameraTransport::Auto}); },
                                      "partial dimensions accepted");
        reject<std::invalid_argument>([] { validate_camera_format({9000, 1, 0, CameraTransport::Auto}); },
                                      "oversize accepted");
        reject<std::invalid_argument>([] { validate_camera_format({640, 480, -1, CameraTransport::Auto}); },
                                      "negative fps accepted");
        reject<std::invalid_argument>([] { validate_camera_format({640, 480, 0.5, CameraTransport::Auto}); },
                                      "fractional low fps accepted");
        reject<std::invalid_argument>([] { validate_camera_format({640, 480, 241, CameraTransport::Auto}); },
                                      "high fps accepted");
        reject<std::invalid_argument>([] {
            validate_camera_format({640, 480, std::numeric_limits<double>::quiet_NaN(),
                                    CameraTransport::Auto});
        }, "NaN fps accepted");
        reject<std::invalid_argument>([] {
            validate_camera_format({0, 0, 0, static_cast<CameraTransport>(255)});
        }, "unknown encoding accepted");
        require(camera_error_name(CameraErrorCode::PermissionDenied) == "permission_denied",
                "permission error lost");
        require(camera_error_name(CameraErrorCode::ReadFailed) == "read_failed",
                "read error lost");
        const CameraError error(CameraErrorCode::FormatRejected, "synthetic");
        require(error.code() == CameraErrorCode::FormatRejected, "structured error lost");
        std::cout << "PASS: camera format selection, strict negotiation, error categories\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "M8.2 format policy failed: " << error.what() << '\n';
        return 1;
    }
}
