#include "chromascope/ImageHeader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace chromascope {
namespace {
using Bytes = std::span<const std::uint8_t>;

void require(Bytes b, std::size_t at, std::size_t length) {
    if (at > b.size() || length > b.size() - at)
        throw std::invalid_argument("Truncated or invalid image header");
}

std::uint64_t number(Bytes b, std::size_t at, int count, bool little) {
    require(b, at, static_cast<std::size_t>(count));
    std::uint64_t result = 0;
    for (int i = 0; i < count; ++i) {
        const auto offset = little ? i : count - 1 - i;
        result |= static_cast<std::uint64_t>(b[at + static_cast<std::size_t>(offset)])
                  << (i * 8);
    }
    return result;
}

bool tag(Bytes b, std::size_t at, const char* text, std::size_t length) {
    if (at > b.size() || length > b.size() - at) return false;
    for (std::size_t i = 0; i < length; ++i)
        if (b[at + i] != static_cast<std::uint8_t>(text[i])) return false;
    return true;
}

ImageHeader jpeg(Bytes b) {
    std::size_t at = 2;
    while (at < b.size()) {
        require(b, at, 2);
        if (b[at++] != 0xff)
            throw std::invalid_argument("Invalid JPEG marker sequence");
        while (at < b.size() && b[at] == 0xff) ++at;
        require(b, at, 1);
        const auto marker = b[at++];
        if (marker == 0xd9 || marker == 0xda)
            break; // The image dimensions must appear before the scan.
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8)) continue;
        const auto length = static_cast<std::size_t>(number(b, at, 2, false));
        if (length < 2) throw std::invalid_argument("Invalid JPEG segment length");
        require(b, at, length);
        const bool is_sof = (marker >= 0xc0 && marker <= 0xcf &&
                             marker != 0xc4 && marker != 0xc8 && marker != 0xcc);
        if (is_sof) {
            if (length < 8) throw std::invalid_argument("Invalid JPEG frame header");
            return {static_cast<std::int64_t>(number(b, at + 5, 2, false)),
                    static_cast<std::int64_t>(number(b, at + 3, 2, false))};
        }
        at += length;
    }
    throw std::invalid_argument("JPEG dimensions not found before scan");
}

ImageHeader png(Bytes b) {
    require(b, 0, 24);
    if (number(b, 8, 4, false) != 13 || !tag(b, 12, "IHDR", 4))
        throw std::invalid_argument("Invalid PNG IHDR");
    return {static_cast<std::int64_t>(number(b, 16, 4, false)),
            static_cast<std::int64_t>(number(b, 20, 4, false))};
}

ImageHeader bmp(Bytes b) {
    require(b, 0, 26);
    const auto dib = number(b, 14, 4, true);
    if (dib == 12) return {static_cast<std::int64_t>(number(b, 18, 2, true)),
                           static_cast<std::int64_t>(number(b, 20, 2, true))};
    if (dib < 40) throw std::invalid_argument("Unsupported BMP DIB header");
    require(b, 0, 26);
    // Interpret BMP's signed 32-bit fields without implementation-defined
    // unsigned-to-signed overflow conversions on the target compiler.
    const auto raw_width = number(b, 18, 4, true);
    const auto raw_height = number(b, 22, 4, true);
    const auto signed32 = [](std::uint64_t raw) -> std::int64_t {
        return raw > 0x7fffffffULL ? static_cast<std::int64_t>(raw) - 0x100000000LL
                                    : static_cast<std::int64_t>(raw);
    };
    const auto height = signed32(raw_height);
    return {signed32(raw_width), height < 0 ? -height : height};
}

