// Offline Debian 12 candidate check: frame PTS must survive FFmpeg 8.1.3.
// The media sample is provided privately by the operator, never redistributed.
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cmath>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: chromascope_v101_p2_pts_probe /local/sample.ts\n";
        return 64;
    }
    cv::VideoCapture cap(argv[1], cv::CAP_FFMPEG);
    if (!cap.isOpened()) {
        std::cerr << "CAPTURE_OPENED=0\n";
        return 2;
    }
    cv::Mat frame;
    unsigned frames = 0;
    unsigned valid = 0;
    unsigned short_intervals = 0;
    unsigned long_intervals = 0;
    double first = -1.0;
    double last = -1.0;
    while (frames < 1000 && cap.read(frame)) {
        if (frame.empty()) break;
        ++frames;
        const double pts = cap.get(cv::CAP_PROP_POS_MSEC);
        if (!std::isfinite(pts) || pts < 0.0) continue;
        ++valid;
        if (first < 0.0) first = pts;
        if (last >= 0.0) {
            const double interval = pts - last;
            if (interval < -0.001) {
                std::cerr << "PTS_REGRESSION_MS=" << interval << '\n';
                return 3;
            }
            if (interval > 0 && interval <= 75.0) ++short_intervals;
            if (interval >= 85.0 && interval < 300.0) ++long_intervals;
        }
        last = pts;
    }
    const double span = last - first;
    std::cout << std::fixed << std::setprecision(3)
              << "FRAMES=" << frames << '\n'
              << "VALID_PTS=" << valid << '\n'
              << "MEDIA_SPAN_MS=" << span << '\n'
              << "SHORT_INTERVALS=" << short_intervals << '\n'
              << "LONG_INTERVALS=" << long_intervals << '\n';
    // Known private sample: 276 frames span ~18.364 s and contains variable
    // 50/100 ms presentation steps. Fail rather than silently using nominal FPS.
    return frames == 276 && valid >= 270 && span >= 18000 && span <= 19000 &&
                   short_intervals >= 10 && long_intervals >= 10 ? 0 : 4;
}
