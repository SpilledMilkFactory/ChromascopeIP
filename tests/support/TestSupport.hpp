#pragma once

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace chromascope::testing {

inline int assertion_count = 0;

inline void check(bool condition, const std::string& message) {
    ++assertion_count;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
int run(const char* suite_name, Function&& tests) {
    try {
        std::forward<Function>(tests)();
        std::cout << "PASS [" << suite_name << "]: " << assertion_count << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL [" << suite_name << "]: " << error.what()
                  << " after " << assertion_count << " checks\n";
        return 1;
    } catch (...) {
        std::cerr << "FAIL [" << suite_name << "]: unexpected non-standard exception\n";
        return 1;
    }
}

} // namespace chromascope::testing
