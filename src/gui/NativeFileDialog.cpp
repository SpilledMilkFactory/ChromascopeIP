#include "chromascope/NativeFileDialog.hpp"

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#else
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace chromascope {
namespace {

struct DialogFilter {
    const char* title;
    const char* globs;
    const wchar_t* win_globs;
};

[[nodiscard]] DialogFilter filter_for(GuiPickerKind kind) noexcept {
    switch (kind) {
    case GuiPickerKind::Image:
        return {"Images", "*.jpg *.jpeg *.png *.bmp *.tif *.tiff *.webp",
                L"*.jpg;*.jpeg;*.png;*.bmp;*.tif;*.tiff;*.webp"};
    case GuiPickerKind::Video:
        return {"Videos", "*.avi *.mp4 *.mov *.mkv *.webm *.m4v *.mpg *.mpeg *.wmv",
                L"*.avi;*.mp4;*.mov;*.mkv;*.webm;*.m4v;*.mpg;*.mpeg;*.wmv"};
    case GuiPickerKind::Model:
        return {"ONNX models", "*.onnx", L"*.onnx"};
    case GuiPickerKind::HlsPlaylist:
        return {"HLS playlists", "*.m3u8", L"*.m3u8"};
    }
    return {"Files", "*", L"*"};
}

[[nodiscard]] const char* title_for(GuiPickerKind kind) noexcept {
    switch (kind) {
    case GuiPickerKind::Image: return "Select an image";
    case GuiPickerKind::Video: return "Select a recorded video";
    case GuiPickerKind::Model: return "Select an ONNX model";
    case GuiPickerKind::HlsPlaylist: return "Select a downloaded HLS playlist";
    }
    return "Select a file";
}

#if defined(_WIN32)
// Use Windows' Explorer-style IFileOpenDialog instead of asking the user to
// install an auxiliary picker executable or relying on the current directory.
[[nodiscard]] NativeFileDialogResult windows_dialog(
        GuiPickerKind kind, const std::filesystem::path& initial_directory) {
    const HRESULT initialized = CoInitializeEx(nullptr,
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(initialized)) {
        return {NativeDialogStatus::Unavailable, {},
                "Native Windows file dialog could not initialize COM."};
    }
    IFileOpenDialog* dialog = nullptr;
    const HRESULT created = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                             CLSCTX_INPROC_SERVER,
                                             IID_PPV_ARGS(&dialog));
    if (FAILED(created) || !dialog) {
        CoUninitialize();
        return {NativeDialogStatus::Unavailable, {},
                "Windows File Open dialog is unavailable."};
    }
    const auto finish = [&](NativeFileDialogResult result) {
        dialog->Release();
        CoUninitialize();
        return result;
    };
    DWORD options = 0;
    if (FAILED(dialog->GetOptions(&options)) ||
        FAILED(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST |
                                  FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR |
                                  FOS_NODEREFERENCELINKS))) {
        return finish({NativeDialogStatus::Error, {},
                       "Unable to configure Windows file dialog."});
    }
    const auto filter = filter_for(kind);
    const std::wstring label(filter.title, filter.title + std::strlen(filter.title));
    const COMDLG_FILTERSPEC file_types[] = {{label.c_str(), filter.win_globs}};
    if (FAILED(dialog->SetFileTypes(1, file_types)) ||
        FAILED(dialog->SetFileTypeIndex(1))) {
        return finish({NativeDialogStatus::Error, {}, "Unable to apply file type filter."});
    }
    const std::string title = title_for(kind);
    const std::wstring wide_title(title.begin(), title.end());
    (void)dialog->SetTitle(wide_title.c_str());
    if (!initial_directory.empty()) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initial_directory.c_str(), nullptr,
                                                 IID_PPV_ARGS(&folder))) && folder) {
            (void)dialog->SetDefaultFolder(folder);
            folder->Release();
        }
    }
    const HRESULT shown = dialog->Show(nullptr);
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return finish({NativeDialogStatus::Cancelled, {}, {}});
    if (FAILED(shown))
        return finish({NativeDialogStatus::Error, {}, "Windows file selection failed."});
    IShellItem* item = nullptr;
    if (FAILED(dialog->GetResult(&item)) || !item)
        return finish({NativeDialogStatus::Error, {}, "No selected filesystem file."});
    PWSTR raw = nullptr;
    const HRESULT name_result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
    item->Release();
    if (FAILED(name_result) || !raw)
        return finish({NativeDialogStatus::Error, {}, "Selection is not a local filesystem path."});
    const std::filesystem::path chosen(raw);
    CoTaskMemFree(raw);
    return finish({NativeDialogStatus::Selected, chosen, {}});
}
#else

