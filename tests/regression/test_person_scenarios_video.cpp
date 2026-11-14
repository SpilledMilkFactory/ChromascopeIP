#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/DnnPreprocessing.hpp"
#include "chromascope/VideoSource.hpp"
#include "support/TestSupport.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <variant>
#include <system_error>

namespace {
using namespace chromascope;
using chromascope::testing::check;
namespace fs = std::filesystem;

struct Rect {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
    double score = .9;
    int class_id = 0;
};

// Hand-authored source-space detection candidates, NOT neural-network
// predictions. Deliberate omissions and a false positive exercise contracts.
const std::array<std::vector<Rect>, 8> kCandidates{{
    {{5, 30, 42, 150, .93}, {43, 35, 46, 145, .87},
     {153, 48, 42, 125, .80}, {5, 30, 42, 150, .72},
     {245, 60, 36, 115, .95, 33}}, // crowd + duplicate + nonperson
    {{13, 28, 40, 148, .95}, {65, 30, 42, 144, .83},
     {120, 30, 42, 148, .99, 33}}, // occluded crowd
    {{25, 100, 9, 23, .73}, {230, 110, 14, 32, .69}}, // tiny person and false positive
    {{80, 6, 140, 230, .95}}, // close person
    {{12, 50, 55, 162, .89}}, // partial occlusion: second person missed
    {{-8, 22, 46, 168, .81}}, // source boundary clipping to x=0
    {}, // empty frame, no predictions
    {{110, 65, 42, 140, .65}} // empty scene with false positive
}};
constexpr std::array<std::size_t, 8> kExpectedCounts{{3, 2, 2, 1, 1, 1, 0, 1}};

void draw_scenario(cv::Mat& pixels, std::size_t index) {
    pixels.setTo(cv::Scalar(31, 37, 43));
    // Entirely generated video content; colored rectangles are geometric
    // placeholders for person bounding boxes, not actual people photographs.
    const auto draw = [&](int x, int y, int w, int h) {
        cv::rectangle(pixels, cv::Rect(x, y, w, h), cv::Scalar(95, 164, 199), -1);
    };
    switch (index) {
    case 0:
        draw(5,30,42,150); draw(43,35,46,145);
        draw(90,40,50,138); draw(153,48,42,125); break;
    case 1:
        draw(13,28,40,148); draw(65,30,42,144);
        draw(120,30,42,148); break;
    case 2:
        draw(25,100,9,23); draw(74,103,12,26); draw(134,99,14,32); break;
    case 3: draw(80,6,140,230); break;
    case 4:
        draw(12,50,55,162); draw(105,40,65,172);
        cv::rectangle(pixels, cv::Rect(120,60,70,110), cv::Scalar(31,37,43), -1);
        break;
    case 5: draw(0,22,38,168); break;
    case 6: break;
    case 7: // only a geometric distractor, no annotated person
        cv::rectangle(pixels, cv::Rect(110,65,42,140), cv::Scalar(40,100,60), -1);
        break;
    default: throw std::logic_error("Unknown synthetic person scenario");
    }
}

std::vector<DnnBoxCandidate> proposals(std::size_t index,
                                        const DnnPreprocessGeometry& geometry) {
    std::vector<DnnBoxCandidate> result;
    for (const Rect& rect : kCandidates[index]) {
        const auto [x0, y0] = geometry.source_to_model(rect.x, rect.y);
        const auto [x1, y1] = geometry.source_to_model(rect.x + rect.w, rect.y + rect.h);
        result.push_back({x0, y0, x1, y1, rect.score, rect.class_id});
    }
    return result;
}

void run(const fs::path& directory) {
    fs::create_directories(directory);
    const fs::path path = directory / "m7-4-synthetic-person-scenarios.avi";
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ec; fs::remove(path, ec); }
    } cleanup{path};
    cv::VideoWriter writer(path.string(), cv::CAP_OPENCV_MJPEG,
        cv::VideoWriter::fourcc('M','J','P','G'), 15.0, cv::Size(320,240), true);
    check(writer.isOpened(), "Could not create generated MJPEG person scenario video");
    for (std::size_t index = 0; index < kCandidates.size(); ++index) {
        cv::Mat pixels(240, 320, CV_8UC3);
        draw_scenario(pixels, index);
        writer.write(pixels);
    }
    writer.release();
    check(fs::is_regular_file(path) && fs::file_size(path) > 128,
          "Generated synthetic scenario video was not written");

    DnnPostprocessOptions options;
    options.person_only = true;
    options.person_display_threshold = .60;
    options.nms_iou_threshold = .60;
    auto source = VideoSource::open(path, VideoBackend::OpenCVMJPEG);
    for (std::size_t index = 0; index < kCandidates.size(); ++index) {
        const auto read = source.read_next();
        check(read.status == VideoReadStatus::Frame && read.frame.has_value(),
              "Generated scenario video lost a frame");
        const auto& frame = *read.frame;
        check(frame.index == index && frame.image.width() == 320 &&
              frame.image.height() == 240,
              "Generated scenario frame metadata/index changed");
        const auto geometry = make_dnn_preprocess_geometry(frame.image.width(), frame.image.height());
        DnnPostprocessSummary diagnostics;
        const auto boxes = postprocess_dnn_boxes(proposals(index, geometry),
                                                  geometry, options, &diagnostics);
        check(boxes.width() == 320 && boxes.height() == 240,
              "Person scenario used model pixel dimensions");
        check(boxes.objects().size() == kExpectedCounts[index],
              "Person scene count, NMS or confidence gate regressed at frame " +
                  std::to_string(index));
        for (const auto& object : boxes.objects()) {
            check(object.class_id == 0 && object.label == "person",
                  "Person-only scenario leaked nonperson detection");
        }
        const auto layer = make_object_confidence_annotations(boxes);
        check(layer.items().size() == boxes.objects().size(),
              "Confidence labels do not match retained person boxes");
        if (index == 0) {
            check(diagnostics.accepted_detections == 3,
                  "Duplicate crowd person was not suppressed");
        }
        if (index == 5) {
            const auto& clipped = std::get<BoxGeometry>(boxes.objects()[0].geometry);
            check(std::abs(clipped.x) < .001 && std::abs(clipped.width - 38.0) < .001,
                  "Partially out-of-frame person was not clipped to image edge");
        }
        if (index == 6) {
            check(diagnostics.status() == "no_accepted_detection",
                  "Empty scene must remain empty, not an invented person");
        }
    }
    check(source.read_next().status == VideoReadStatus::EndOfStream,
          "Generated scenario video did not terminate at EOF");
    source.seek_to(5);
    const auto replay = source.read_next();
    check(replay.frame.has_value() && replay.frame->index == 5,
          "Replaying cropped boundary scenario after seek failed");
    source.seek_to(6);
    const auto blank = source.read_next();
    check(blank.frame.has_value() && blank.frame->index == 6,
          "Seeking into empty scene failed");
}
} // namespace

int main(int argc, char** argv) {
    return chromascope::testing::run("M7.4 crowds, scale, occlusion and empty video", [&] {
        if (argc != 2) throw std::invalid_argument("Expected build-tree output directory");
        run(argv[1]);
    });
}
