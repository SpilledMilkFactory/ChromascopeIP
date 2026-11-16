#pragma once

// M10.6: strict, tiny, deterministic seed corpus reader. No codecs, camera,
// model loading, networking, or writes. Seed bytes are synthetic test inputs.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace chromascope::testing {
struct MalformedSeed {
    std::string name;
    std::string outcome;
    std::vector<std::uint8_t> bytes;
};

inline std::vector<MalformedSeed> read_malformed_seeds(std::istream& stream) {
    std::vector<MalformedSeed> seeds;
    std::unordered_set<std::string> names;
    std::string line;
    while (std::getline(stream, line)) {
        // std::getline strips LF, but leaves CR from a Windows CRLF checkout.
        // Remove exactly one CR only when LF terminated the line. Bare CR at
        // EOF, embedded CR, repeated CR and overlong records still fail closed.
        if (line.size() > 1025)
            throw std::runtime_error("invalid seed manifest line size/ending");
        if (!line.empty() && line.back() == '\r' && !stream.eof())
            line.pop_back();
        if (line.size() > 1024 || line.find('\r') != std::string::npos)
            throw std::runtime_error("invalid seed manifest line size/ending");
        if (line.empty() || line[0] == '#') continue;
        const auto first = line.find('|');
        const auto second = first == std::string::npos ? first : line.find('|', first + 1);
        if (first == std::string::npos || second == std::string::npos ||
            line.find('|', second + 1) != std::string::npos)
            throw std::runtime_error("invalid seed manifest fields");
        MalformedSeed seed;
        seed.name = line.substr(0, first);
        seed.outcome = line.substr(first + 1, second - first - 1);
        if (seed.name.empty() || seed.name.size() > 64 || !names.insert(seed.name).second ||
            (seed.outcome != "pass" && seed.outcome != "reject" &&
             seed.outcome != "budget" && seed.outcome != "video"))
            throw std::runtime_error("invalid/duplicate seed metadata");
        for (const char c : seed.name) {
            if ((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '_')
                throw std::runtime_error("seed name contains unsafe characters");
        }
        const auto hex = line.substr(second + 1);
        if (hex.size() % 2 != 0 || hex.size() > 512)
            throw std::runtime_error("invalid seed byte length");
        const auto digit = [](char c) -> std::uint8_t {
            if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
            throw std::runtime_error("invalid hexadecimal seed data");
        };
        for (std::size_t i = 0; i < hex.size(); i += 2) {
            const auto high = digit(hex[i]);
            const auto low = digit(hex[i + 1]);
            seed.bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
        }
        seeds.push_back(std::move(seed));
        if (seeds.size() > 64) throw std::runtime_error("too many seeds");
    }
    if (!stream.eof()) throw std::runtime_error("failed to read seed manifest");
    return seeds;
}

inline std::vector<MalformedSeed> read_malformed_seeds(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open deterministic seed manifest");
    return read_malformed_seeds(stream);
}
} // namespace chromascope::testing
