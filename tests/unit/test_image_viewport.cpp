#include "chromascope/ImageViewport.hpp"

#include "support/TestSupport.hpp"

#include <cmath>
#include <limits>

int main() {
    return chromascope::testing::run("unit.image_viewport", [] {
        using chromascope::ImageViewport;
        using chromascope::Point2D;
        const auto close = [](double actual, double expected, double tolerance = 1e-7) {
            chromascope::testing::check(std::abs(actual - expected) < tolerance, "viewport transform mismatch");
        };

        ImageViewport view;
        chromascope::testing::check(!view.valid(), "default viewport is invalid");
        chromascope::testing::check(!view.configure(0.0, 1.0, 800.0, 600.0), "reject zero dimensions");
        chromascope::testing::check(!view.configure(std::numeric_limits<double>::infinity(), 1.0, 800.0, 600.0),
                "reject nonfinite dimensions");
        chromascope::testing::check(view.configure(4032.0, 3024.0, 900.0, 600.0), "configure valid viewport");
        close(view.pixels_per_image_pixel(), 600.0 / 3024.0);
        const auto center = view.image_to_canvas({2016.0, 1512.0});
        close(center.x, 450.0);
        close(center.y, 300.0);

        // Keep original-image coordinates, not 1800-pixel GPU preview coordinates.
        const Point2D sample{1430.5, 1850.25};
        auto painted = view.image_to_canvas(sample);
        auto recovered = view.canvas_to_image(painted);
        close(recovered.x, sample.x);
        close(recovered.y, sample.y);

        // Zoom about the cursor; point under cursor must stay fixed when it can
        // do so without exposing empty space.
        const Point2D anchor{500.0, 350.0};
        const auto anchored_pixel = view.canvas_to_image(anchor);
        view.zoom_at(2.5, anchor);
        close(view.zoom(), 2.5);
        const auto anchored_screen = view.image_to_canvas(anchored_pixel);
        close(anchored_screen.x, anchor.x);
        close(anchored_screen.y, anchor.y);
        view.pan_by(80.0, -50.0);
        recovered = view.canvas_to_image(view.image_to_canvas(sample));
        close(recovered.x, sample.x);
        close(recovered.y, sample.y);

        // Enormous drags cannot move the entire image outside the canvas.
        view.pan_by(1000000.0, -1000000.0);
        const auto bounds = view.image_rect();
        chromascope::testing::check(bounds.minimum.x <= 0.0 && bounds.maximum.x >= 0.0,
                "horizontal content remains reachable");
        chromascope::testing::check(bounds.minimum.y <= 600.0 && bounds.maximum.y >= 600.0,
                "vertical content remains reachable");

        // Resizing the framebuffer (e.g. different DPI) does not change
        // source pixel math. ImGui logical canvas sizes are the input.
        chromascope::testing::check(view.configure(4032.0, 3024.0, 1200.0, 800.0), "DPI resize");
        recovered = view.canvas_to_image(view.image_to_canvas(sample));
        close(recovered.x, sample.x);
        close(recovered.y, sample.y);
        view.fit();
        close(view.zoom(), 1.0);
        close(view.image_to_canvas({2016.0, 1512.0}).x, 600.0);
        close(view.image_to_canvas({2016.0, 1512.0}).y, 400.0);
        view.zoom_at(1000000.0, {600.0, 400.0});
        close(view.zoom(), 32.0);
        view.zoom_at(0.00001, {600.0, 400.0});
        close(view.zoom(), 1.0);
        chromascope::testing::check(!view.contains_image_pixel({4032.0, 10.0}), "image right edge excluded");
        chromascope::testing::check(view.contains_image_pixel({4031.0, 3023.0}), "last pixel included");
    });
}
