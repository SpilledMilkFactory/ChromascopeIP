#include "chromascope/Detection.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageFrameCV.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    ~Texture() { reset(); }
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void reset() {
        if (id) glDeleteTextures(1, &id);
        id = 0;
        width = height = 0;
    }

    void upload(const cv::Mat& bgr) {
        reset();
        if (bgr.empty()) return;
        cv::Mat small, rgba;
        const int side = std::max(bgr.rows, bgr.cols);
        if (side > 1800) {
            const double scale = 1800.0 / side;
            cv::resize(bgr, small, {}, scale, scale, cv::INTER_AREA);
        } else small = bgr;
        cv::cvtColor(small, rgba, cv::COLOR_BGR2RGBA);
        width = rgba.cols;
        height = rgba.rows;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
};

struct App {
    std::array<char, 4096> path{};
    std::array<char, 4096> export_path{};
    std::array<char, 4096> browser_directory{};
    chromascope::Settings settings;
    chromascope::ProcessorRegistry processors = chromascope::make_builtin_registry();
    std::string selected_profile = "skittles";
    std::optional<chromascope::DetectionResult> generalized_result;
    cv::Mat input;
    std::optional<chromascope::ImageFrame> input_frame;
    chromascope::Result result;
    Texture original_texture, annotated_texture;
    std::string status = "Open one of the legacy samples or enter an image path.";
    bool show_overlay = true;
    bool show_labels = true;
    bool have_result = false;
    bool browse_requested = false;
    int input_width = 0, input_height = 0;

    App() {
        std::snprintf(path.data(), path.size(), "%s", "samples/skittles/s1.jpg");
        std::snprintf(export_path.data(), export_path.size(), "%s", "annotated.jpg");
        std::snprintf(browser_directory.data(), browser_directory.size(), "%s", ".");
    }

    void analyze() {
        if (!input_frame) return;
        try {
            auto profile = chromascope::skittles_profile_from_settings(settings);
            profile.processor_id = selected_profile;
            auto processed = processors.run(profile, *input_frame);
            result = chromascope::to_legacy_skittles_result(
                processed.detections, processed.elapsed_ms);
            generalized_result = std::move(processed.detections);
            have_result = true;
            refresh_overlay();
            status = "Analysis complete: " + std::to_string(result.objects.size()) + " colored circles.";
        } catch (const std::exception& e) {
            have_result = false;
            generalized_result.reset();
            annotated_texture.reset();
            status = std::string("Analysis failed: ") + e.what();
        }
    }

    void refresh_overlay() {
        if (have_result) {
            annotated_texture.upload(chromascope::draw_annotations(input, result, show_labels));
        }
    }

    void load(const std::filesystem::path& file) {
        try {
            cv::Mat loaded = chromascope::read_image(file);
            auto owned_frame = chromascope::from_cv_mat(loaded, chromascope::PixelFormat::BGR8);
            input = std::move(loaded);
            input_frame = std::move(owned_frame);
            input_width = input.cols;
            input_height = input.rows;
            std::snprintf(path.data(), path.size(), "%s", file.string().c_str());
            original_texture.upload(input);
            have_result = false;
            analyze();
        } catch (const std::exception& e) {
            status = std::string("Open failed: ") + e.what();
        }
    }

    void save() {
        if (!have_result) { status = "Analyze an image before exporting."; return; }
        try {
            chromascope::write_image(export_path.data(),
                chromascope::draw_annotations(input, result, show_labels));
            status = std::string("Saved: ") + export_path.data();
        } catch (const std::exception& e) {
            status = std::string("Export failed: ") + e.what();
        }
    }


    void open_file_picker() {
        const std::filesystem::path selected(path.data());
        const auto dir = selected.has_parent_path() ? selected.parent_path()
                                                    : std::filesystem::path(".");
        std::snprintf(browser_directory.data(), browser_directory.size(), "%s",
                      dir.string().c_str());
        browse_requested = true;
    }

