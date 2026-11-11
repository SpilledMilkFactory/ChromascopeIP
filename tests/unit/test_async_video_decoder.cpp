#include "chromascope/AsyncVideoDecoder.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
template <typename Exception, typename Function>
void expects(Function&& function, const char* description) {
    bool caught = false;
    try { function(); } catch (const Exception&) { caught = true; }
    require(caught, description);
}
} // namespace

int main() {
    using namespace chromascope;
    static_assert(!std::is_copy_constructible_v<AsyncVideoDecoder>);
    static_assert(!std::is_move_constructible_v<AsyncVideoDecoder>);
    try {
        expects<std::invalid_argument>([] { AsyncVideoDecoder decoder({0, 4096}); },
                                       "zero frame capacity accepted");
        expects<std::invalid_argument>([] { AsyncVideoDecoder decoder({17, 4096}); },
                                       "unreasonably large frame capacity accepted");
        expects<std::invalid_argument>([] { AsyncVideoDecoder decoder({2, 0}); },
                                       "zero byte capacity accepted");
        expects<std::invalid_argument>([] { AsyncVideoDecoder decoder({2, 257U * 1024U * 1024U}); },
                                       "unreasonably large byte capacity accepted");
        AsyncVideoDecoder decoder({2, 4096});
        require(!decoder.poll(), "new decoder returned unexpected events");
        require(decoder.queued_events() == 0 && decoder.queued_pixel_bytes() == 0,
                "new decoder queue is not empty");
        expects<std::logic_error>([&] { static_cast<void>(decoder.seek(0)); },
                                  "seek without open accepted");
        expects<std::invalid_argument>([&] { static_cast<void>(decoder.open({})); },
                                       "empty path accepted");
        expects<std::invalid_argument>([&] {
            static_cast<void>(decoder.open("ignored.avi", static_cast<VideoBackend>(255)));
        }, "unknown backend accepted");
        decoder.cancel();
        require(!decoder.poll(), "cancellation generated unexpected event");
        require(decoder.queued_events() == 0 && decoder.queued_pixel_bytes() == 0,
                "cancel failed to clear queue");
        std::cout << "PASS: asynchronous decoder buffer contract and invalid requests\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "M4.3 decoder unit failure: " << exception.what() << '\n';
        return 1;
    }
}
