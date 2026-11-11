#include "chromascope/ImageViewport.hpp"
#include "support/TestSupport.hpp"

#include <cmath>

int main() {
    return chromascope::testing::run("integration.viewport_comparison", [] {
        using chromascope::ImageViewport;
        using chromascope::Point2D;
        auto near = [](double a, double b) {
            chromascope::testing::check(std::abs(a - b) < 1e-6,
                                       "comparison coordinates do not align");
        };

        ImageViewport original, processed;
        chromascope::testing::check(original.configure(4032, 3024, 500, 400),
                                   "original viewport");
        chromascope::testing::check(processed.configure(4032, 3024, 700, 600),
                                   "processed viewport on different DPI-scaled canvas");
        // Simulate the same annotation geometry rasterized in both textures:
        // its source-pixel center and bounding rectangle MUST agree, even if
        // the GPU preview was downsampled to 1800x1350 before upload.
        const Point2D box_top_left{680, 420};
        const Point2D box_bottom_right{1080, 820};
        original.zoom_at(3.0, {250, 200});
        original.pan_by(-35, 40);
        processed.synchronize_from(original);
        near(original.zoom(), processed.zoom());
        for (const auto point : {box_top_left, box_bottom_right, Point2D{900, 620}}) {
            const auto left_screen = original.image_to_canvas(point);
            const auto right_screen = processed.image_to_canvas(point);
            const auto left_pixel = original.canvas_to_image(left_screen);
            const auto right_pixel = processed.canvas_to_image(right_screen);
            near(left_pixel.x, right_pixel.x);
            near(left_pixel.y, right_pixel.y);
            near(left_pixel.x, point.x);
            near(left_pixel.y, point.y);
        }
        const auto original_center = original.canvas_to_image({250, 200});
        const auto processed_center = processed.canvas_to_image({350, 300});
        near(original_center.x, processed_center.x);
        near(original_center.y, processed_center.y);

        // Test reverse navigation from the other pane and scaled image sizes.
        processed.zoom_at(1.7, {300, 250});
        processed.pan_by(42, -24);
        original.synchronize_from(processed);
        near(original.zoom(), processed.zoom());
        const auto a = original.canvas_to_image({250, 200});
        const auto b = processed.canvas_to_image({350, 300});
        near(a.x, b.x);
        near(a.y, b.y);
        // A processed-frame size change is normalized and will not accidentally
        // interpret image coordinates in the 1800px preview texture space.
        chromascope::testing::check(processed.configure(2016, 1512, 700, 600),
                                   "different sized image output");
        processed.synchronize_from(original);
        const auto scaled_center = processed.canvas_to_image({350, 300});
        near(scaled_center.x * 2.0, a.x);
        near(scaled_center.y * 2.0, a.y);
    });
}
