#include "chromascope/VideoSource.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

int main() {
    static_assert(!std::is_copy_constructible_v<chromascope::VideoSource>);
    static_assert(!std::is_copy_assignable_v<chromascope::VideoSource>);
    static_assert(std::is_nothrow_move_constructible_v<chromascope::VideoSource>);
    static_assert(std::is_nothrow_move_assignable_v<chromascope::VideoSource>);
    try {
        chromascope::FrameMetadata metadata;
        metadata.timestamp = chromascope::FrameTimestamp{
            std::chrono::milliseconds(100), chromascope::TimestampDomain::MediaPresentation};
        std::vector<std::uint8_t> bytes(3 * 2 * 2, 27);
        chromascope::VideoFramePacket packet{
            42, chromascope::ImageFrame::from_owned_bytes(
                2, 2, chromascope::PixelFormat::BGR8, 6, std::move(bytes), metadata)};
        chromascope::VideoReadResult result{
            chromascope::VideoReadStatus::Frame, std::move(packet)};
        if (!result.frame || result.frame->index != 42 ||
            result.frame->image.metadata().timestamp->value != std::chrono::milliseconds(100))
            throw std::runtime_error("packet lost index or presentation timestamp");
        if (result.frame->image.pixels().size() != 12)
            throw std::runtime_error("packet lost owned pixel buffer");
        chromascope::VideoReadResult end{
            chromascope::VideoReadStatus::EndOfStream, std::nullopt};
        if (end.frame) throw std::runtime_error("EOF incorrectly contains pixels");
        std::cout << "PASS: VideoSource ownership, metadata and status contract\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M4.2 VideoSource contract failed: " << exception.what() << '\n';
        return 1;
    }
}