ImageHeader tiff(Bytes b) {
    require(b, 0, 8);
    const bool little = tag(b, 0, "II", 2);
    if (!little && !tag(b, 0, "MM", 2))
        throw std::invalid_argument("Invalid TIFF byte order");
    if (number(b, 2, 2, little) != 42)
        throw std::invalid_argument("Unsupported TIFF version (classic TIFF required)");
    const auto offset = number(b, 4, 4, little);
    if (offset > b.size()) throw std::invalid_argument("Invalid TIFF directory offset");
    const auto at = static_cast<std::size_t>(offset);
    const auto count = number(b, at, 2, little);
    if (count > 4096) throw std::invalid_argument("Excessive TIFF directory entries");
    require(b, at + 2, static_cast<std::size_t>(count) * 12);
    ImageHeader result;
    for (std::size_t i = 0; i < count; ++i) {
        const auto p = at + 2 + i * 12;
        const auto key = number(b, p, 2, little);
        if (key != 256 && key != 257) continue;
        const auto type = number(b, p + 2, 2, little);
        const auto values = number(b, p + 4, 4, little);
        if (values != 1 || (type != 3 && type != 4))
            throw std::invalid_argument("Unsupported TIFF image dimension tag");
        const auto value = static_cast<std::int64_t>(
            number(b, p + 8, type == 3 ? 2 : 4, little));
        if (key == 256) result.width = value;
        else result.height = value;
    }
    return result;
}

ImageHeader webp(Bytes b) {
    require(b, 0, 20);
    const auto declared = number(b, 4, 4, true);
    if (declared < 12 || declared > b.size() - 8)
        throw std::invalid_argument("Invalid WebP RIFF length");
    const auto end = static_cast<std::size_t>(declared) + 8;
    std::size_t at = 12;
    while (at <= end && end - at >= 8) {
        const auto length = number(b, at + 4, 4, true);
        if (length > end - at - 8)
            throw std::invalid_argument("Invalid WebP chunk length");
        const auto p = at + 8;
        if (tag(b, at, "VP8X", 4)) {
            if (length < 10) throw std::invalid_argument("Invalid WebP VP8X header");
            return {static_cast<std::int64_t>(1 + number(b, p + 4, 3, true)),
                    static_cast<std::int64_t>(1 + number(b, p + 7, 3, true))};
        }
        if (tag(b, at, "VP8L", 4)) {
            if (length < 5 || b[p] != 0x2f)
                throw std::invalid_argument("Invalid WebP VP8L header");
            return {static_cast<std::int64_t>(1 + b[p + 1] + ((b[p + 2] & 0x3f) << 8)),
                    static_cast<std::int64_t>(1 + (b[p + 2] >> 6) + (b[p + 3] << 2) +
                                             ((b[p + 4] & 0x0f) << 10))};
        }
        if (tag(b, at, "VP8 ", 4)) {
            if (length < 10 || b[p + 3] != 0x9d || b[p + 4] != 0x01 || b[p + 5] != 0x2a)
                throw std::invalid_argument("Invalid WebP VP8 header");
            return {static_cast<std::int64_t>(number(b, p + 6, 2, true) & 0x3fff),
                    static_cast<std::int64_t>(number(b, p + 8, 2, true) & 0x3fff)};
        }
        at = p + static_cast<std::size_t>(length) + static_cast<std::size_t>(length % 2);
    }
    throw std::invalid_argument("WebP dimensions not found");
}
} // namespace

ImageHeader inspect_image_header(std::span<const std::uint8_t> encoded) {
    if (encoded.size() > 32u * 1024u * 1024u)
        throw std::invalid_argument("Encoded image exceeds 32 MiB budget");
    if (encoded.size() >= 8 &&
        std::equal(encoded.begin(), encoded.begin() + 8,
                   std::array<std::uint8_t, 8>{137,80,78,71,13,10,26,10}.begin()))
        return png(encoded);
    if (encoded.size() >= 2 && encoded[0] == 0xff && encoded[1] == 0xd8)
        return jpeg(encoded);
    if (tag(encoded, 0, "BM", 2)) return bmp(encoded);
    if (tag(encoded, 0, "II", 2) || tag(encoded, 0, "MM", 2)) return tiff(encoded);
    if (tag(encoded, 0, "RIFF", 4) && tag(encoded, 8, "WEBP", 4))
        return webp(encoded);
    throw std::invalid_argument("Unsupported still-image format (JPEG, PNG, BMP, TIFF, WebP)");
}
} // namespace chromascope
