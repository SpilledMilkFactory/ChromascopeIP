#pragma once

namespace chromascope {

// Caps the number of OpenCV/FFmpeg decoder restarts made without a single
// successfully decoded video frame. Neither network nor security-policy
// errors consume this budget: those errors are propagated immediately.
class HlsDecoderRecovery {
public:
    static constexpr unsigned max_consecutive_failures = 4;

    [[nodiscard]] bool on_failure() noexcept {
        if (failures_ == max_consecutive_failures) return false;
        ++failures_;
        return true;
    }
    void on_frame() noexcept { failures_ = 0; }
    [[nodiscard]] unsigned failures() const noexcept { return failures_; }

private:
    unsigned failures_ = 0;
};

} // namespace chromascope
