#pragma once

// Native, interactive *file-open* dialog for the GUI. This interface deliberately
// has no OpenCV, ImGui, media, model-loading, or network dependencies.
#include "chromascope/GuiExperiencePolicy.hpp"

#include <filesystem>
#include <string>

namespace chromascope {

enum class NativeDialogStatus { Selected, Cancelled, Unavailable, Error };

struct NativeFileDialogResult {
    NativeDialogStatus status = NativeDialogStatus::Unavailable;
    std::filesystem::path path;
    std::string message;
};

// The initial directory is optional. Never persist the returned path as a GUI
// preference. Cancel is not a failure and must not open a second dialog.
[[nodiscard]] NativeFileDialogResult open_native_file_dialog(
    GuiPickerKind kind, const std::filesystem::path& initial_directory = {});

} // namespace chromascope
