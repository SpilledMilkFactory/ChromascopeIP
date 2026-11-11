#include "chromascope/VideoPlayer.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
template <typename F> void must_throw(F&& action, const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    using namespace chromascope;
    try {
        static_assert(!std::is_copy_constructible_v<VideoPlayer>);
        static_assert(!std::is_move_constructible_v<VideoPlayer>);
        VideoPlayer player({2, 1024 * 1024});
        require(!player.has_source() && !player.playing(), "initially idle");
        require(!player.update(), "idle cannot produce frames");
        player.play();
        player.step_forward();
        player.step_backward();
        require(!player.playing(), "idle play has no effect");
        must_throw([&] { player.seek(0); }, "seek without recording rejected");
        must_throw([&] { player.open({}); }, "empty recording path rejected");
        must_throw([&] { player.set_playback_fps(0); }, "zero FPS rejected");
        must_throw([&] { player.set_playback_fps(121); }, "excess FPS rejected");
        must_throw([&] { player.set_playback_fps(NAN); }, "NaN FPS rejected");
        player.set_playback_fps(24);
        require(player.playback_fps() == 24, "explicit FPS setting");
        player.close();
        require(!player.has_source() && !player.current_frame(), "closed source is empty");
        std::cout << "PASS: VideoPlayer headless lifecycle and configuration\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
