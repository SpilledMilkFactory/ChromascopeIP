#include "chromascope/GuiExperiencePolicy.hpp"
#include "support/TestSupport.hpp"

#include <cmath>
#include <limits>

namespace {
using chromascope::testing::check;
void run() {
    using namespace chromascope;
    for (float scale : {1.0F, 1.25F, 1.5F, 2.0F, 2.5F}) {
        for (int width = 240; width < 3200; width += 41) {
            for (int height : {90, 300, 600, 1050}) {
                const auto p = gui_panel_layout(static_cast<float>(width),
                    static_cast<float>(height), scale, 490.0F);
                check(std::isfinite(p.controls_width), "invalid pane width");
                check(p.controls_width > 0.0F && p.controls_width <= width,
                      "controls overflowed window");
                if (p.stacked) {
                    check(p.controls_height > 0.0F && p.controls_height < height,
                          "stacked preview has no height");
                } else {
                    check(p.controls_height == 0.0F && p.splitter_width > 0.0F,
                          "split layout missing shared height");
                    check(p.controls_width + p.splitter_width + 260.0F * scale <= width,
                          "preview clipped by resized controls");
                }
            }
        }
    }
    const auto bad = gui_panel_layout(std::numeric_limits<float>::quiet_NaN(),
        -3.0F, 0.0F, std::numeric_limits<float>::infinity());
    check(bad.stacked && std::isfinite(bad.controls_width) &&
          std::isfinite(bad.controls_height), "invalid layout inputs propagated");
}
}
int main() { return chromascope::testing::run("M10.1 resize regression", run); }
