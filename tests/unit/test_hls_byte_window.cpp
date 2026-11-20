#include "chromascope/HlsByteWindow.hpp"
#include "chromascope/HlsDecoderRecovery.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
std::vector<std::uint8_t> bytes(const char* text) {
    const std::string value(text);
    return {value.begin(), value.end()};
}
}

int main() {
    try {
        chromascope::HlsByteWindow stream;
        std::array<char, 12> output{};
        require(stream.read(output.data(), output.size()) == 0, "Empty stream must not fabricate bytes");
        stream.append(bytes("segment-A"));
        require(stream.read(output.data(), 4) == 4 && std::string(output.data(), 4) == "segm",
                "First segment byte content");
        require(stream.seek(-4, SEEK_CUR) == 0, "Backward probe seek");
        require(stream.read(output.data(), 9) == 9 && std::string(output.data(), 9) == "segment-A",
                "Probe replay");
        stream.append(bytes("segment-B"));
        require(stream.read(output.data(), 9) == 9 && std::string(output.data(), 9) == "segment-B",
                "Continuous TS bytes after segment boundary");
        require(stream.seek(8, SEEK_SET) == 8 && stream.read(output.data(), 2) == 1 &&
                output[0] == 'A' && stream.read(output.data() + 1, 1) == 1 &&
                std::string(output.data(), 2) == "As", "Seek across adjacent segment boundary");
        require(stream.seek(0, SEEK_END) == -1 && stream.seek(500, SEEK_SET) == -1,
                "Cannot pretend an unbounded live stream has a seekable end");
        require(stream.seek(18, SEEK_SET) == 18, "Seek to current live end");
        stream.append(bytes("CCC"));
        stream.append(bytes("DDD"));
        require(stream.retained_segments() == 4, "Memory window keeps at most four segments");
        stream.append(bytes("EEE"));
        require(stream.retained_segments() == 4 && stream.seek(0, SEEK_SET) == -1,
                "Old window data evicted and cannot be re-fetched via seek");
        require(stream.retained_bytes() == 9 + 3 + 3 + 3, "Bounded retained byte count");
        require(stream.seek(18, SEEK_SET) == 18 && stream.read(output.data(), 3) == 3 &&
                std::string(output.data(), 3) == "CCC", "Retained byte accuracy after eviction");
        require(stream.seek(-500, SEEK_CUR) == -1, "Negative seek underflow refused");
        bool refused = false;
        try { stream.append({}); } catch (const std::invalid_argument&) { refused = true; }
        require(refused, "Zero-byte HLS segment refused");
        refused = false;
        try { stream.append(std::vector<std::uint8_t>(
            chromascope::HlsByteWindow::max_segment_bytes + 1));
        } catch (const std::invalid_argument&) { refused = true; }
        require(refused, "Oversized HLS segment refused before buffering");
        refused = false;
        try { (void)stream.read(nullptr, 1); } catch (const std::invalid_argument&) { refused = true; }
        require(refused, "Invalid decoder destination refused");
        chromascope::HlsDecoderRecovery recovery;
        for (unsigned i = 0; i < chromascope::HlsDecoderRecovery::max_consecutive_failures; ++i)
            require(recovery.on_failure(), "Bounded decoder recovery permits an attempt");
        require(!recovery.on_failure(), "Decoder recovery must stop after bounded failures");
        recovery.on_frame();
        require(recovery.failures() == 0 && recovery.on_failure(),
                "A successfully decoded frame resets the recovery budget");
        std::cout << "PASS: bounded continuous HLS MPEG-TS byte window and decoder recovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
