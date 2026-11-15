#include "chromascope/LiveGuiControlPolicy.hpp"
#include "support/TestSupport.hpp"

#include <stdexcept>

namespace {
using chromascope::testing::check;
void run() {
    using chromascope::live_inference_spacing_ms;
    using chromascope::live_confidence_visible;
    check(live_inference_spacing_ms(0, 0) == 0, "default should remain uncapped");
    check(live_inference_spacing_ms(60, 0) == 17, "60 FPS must round to 17 ms");
    check(live_inference_spacing_ms(30, 0) == 34, "30 FPS must round up");
    check(live_inference_spacing_ms(1, 0) == 1000, "minimum supported FPS incorrect");
    check(live_inference_spacing_ms(0, 100) == 100, "legacy M9.2 pacing changed");
    check(live_inference_spacing_ms(60, 100) == 100, "strongest pacing must win");
    check(live_inference_spacing_ms(10, 3) == 100, "FPS cap not enforced");
    for (const int fps : {-1, 61, 1000}) {
        bool threw = false;
        try { (void)live_inference_spacing_ms(fps, 0); }
        catch (const std::invalid_argument&) { threw = true; }
        check(threw, "bad FPS accepted");
    }
    for (const int ms : {-1, 1001}) {
        bool threw = false;
        try { (void)live_inference_spacing_ms(0, ms); }
        catch (const std::invalid_argument&) { threw = true; }
        check(threw, "invalid M9.2 pacing accepted");
    }
    check(live_confidence_visible(0.50, 0.50), "inclusive threshold changed");
    check(!live_confidence_visible(0.49, 0.50), "below-cutoff detection displayed");
    check(live_confidence_visible(1.0, 1.0), "max confidence rejected");
    check(!live_confidence_visible(-0.1, 0.5), "invalid score accepted");
    check(!live_confidence_visible(1.1, 0.5), "out-of-range score accepted");
    check(!live_confidence_visible(0.9, -0.1), "invalid threshold accepted");
    check(!live_confidence_visible(0.9, 1.1), "invalid threshold accepted");
}
}
int main() { return chromascope::testing::run("M9.5 GUI control policy", run); }
