#include "chromascope/WindowsEnvironment.hpp"
#include "support/TestSupport.hpp"

#ifdef _WIN32
#include <windows.h>

#include <filesystem>
#include <string>
#include <utility>

namespace {

// Test variables are process-local and intentionally distinct from user settings.
class ScopedTestEnvironment final {
public:
    explicit ScopedTestEnvironment(const wchar_t* name) : name_(name) {
        const DWORD size = GetEnvironmentVariableW(name_, nullptr, 0);
        if (size > 0 && size <= 32768) {
            std::wstring value(size, L'\0');
            const DWORD count = GetEnvironmentVariableW(name_, value.data(), size);
            if (count > 0 && count < size) {
                value.resize(count);
                prior_ = std::move(value);
                had_prior_ = true;
            }
        }
    }
    ~ScopedTestEnvironment() {
        SetEnvironmentVariableW(name_, had_prior_ ? prior_.c_str() : nullptr);
    }
    ScopedTestEnvironment(const ScopedTestEnvironment&) = delete;
    ScopedTestEnvironment& operator=(const ScopedTestEnvironment&) = delete;
private:
    const wchar_t* name_;
    std::wstring prior_;
    bool had_prior_ = false;
};

void test_windows_environment() {
    using chromascope::testing::check;
    constexpr auto kPathKey = L"CHROMASCOPE_P5_TEST_UNICODE_PATH";
    constexpr auto kTraceKey = L"CHROMASCOPE_P5_TEST_TRACE_FLAG";
    const ScopedTestEnvironment restore_path(kPathKey);
    const ScopedTestEnvironment restore_trace(kTraceKey);

    SetEnvironmentVariableW(kPathKey, nullptr);
    check(!chromascope::windows_environment_path(kPathKey), "missing path is absent");
    check(SetEnvironmentVariableW(kPathKey, L"C:\\Program Files\\ChromascopeIP") != 0,
          "assign absolute test path");
    const auto path = chromascope::windows_environment_path(kPathKey);
    check(path && *path == std::filesystem::path(L"C:\\Program Files\\ChromascopeIP"),
          "wide Windows path is preserved exactly");
    check(!chromascope::windows_environment_path(nullptr), "null variable rejected");
    check(SetEnvironmentVariableW(kPathKey, L"") != 0, "clear path with empty value");
    check(!chromascope::windows_environment_path(kPathKey), "empty path is absent");

    check(SetEnvironmentVariableW(kTraceKey, L"1") != 0, "enable trace");
    check(chromascope::windows_environment_enabled(kTraceKey), "exact 1 enables trace");
    check(SetEnvironmentVariableW(kTraceKey, L"10") != 0, "set extended trace");
    check(!chromascope::windows_environment_enabled(kTraceKey), "longer value rejected");
    check(SetEnvironmentVariableW(kTraceKey, L"0") != 0, "disable trace");
    check(!chromascope::windows_environment_enabled(kTraceKey), "0 rejected");
    check(SetEnvironmentVariableW(kTraceKey, nullptr) != 0, "clear trace");
    check(!chromascope::windows_environment_enabled(kTraceKey), "missing trace rejected");
}
} // namespace

int main() {
    return chromascope::testing::run("unit.windows_environment", test_windows_environment);
}
#else
int main() { return 0; }
#endif
