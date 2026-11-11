#include "chromascope/VideoPlayer.hpp"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
template <typename Predicate> void until(Predicate&& predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error(message);
}
struct RemoveFile {
    std::filesystem::path path;
    ~RemoveFile() { std::error_code error; std::filesystem::remove(path, error); }
};
}

int main(int argc, char** argv) {
    using namespace chromascope;
    try {
        require(argc == 2, "requires test output directory");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        RemoveFile media{directory / "m4-4-player.avi"};
        {
            cv::VideoWriter writer(media.path.string(), cv::CAP_OPENCV_MJPEG,
                                   cv::VideoWriter::fourcc('M','J','P','G'), 12.0,
                                   cv::Size(80, 60), true);
            require(writer.isOpened(), "MJPEG writer unavailable");
            for (int n = 0; n < 8; ++n) {
                cv::Mat frame(60, 80, CV_8UC3, cv::Scalar(10 + 15 * n, 40 + n, 120 - n));
                writer.write(frame);
            }
            writer.release();
        }
        VideoPlayer player({2, 80 * 60 * 3 * 2});
        using Clock = VideoPlayer::Clock;
        player.open(media.path, VideoBackend::OpenCVMJPEG);
        require(player.has_source() && !player.playing(), "opening starts paused");
        until([&] { return player.update(); }, "initial frame timeout");
        require(player.current_frame() && player.current_frame()->index == 0,
                "initial first frame");
        require(player.current_frame()->image.width() == 80 &&
                player.current_frame()->image.height() == 60, "frame dimensions");
        require(player.stream_info().has_value(), "metadata published from worker");
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            require(!player.update(), "paused player must not advance");
        }
        player.step_forward();
        until([&] { return player.update(); }, "step forward timeout");
        require(player.current_frame()->index == 1, "step forward index");
        player.step_backward();
        until([&] { return player.update(); }, "step backward timeout");
        require(player.current_frame()->index == 0, "step backward index");
        player.seek(5);
        player.seek(3);
        until([&] { return player.update(); }, "latest seek timeout");
        require(player.current_frame()->index == 3, "stale seek result must not appear");
        player.play();
        require(player.playing(), "play starts transport");
        std::size_t consumed = 0;
        auto future = Clock::now();
        until([&] {
            future += std::chrono::seconds(1);
            if (player.update(future)) ++consumed;
            return player.ended();
        }, "end of stream timeout");
        require(consumed == 4 && player.current_frame()->index == 7,
                "playback must deliver all frames before EOF");
        require(!player.playing(), "EOF pauses playback");
        player.play();
        until([&] { return player.update(); }, "restart timeout");
        require(player.current_frame()->index == 0, "play after EOF restarts at zero");
        player.close();
        require(!player.update() && !player.current_frame(), "closed source does not publish");
        player.open(directory / "missing-video.avi");
        until([&] {
            (void)player.update();
            return player.status().find("Video error:") != std::string::npos;
        }, "invalid source error timeout");
        require(!player.playing() && !player.current_frame(), "error is terminal and safe");
        player.open(media.path, VideoBackend::OpenCVMJPEG);
        until([&] { return player.update(); }, "recovery after error timeout");
        require(player.current_frame()->index == 0, "reopen after failure");
        std::cout << "PASS: M4.4 video GUI presentation controller and playback lifecycle\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