[[nodiscard]] bool contains_desktop(std::string_view desktop, std::string_view token) {
    // XDG_CURRENT_DESKTOP may contain multiple colon-separated entries.
    for (std::size_t begin = 0; begin < desktop.size();) {
        const auto end = desktop.find(':', begin);
        const auto part = desktop.substr(begin, end == std::string_view::npos ? end : end - begin);
        if (part == token) return true;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return false;
}

[[nodiscard]] std::filesystem::path default_directory(
        const std::filesystem::path& initial) {
    std::error_code ec;
    if (!initial.empty() && std::filesystem::is_directory(initial, ec) && !ec)
        return std::filesystem::absolute(initial, ec);
    const char* home = std::getenv("HOME");
    if (home && *home && std::filesystem::is_directory(home, ec) && !ec)
        return std::filesystem::path(home);
    return "/"; // Never start in the arbitrary portable extraction directory.
}

// Use posix_spawnp and an argv array, not a command shell. Paths and titles
// cannot become shell syntax; captured output is bounded.
[[nodiscard]] NativeFileDialogResult run_helper(
        const std::vector<std::string>& args) {
    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) != 0)
        return {NativeDialogStatus::Error, {}, "Could not create picker output pipe."};
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return {NativeDialogStatus::Error, {}, "Could not prepare native dialog."};
    }
    (void)posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
    (void)posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
    (void)posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    pid_t child = -1;
    const int spawned = posix_spawnp(&child, argv.front(), &actions,
                                    nullptr, argv.data(), environ);
    (void)posix_spawn_file_actions_destroy(&actions);
    close(pipe_fds[1]);
    if (spawned != 0) {
        close(pipe_fds[0]);
        if (spawned == ENOENT)
            return {NativeDialogStatus::Unavailable, {}, "Native dialog helper is not installed."};
        return {NativeDialogStatus::Error, {}, "Could not start native file dialog."};
    }
    std::string output;
    std::array<char, 1024> buffer{};
    bool overflow = false;
    for (;;) {
        const auto n = read(pipe_fds[0], buffer.data(), buffer.size());
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            overflow = true;
            break;
        }
        if (output.size() + static_cast<std::size_t>(n) > 8192) overflow = true;
        else if (!overflow) output.append(buffer.data(), static_cast<std::size_t>(n));
    }
    close(pipe_fds[0]);
    int exit_status = 0;
    while (waitpid(child, &exit_status, 0) < 0) {
        if (errno != EINTR)
            return {NativeDialogStatus::Error, {}, "Could not wait for native dialog."};
    }
    if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 1)
        return {NativeDialogStatus::Cancelled, {}, {}};
    if (!WIFEXITED(exit_status) || WEXITSTATUS(exit_status) != 0 || overflow)
        return {NativeDialogStatus::Error, {}, "Native file chooser did not return a valid path."};
    while (!output.empty() && (output.back() == '\r' || output.back() == '\n'))
        output.pop_back();
    if (output.empty() || output.find('\n') != std::string::npos ||
        output.find('\r') != std::string::npos || output.find('\0') != std::string::npos)
        return {NativeDialogStatus::Error, {}, "Native file chooser returned an invalid selection."};
    const std::filesystem::path selected(output);
    if (!selected.is_absolute())
        return {NativeDialogStatus::Error, {}, "File chooser returned a non-absolute path."};
    return {NativeDialogStatus::Selected, selected, {}};
}

[[nodiscard]] NativeFileDialogResult linux_dialog(
        GuiPickerKind kind, const std::filesystem::path& initial_directory) {
    const auto filter = filter_for(kind);
    const auto directory = default_directory(initial_directory);
    const char* desktop_env = std::getenv("XDG_CURRENT_DESKTOP");
    const std::string_view desktop = desktop_env ? desktop_env : "";
    const bool kde = contains_desktop(desktop, "KDE") ||
                     contains_desktop(desktop, "LXQt");
    const std::array<const char*, 2> providers = kde
        ? std::array<const char*, 2>{"kdialog", "zenity"}
        : std::array<const char*, 2>{"zenity", "kdialog"};
    for (const char* provider : providers) {
        std::vector<std::string> args;
        if (std::string_view(provider) == "kdialog") {
            args = {"kdialog", "--title", title_for(kind), "--getopenfilename",
                    directory.string(), std::string(filter.title) + " (" + filter.globs + ")"};
        } else {
            args = {"zenity", "--file-selection", "--title", title_for(kind),
                    "--filename=" + directory.string() + "/",
                    "--file-filter=" + std::string(filter.title) + " | " + filter.globs};
        }
        auto result = run_helper(args);
        if (result.status != NativeDialogStatus::Unavailable) return result;
    }
    return {NativeDialogStatus::Unavailable, {},
            "Install kdialog (KDE Plasma) or zenity (GNOME) for your system file chooser."};
}
#endif
} // namespace

NativeFileDialogResult open_native_file_dialog(
        GuiPickerKind kind, const std::filesystem::path& initial_directory) {
#if defined(_WIN32)
    return windows_dialog(kind, initial_directory);
#else
    return linux_dialog(kind, initial_directory);
#endif
}

} // namespace chromascope
