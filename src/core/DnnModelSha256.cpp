#include "chromascope/DnnGuiSession.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace chromascope {
namespace {
constexpr std::array<std::uint32_t, 64> kConstants{
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

class Sha256 final {
public:
    void update(const std::uint8_t* bytes, std::size_t count) {
        for (std::size_t n = 0; n < count; ++n) {
            buffer_[used_++] = bytes[n];
            if (used_ == buffer_.size()) { compress(); used_ = 0; }
        }
        total_ += count;
    }
    [[nodiscard]] std::string finish() {
        const std::uint64_t bits = total_ * 8U;
        buffer_[used_++] = 0x80U;
        if (used_ > 56) {
            while (used_ != 64) buffer_[used_++] = 0;
            compress(); used_ = 0;
        }
        while (used_ < 56) buffer_[used_++] = 0;
        for (unsigned i = 0; i < 8; ++i)
            buffer_[56+i] = static_cast<std::uint8_t>(bits >> (56U - 8U*i));
        compress();
        std::ostringstream result;
        result << std::hex << std::setfill('0');
        for (auto word : hash_) result << std::setw(8) << word;
        return result.str();
    }
private:
    void compress() {
        std::array<std::uint32_t,64> w{};
        for (unsigned i=0; i<16; ++i) {
            const unsigned k=4U*i;
            w[i] = (static_cast<std::uint32_t>(buffer_[k])<<24U) |
                   (static_cast<std::uint32_t>(buffer_[k+1])<<16U) |
                   (static_cast<std::uint32_t>(buffer_[k+2])<<8U) | buffer_[k+3];
        }
        for (unsigned i=16; i<64; ++i) {
            const auto s0 = std::rotr(w[i-15],7) ^ std::rotr(w[i-15],18) ^ (w[i-15]>>3U);
            const auto s1 = std::rotr(w[i-2],17) ^ std::rotr(w[i-2],19) ^ (w[i-2]>>10U);
            w[i] = w[i-16]+s0+w[i-7]+s1;
        }
        auto [a,b,c,d,e,f,g,h] = std::array<std::uint32_t,8>{hash_[0],hash_[1],hash_[2],hash_[3],hash_[4],hash_[5],hash_[6],hash_[7]};
        for (unsigned i=0;i<64;++i) {
            const auto s1=std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25);
            const auto ch=(e&f)^(~e&g);
            const auto t1=h+s1+ch+kConstants[i]+w[i];
            const auto s0=std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22);
            const auto maj=(a&b)^(a&c)^(b&c);
            const auto t2=s0+maj;
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        hash_[0]+=a; hash_[1]+=b; hash_[2]+=c; hash_[3]+=d;
        hash_[4]+=e; hash_[5]+=f; hash_[6]+=g; hash_[7]+=h;
    }
    std::array<std::uint32_t,8> hash_{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                                     0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    std::array<std::uint8_t,64> buffer_{};
    std::size_t used_=0;
    std::uint64_t total_=0;
};
} // namespace

std::string dnn_file_sha256(const std::filesystem::path& file) {
    std::error_code ec;
    if (file.extension() != ".onnx" && file.extension() != ".bin") {
        // Hashing test vectors in regular .bin files is allowed; no arbitrary
        // filename extension can be confused with an accepted ONNX model.
        throw std::invalid_argument("Model digest requires an .onnx or .bin file");
    }
    if (!std::filesystem::is_regular_file(file, ec) || ec)
        throw std::invalid_argument("Model is not an accessible regular file");
    const auto size=std::filesystem::file_size(file,ec);
    if (ec || size==0 || size>128ULL*1024ULL*1024ULL)
        throw std::invalid_argument("Model must be nonempty and no larger than 128 MiB");
    std::ifstream stream(file,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open model for integrity verification");
    Sha256 hash;
    std::array<char,65536> block{};
    std::uintmax_t consumed=0;
    while (stream) {
        stream.read(block.data(),static_cast<std::streamsize>(block.size()));
        const auto count=stream.gcount();
        if (count>0) {
            consumed+=static_cast<std::uintmax_t>(count);
            if (consumed>128ULL*1024ULL*1024ULL)
                throw std::length_error("Model changed or grew during verification");
            hash.update(reinterpret_cast<const std::uint8_t*>(block.data()),
                        static_cast<std::size_t>(count));
        }
    }
    if (!stream.eof() || consumed!=size)
        throw std::runtime_error("Model changed or could not be read completely");
    return hash.finish();
}

} // namespace chromascope