    void draw_file_picker() {
        ImGui::SetNextWindowSize(ImVec2(740, 560), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Select image", nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) return;
        ImGui::TextUnformatted("Choose a photograph (JPG, PNG, BMP, TIFF, WEBP)");
        ImGui::InputText("Directory", browser_directory.data(), browser_directory.size());
        ImGui::SameLine();
        if (ImGui::Button("Up")) {
            const auto dir = std::filesystem::path(browser_directory.data()).parent_path();
            const auto next = dir.empty() ? std::filesystem::path(".") : dir;
            std::snprintf(browser_directory.data(), browser_directory.size(), "%s",
                          next.string().c_str());
        }
        ImGui::Separator();
        const auto dir = std::filesystem::path(browser_directory.data());
        std::error_code error;
        std::filesystem::directory_iterator cursor(dir, error);
        if (error) {
            ImGui::TextWrapped("Directory unavailable: %s", error.message().c_str());
        } else {
            std::vector<std::filesystem::directory_entry> entries;
            for (const auto& entry : cursor) entries.push_back(entry);
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                std::error_code ea, eb;
                const bool ad = a.is_directory(ea), bd = b.is_directory(eb);
                if (ad != bd) return ad > bd;
                return a.path().filename().string() < b.path().filename().string();
            });
            ImGui::BeginChild("Files", ImVec2(0, -48), ImGuiChildFlags_Borders);
            for (const auto& entry : entries) {
                std::error_code stat_error;
                const bool is_dir = entry.is_directory(stat_error);
                if (stat_error) continue;
                const auto filename = entry.path().filename().string();
                if (!is_dir) {
                    auto extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (extension != ".jpg" && extension != ".jpeg" &&
                        extension != ".png" && extension != ".bmp" &&
                        extension != ".tif" && extension != ".tiff" &&
                        extension != ".webp") continue;
                }
                const std::string label = (is_dir ? "[Folder] " : "[Image] ") + filename;
                if (ImGui::Selectable(label.c_str())) {
                    if (is_dir) {
                        const std::string next = entry.path().string();
                        std::snprintf(browser_directory.data(), browser_directory.size(),
                                      "%s", next.c_str());
                    } else {
                        load(entry.path());
                        ImGui::CloseCurrentPopup();
                    }
                    break;
                }
            }
            ImGui::EndChild();
        }
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    void draw_ui() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("ChromascopeIP Workspace", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(ImVec4(0.44f, 0.79f, 0.96f, 1.0f), "ChromascopeIP");
        ImGui::SameLine();
        ImGui::TextDisabled("v0.1.3  |  Still image laboratory  |  OpenCV 5.1.0 / ImGui 1.93.0");
        ImGui::Separator();

        const float sidebar_width = 345.0f;
        ImGui::BeginChild("Controls", ImVec2(sidebar_width, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("INPUT IMAGE");
        ImGui::InputText("##path", path.data(), path.size());
        if (ImGui::Button("Open image", ImVec2(159, 0))) load(path.data());
        ImGui::SameLine();
        if (ImGui::Button("Browse...", ImVec2(-1, 0))) open_file_picker();
        ImGui::TextDisabled("Browse folders or paste an absolute image path.");
        ImGui::Spacing();
        ImGui::TextUnformatted("ORIGINAL SKITTLES SAMPLES");
        for (int n = 1; n <= 4; ++n) {
            const std::string button = "s" + std::to_string(n) + ".jpg";
            if (ImGui::Button(button.c_str(), ImVec2(75, 0))) {
                load(std::filesystem::path(CHROMASCOPE_SAMPLE_DIR) / button);
            }
            if (n != 4) ImGui::SameLine();
        }
        ImGui::Separator();
        ImGui::TextUnformatted("PROCESSING PROFILE");
        const auto descriptors = processors.descriptors();
        const char* selected_name = "Unknown processor";
        for (const auto& descriptor : descriptors)
            if (descriptor.id == selected_profile) selected_name = descriptor.display_name.c_str();
        if (ImGui::BeginCombo("##processor_profile", selected_name)) {
            for (const auto& descriptor : descriptors) {
                const bool selected = descriptor.id == selected_profile;
                if (ImGui::Selectable(descriptor.display_name.c_str(), selected) && !selected) {
                    selected_profile = descriptor.id;
                    analyze();
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Registered processors; additional profiles are planned.");
        ImGui::Spacing();
        float accumulator = static_cast<float>(settings.accumulator_threshold);
        if (ImGui::SliderFloat("Circle threshold", &accumulator, 18.0f, 55.0f, "%.0f"))
            settings.accumulator_threshold = accumulator;
        ImGui::SetItemTooltip("Lower values: more candidates; more false positives.");
        float minimum_confidence = static_cast<float>(settings.minimum_confidence);
        if (ImGui::SliderFloat("Color confidence", &minimum_confidence, 0.0f, 0.95f, "%.2f"))
            settings.minimum_confidence = minimum_confidence;
        int max_size = settings.max_dimension;
        if (ImGui::SliderInt("Analysis max size", &max_size, 512, 2000))
            settings.max_dimension = max_size;
        if (ImGui::Button("Re-run analysis", ImVec2(-1, 0))) analyze();
        ImGui::Separator();

        ImGui::TextUnformatted("VIEW & EXPORT");
        ImGui::Checkbox("Show detected outlines", &show_overlay);
        if (ImGui::Checkbox("Include color labels", &show_labels)) refresh_overlay();
        ImGui::InputText("##export", export_path.data(), export_path.size());
        if (ImGui::Button("Export annotated image", ImVec2(-1, 0))) save();
        ImGui::Separator();
        ImGui::TextUnformatted("RESULTS");
        if (have_result) {
            ImGui::Text("Size: %d x %d", input_width, input_height);
            ImGui::Text("Found: %zu", result.objects.size());
            ImGui::Text("Processing time: %.1f ms", result.elapsed_ms);
            for (std::size_t i = 0; i < chromascope::kColorCount; ++i) {
                const auto color = static_cast<chromascope::CandyColor>(i);
                ImGui::BulletText("%s: %d", chromascope::color_name(color).data(), result.counts[i]);
            }
        } else ImGui::TextDisabled("No analysis yet.");
        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("Preview", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("IMAGE PREVIEW");
        ImGui::SameLine();
        ImGui::TextDisabled(show_overlay ? "Annotated" : "Original");
        ImGui::Separator();
        const Texture& texture = show_overlay && have_result ? annotated_texture : original_texture;
        if (texture.id) {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float factor = std::min(avail.x / static_cast<float>(texture.width),
                                          avail.y / static_cast<float>(texture.height));
            const float scale = std::max(0.01f, factor);
            ImGui::Image(static_cast<ImTextureID>(texture.id),
                ImVec2(texture.width * scale, texture.height * scale));
        } else {
            ImGui::TextWrapped("Open one of the four supplied sample images to begin.");
        }
        ImGui::EndChild();
        if (browse_requested) {
            ImGui::OpenPopup("Select image");
            browse_requested = false;
        }
        draw_file_picker();
        ImGui::End();
    }
};

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1440, 900, "ChromascopeIP", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    {
        App app;
        if (argc > 1) app.load(argv[1]);
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            app.draw_ui();
            ImGui::Render();
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    } // delete image textures while the OpenGL context is alive
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
