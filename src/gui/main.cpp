#include "chromascope/Detection.hpp"
#include "chromascope/AnnotationBridge.hpp"
#include "chromascope/AnnotationView.hpp"
#include "chromascope/AnnotationRendererCV.hpp"
#include "chromascope/DetectionResultBridge.hpp"
#include "chromascope/SkittlesProcessor.hpp"
#include "chromascope/ProcessingQueue.hpp"
#include "chromascope/ImageIO.hpp"
#include "chromascope/ImageFrameCV.hpp"
#include "chromascope/ImageViewport.hpp"
#include "chromascope/VideoPlayer.hpp"
#include "chromascope/DnnGuiSession.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <system_error>
#include <vector>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <map>
#include <cstdint>
#include <optional>
#include <variant>
#include <stdexcept>
#include <string>

namespace {

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int source_width = 0;
    int source_height = 0;
    ~Texture() { reset(); }
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void reset() {
        if (id) glDeleteTextures(1, &id);
        id = 0;
        width = height = 0;
        source_width = source_height = 0;
    }

    void upload(const cv::Mat& bgr) {
        reset();
        if (bgr.empty()) return;
        source_width = bgr.cols;
        source_height = bgr.rows;
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
    std::array<char, 4096> video_path{};
    std::array<char, 4096> detector_model_path{};
    std::array<char, 4096> browser_directory{};
    chromascope::Settings settings;
    chromascope::ProcessorRegistry processors = chromascope::make_builtin_registry();
    // Declared after processors so it joins before registry destruction.
    std::unique_ptr<chromascope::ProcessingQueue> processing;
    std::uint64_t pending_ticket = 0;
    std::string selected_profile = "skittles";
    std::map<std::string, chromascope::ProcessorProfile, std::less<>> filter_profiles;
    cv::Mat processed_image; // Presentation-only BGR8; GUI thread owns its pixels.
    std::optional<chromascope::DetectionResult> generalized_result;
    cv::Mat input;
    std::shared_ptr<const chromascope::ImageFrame> input_frame;
    std::optional<chromascope::AnnotationLayer> annotation_layer;
    std::array<int, chromascope::kColorCount> color_counts{};
    double analysis_elapsed_ms = 0.0;
    chromascope::VideoPlayer video_player;
    Texture video_texture;
    chromascope::ImageViewport video_view;
    std::uint64_t video_seek_target = 0;
    bool video_scrubbing = false;
    int workspace_mode = 0; // 0 = still image; 1 = recorded video
    int detector_profile = 0; // 0 = disabled; 1 = NanoDet raw CPU diagnostics
    chromascope::DnnGuiSession detector_session;
    chromascope::DnnGuiOptions detector_options;
    bool picker_video = false;
    bool picker_model = false;
    Texture original_texture, processed_texture;
    Texture source_overlay_texture, processed_overlay_texture;
    chromascope::AnnotationViewOptions overlay_options;
    int preview_mode = 0; // Result, Original, Side by side
    // View state is independent for the main image and each comparison pane.
    // All math uses native-image pixels, even though OpenGL textures may be
    // display-downsampled to a maximum side of 1800.
    chromascope::ImageViewport result_view, comparison_original_view,
        comparison_processed_view;
    int export_base_mode = 0; // As configured, Original, Processed
    bool synchronize_comparison = true;
    std::string status = "Open one of the legacy samples or enter an image path.";
    bool show_overlay = true;
    bool show_labels = true;
    bool show_mask_preview = false;
    bool have_result = false;
    bool browse_requested = false;
    int input_width = 0, input_height = 0;

    App() {
        processing = std::make_unique<chromascope::ProcessingQueue>(
            [this](const chromascope::ProcessorProfile& profile,
                   const chromascope::ImageFrame& frame, std::stop_token token) {
                return processors.run(profile, frame, token);
            });
        std::snprintf(path.data(), path.size(), "%s", "samples/skittles/s1.jpg");
        std::snprintf(export_path.data(), export_path.size(), "%s", "annotated.jpg");
        std::snprintf(video_path.data(), video_path.size(), "%s", "recording.avi");
        std::snprintf(detector_model_path.data(), detector_model_path.size(), "%s",
                      "models/object_detection_nanodet_2022nov.onnx");
        std::snprintf(browser_directory.data(), browser_directory.size(), "%s", ".");
    }

    void analyze() {
        if (!input_frame) return;
        try {
            auto profile = selected_profile == "skittles"
                ? chromascope::skittles_profile_from_settings(settings)
                : filter_profiles.at(selected_profile);
            // At most one pending + one in-flight request: repeated changes
            // supersede stale work without creating unbounded threads or frames.
            pending_ticket = processing->submit(std::move(profile), input_frame);
            have_result = false;
            generalized_result.reset();
            annotation_layer.reset();
            processed_texture.reset();
            source_overlay_texture.reset();
            processed_overlay_texture.reset();
            processed_image.release();
            overlay_options.selected_index.reset();
            overlay_options.selected_only = false;
            status = "Analyzing in background...";
        } catch (const std::exception& e) {
            pending_ticket = 0;
            have_result = false;
            generalized_result.reset();
            annotation_layer.reset();
            processed_texture.reset();
            source_overlay_texture.reset();
            processed_overlay_texture.reset();
            status = std::string("Analysis failed: ") + e.what();
        }
    }

    void update() {
        if (video_player.update()) {
            const auto& frame = video_player.current_frame();
            if (frame) {
                const cv::Mat decoded = chromascope::to_cv_mat_copy(frame->image);
                const bool changed_size = video_texture.source_width != decoded.cols ||
                                          video_texture.source_height != decoded.rows;
                video_texture.upload(decoded); // OpenGL always on the GUI thread.
                if (changed_size) video_view.fit();
                if (!video_scrubbing) video_seek_target = frame->index;
            }
        }
        auto completion = processing->poll();
        if (!completion || completion->ticket != pending_ticket) return;
        pending_ticket = 0;
        try {
            if (!completion->error.empty())
                throw std::runtime_error(completion->error);
            if (!completion->processed)
                throw std::runtime_error("Processor returned no result");
            auto& processed = *completion->processed;
            // Keep the processor result immutable and generate presentation
            // data once; the renderer never inspects processor classes/scores.
            auto next_layer = chromascope::make_annotation_layer(processed.detections);
            color_counts.fill(0);
            if (processed.detections.processor_id() == "skittles") {
                for (const auto& detection : processed.detections.objects()) {
                    if (detection.class_id && *detection.class_id >= 0 &&
                        *detection.class_id < static_cast<int>(chromascope::kColorCount))
                        ++color_counts[static_cast<std::size_t>(*detection.class_id)];
                }
            }
            analysis_elapsed_ms = processed.elapsed_ms;
            annotation_layer = std::move(next_layer);
            generalized_result = std::move(processed.detections);
            processed_image = processed.output_frame
                ? chromascope::to_cv_mat_copy(*processed.output_frame) : input;
            // OpenGL uploads are done on the GUI thread, never on the worker.
            processed_texture.upload(processed_image);
            have_result = true;
            refresh_overlay();
            status = generalized_result->processor_id() == "skittles"
                ? "Analysis complete: " + std::to_string(generalized_result->objects().size()) + " objects."
                : (is_geometric_profile()
                    ? "Found " + std::to_string(generalized_result->objects().size()) +
                      " geometric regions (no semantic classification)."
                    : "Image processing complete: " + generalized_result->processor_id() + ".");
        } catch (const std::exception& e) {
            have_result = false;
            generalized_result.reset();
            annotation_layer.reset();
            processed_texture.reset();
            source_overlay_texture.reset();
            processed_overlay_texture.reset();
            status = std::string("Analysis failed: ") + e.what();
        }
    }

    [[nodiscard]] bool is_geometric_profile() const {
        return selected_profile == "hsv_segmentation" ||
               selected_profile == "contour_detection";
    }

    void refresh_overlay() {
        // These are GPU presentation caches, not processor outputs. A filtered
        // view never mutates detection data or the authoritative annotation.
        source_overlay_texture.reset();
        processed_overlay_texture.reset();
        if (!have_result || !annotation_layer || !show_overlay || preview_mode == 1) return;
        const auto view = chromascope::make_annotation_view(*annotation_layer, overlay_options);
        if (view.items().empty()) return;
        const chromascope::AnnotationRenderOptions draw{
            .show_outlines = true, .show_labels = show_labels};
        // Only rasterize the visible target. A full-resolution overlay can
        // occupy tens of MiB: never allocate two copies unnecessarily.
        const bool source_result = is_geometric_profile() && !show_mask_preview;
        if (preview_mode == 0 && source_result)
            source_overlay_texture.upload(chromascope::render_annotations(input, view, draw));
        else
            processed_overlay_texture.upload(chromascope::render_annotations(processed_image, view, draw));
    }

    [[nodiscard]] const Texture& result_preview_texture() const {
        // Preserve the familiar M3.2 presentation: masks default to an
        // annotation over the photograph, unless explicitly requested.
        const bool use_source = is_geometric_profile() && !show_mask_preview;
        if (use_source) return show_overlay && source_overlay_texture.id
            ? source_overlay_texture : original_texture;
        return show_overlay && processed_overlay_texture.id
            ? processed_overlay_texture : processed_texture;
    }

    [[nodiscard]] const Texture& comparison_processed_texture() const {
        // A comparison must show actual output pixels, including HSV masks.
        return show_overlay && processed_overlay_texture.id
            ? processed_overlay_texture : processed_texture;
    }

    bool draw_viewport_texture(const char* widget, const Texture& texture,
                               chromascope::ImageViewport& view) {
        if (!texture.id || texture.source_width <= 0 || texture.source_height <= 0) {
            ImGui::TextDisabled("No image available.");
            return false;
        }
        const ImVec2 area = ImGui::GetContentRegionAvail();
        const ImVec2 size{std::max(1.0f, area.x), std::max(1.0f, area.y)};
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        if (!view.configure(static_cast<double>(texture.source_width),
                            static_cast<double>(texture.source_height),
                            static_cast<double>(size.x), static_cast<double>(size.y))) {
            ImGui::TextDisabled("Invalid image viewport.");
            return false;
        }
        // The widget covers the viewport, rather than the scaled image. Mouse
        // capture remains correct while a zoomed image extends beyond its area.
        ImGui::InvisibleButton(widget, size);
        const bool hovered = ImGui::IsItemHovered();
        bool changed = false;
        const auto& io = ImGui::GetIO();
        if (hovered) {
            const chromascope::Point2D anchor{
                static_cast<double>(io.MousePos.x - origin.x),
                static_cast<double>(io.MousePos.y - origin.y)};
            if (io.MouseWheel != 0.0f) {
                view.zoom_at(std::pow(1.2, static_cast<double>(io.MouseWheel)), anchor);
                changed = true;
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
                ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
                view.pan_by(io.MouseDelta.x, io.MouseDelta.y);
                changed = true;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                view.fit();
                changed = true;
            }
        }
        const auto bounds = view.image_rect();
        const ImVec2 top_left{
            origin.x + static_cast<float>(bounds.minimum.x),
            origin.y + static_cast<float>(bounds.minimum.y)};
        const ImVec2 bottom_right{
            origin.x + static_cast<float>(bounds.maximum.x),
            origin.y + static_cast<float>(bounds.maximum.y)};
        const ImVec2 clip_max{origin.x + size.x, origin.y + size.y};
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, clip_max, true);
        draw->AddRectFilled(origin, clip_max, IM_COL32(17, 19, 24, 255));
        // Draw the complete texture using its source-image aspect ratio. The
        // identical transform applies to pre-rasterized source-pixel geometry.
        draw->AddImage(static_cast<ImTextureID>(texture.id), top_left, bottom_right);
        draw->PopClipRect();
        if (hovered) {
            const auto point = view.canvas_to_image({
                static_cast<double>(io.MousePos.x - origin.x),
                static_cast<double>(io.MousePos.y - origin.y)});
            if (view.contains_image_pixel(point)) {
                ImGui::BeginTooltip();
                ImGui::Text("Source pixel: %d, %d", static_cast<int>(point.x),
                            static_cast<int>(point.y));
                ImGui::Text("Image: %d x %d | Zoom: %.1fx fit",
                            texture.source_width, texture.source_height, view.zoom());
                if (texture.source_width != texture.width ||
                    texture.source_height != texture.height)
                    ImGui::TextDisabled("Preview downsampled to %d x %d",
                                        texture.width, texture.height);
                ImGui::EndTooltip();
            }
        }
        return changed;
    }

    void load(const std::filesystem::path& file) {
        try {
            cv::Mat loaded = chromascope::read_image(file);
            auto owned_frame = chromascope::from_cv_mat(loaded, chromascope::PixelFormat::BGR8);
            input = std::move(loaded);
            input_frame = std::make_shared<const chromascope::ImageFrame>(
                std::move(owned_frame));
            if (detector_profile != 0) detector_session.unload();
            input_width = input.cols;
            input_height = input.rows;
            std::snprintf(path.data(), path.size(), "%s", file.string().c_str());
            original_texture.upload(input);
            result_view.fit();
            comparison_original_view.fit();
            comparison_processed_view.fit();
            processed_texture.reset();
            source_overlay_texture.reset();
            processed_overlay_texture.reset();
            overlay_options.selected_index.reset();
            overlay_options.selected_only = false;
            processed_image.release();
            have_result = false;
            generalized_result.reset();
            annotation_layer.reset();
            analyze();
        } catch (const std::exception& e) {
            status = std::string("Open failed: ") + e.what();
        }
    }

    void save() {
        if (!have_result) { status = "Analyze an image before exporting."; return; }
        try {
            // Export has an explicit source/processed selection. The comparison
            // layout itself is never accidentally baked into the output file.
            const bool use_source = export_base_mode == 1 ||
                (export_base_mode == 0 && is_geometric_profile() && !show_mask_preview);
            const auto visible = chromascope::make_annotation_view(
                *annotation_layer, overlay_options);
            chromascope::write_image(export_path.data(),
                chromascope::render_annotations(
                    use_source ? input : processed_image, visible,
                    {.show_outlines = show_overlay,
                     .show_labels = show_overlay && show_labels}));
            status = std::string("Saved: ") + export_path.data();
        } catch (const std::exception& e) {
            status = std::string("Export failed: ") + e.what();
        }
    }


    void open_video(const std::filesystem::path& file) {
        try {
            video_player.open(file);
            video_texture.reset();
            video_view.fit();
            video_seek_target = 0;
            video_scrubbing = false;
            video_status.clear();
            std::snprintf(video_path.data(), video_path.size(), "%s", file.string().c_str());
            workspace_mode = 1;
        } catch (const std::exception& exception) {
            video_status = std::string("Open failed: ") + exception.what();
        }
    }

    std::string video_status;

    void open_file_picker(bool for_video = false, bool for_model = false) {
        picker_video = for_video;
        picker_model = for_model;
        const std::filesystem::path selected(for_model ? detector_model_path.data() :
            (for_video ? video_path.data() : path.data()));
        const auto dir = selected.has_parent_path() ? selected.parent_path()
                                                    : std::filesystem::path(".");
        std::snprintf(browser_directory.data(), browser_directory.size(), "%s",
                      dir.string().c_str());
        browse_requested = true;
    }

    void draw_file_picker() {
        ImGui::SetNextWindowSize(ImVec2(740, 560), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Select media", nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) return;
        ImGui::TextUnformatted(picker_model ? "Choose a local ONNX model (.onnx)" :
            (picker_video ? "Choose a recorded video (AVI, MP4, MOV, MKV, WEBM, MPEG, WMV)"
                          : "Choose a photograph (JPG, PNG, BMP, TIFF, WEBP)"));
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
                    const bool valid_image = extension == ".jpg" || extension == ".jpeg" ||
                        extension == ".png" || extension == ".bmp" || extension == ".tif" ||
                        extension == ".tiff" || extension == ".webp";
                    const bool valid_video = extension == ".avi" || extension == ".mp4" ||
                        extension == ".mov" || extension == ".mkv" || extension == ".webm" ||
                        extension == ".m4v" || extension == ".mpg" || extension == ".mpeg" ||
                        extension == ".wmv";
                    if (picker_model ? extension != ".onnx" :
                        (picker_video ? !valid_video : !valid_image)) continue;
                }
                const std::string label = (is_dir ? "[Folder] " :
                    (picker_model ? "[ONNX] " : (picker_video ? "[Video] " : "[Image] "))) + filename;
                if (ImGui::Selectable(label.c_str())) {
                    if (is_dir) {
                        const std::string next = entry.path().string();
                        std::snprintf(browser_directory.data(), browser_directory.size(),
                                      "%s", next.c_str());
                    } else {
                        if (picker_model) {
                            detector_session.unload();
                            std::snprintf(detector_model_path.data(), detector_model_path.size(),
                                          "%s", entry.path().string().c_str());
                        } else if (picker_video) open_video(entry.path());
                        else load(entry.path());
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

    void draw_detector_controls() {
        ImGui::Separator();
        ImGui::TextUnformatted("OBJECT DETECTION (M5.5)");
        if (ImGui::Combo("Detector profile", &detector_profile,
                         "Disabled (classical profiles)\0NanoDet COCO-80 / CPU\0")) {
            detector_session.unload();
        }
        if (detector_profile == 0) {
            ImGui::TextDisabled("Classical image processing is unchanged.");
            return;
        }
        ImGui::TextWrapped("NanoDet-Plus-M 1.5x, 416 x 416, 80 classes. "
                           "ONNX weights are not bundled.");
        if (ImGui::InputText("ONNX path", detector_model_path.data(),
                             detector_model_path.size())) {
            // Editing a path invalidates the prior verified identity immediately.
            detector_session.unload();
        }
        if (ImGui::Button("Browse ONNX...", ImVec2(-1, 0)))
            open_file_picker(false, true);
        const auto state = detector_session.snapshot();
        const bool busy = state.stage == chromascope::DnnGuiStage::Checking ||
                          state.stage == chromascope::DnnGuiStage::Running;
        if (ImGui::Button("Verify SHA-256 + load", ImVec2(-1, 0))) {
            try { detector_session.load(detector_model_path.data()); }
            catch (const std::exception& e) { status = std::string("Model load rejected: ") + e.what(); }
        }
        ImGui::TextWrapped("Model status: %s", state.message.empty()
                           ? "Not loaded / unverified" : state.message.c_str());
        ImGui::TextDisabled("Only the M5.1 pinned SHA-256 is trusted. CPU backend only.");
        float confidence = static_cast<float>(detector_options.confidence_threshold);
        if (ImGui::SliderFloat("Confidence cutoff", &confidence, 0.0f, 1.0f, "%.2f"))
            detector_options.confidence_threshold = confidence;
        float iou = static_cast<float>(detector_options.nms_iou_threshold);
        if (ImGui::SliderFloat("NMS IoU cutoff", &iou, 0.0f, 1.0f, "%.2f"))
            detector_options.nms_iou_threshold = iou;
        ImGui::SliderInt("Max detections", &detector_options.max_detections, 1, 512);
        ImGui::TextDisabled("Postprocessing settings apply after tensor decoding is added.");
        const bool ready = detector_session.ready_for(detector_model_path.data());
        ImGui::BeginDisabled(!ready || !input_frame || busy);
        if (ImGui::Button("Run raw CPU inference", ImVec2(-1, 0))) {
            try {
                chromascope::validate_dnn_gui_options(detector_options);
                detector_session.run(input_frame);
            } catch (const std::exception& e) {
                status = std::string("CPU inference rejected: ") + e.what();
            }
        }
        ImGui::EndDisabled();
        if (busy && ImGui::Button("Cancel inference / load", ImVec2(-1, 0)))
            detector_session.cancel();
        if (!busy && ImGui::Button("Unload model", ImVec2(-1, 0)))
            detector_session.unload();
        if (state.stage == chromascope::DnnGuiStage::Completed) {
            ImGui::Text("Raw inference: %.1f ms", state.elapsed_ms);
            for (std::size_t i = 0; i < state.output_shapes.size(); ++i) {
                std::string shape;
                for (int dim : state.output_shapes[i]) {
                    if (!shape.empty()) shape += " x ";
                    shape += std::to_string(dim);
                }
                ImGui::Text("Tensor %zu: [%s]", i + 1, shape.c_str());
            }
        }
        ImGui::TextWrapped("Raw ONNX outputs only. No decoded object boxes, "
                           "classification overlays or detector exports yet.");
    }

    void draw_video_ui() {
        const float sidebar_width = 345.0f;
        ImGui::BeginChild("Video controls", ImVec2(sidebar_width, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("RECORDED VIDEO");
        ImGui::InputText("##video_path", video_path.data(), video_path.size());
        if (ImGui::Button("Open video", ImVec2(159, 0))) open_video(video_path.data());
        ImGui::SameLine();
        if (ImGui::Button("Browse video...", ImVec2(-1, 0))) open_file_picker(true);
        ImGui::TextDisabled("Decoder runs on a bounded background worker.");
        ImGui::Separator();

        const bool has_source = video_player.has_source();
        ImGui::BeginDisabled(!has_source);
        if (ImGui::Button(video_player.playing() ? "Pause" : "Play", ImVec2(108, 0))) {
            if (video_player.playing()) video_player.pause();
            else video_player.play();
        }
        ImGui::SameLine();
        if (ImGui::Button("< Frame", ImVec2(99, 0))) {
            try { video_player.step_backward(); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Frame >", ImVec2(99, 0))) {
            try { video_player.step_forward(); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        if (ImGui::Button("Beginning", ImVec2(159, 0))) {
            try { video_player.seek(0); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Close video", ImVec2(-1, 0))) {
            video_player.close();
            video_texture.reset();
            video_view.fit();
        }
        ImGui::Separator();
        const auto& current = video_player.current_frame();
        if (current) ImGui::Text("Frame: %llu", static_cast<unsigned long long>(current->index));
        else ImGui::TextDisabled("No decoded frame yet.");
        const auto& metadata = video_player.stream_info();
        if (metadata && metadata->reported_frame_count &&
            *metadata->reported_frame_count > 0) {
            const std::uint64_t first = 0;
            const std::uint64_t last = *metadata->reported_frame_count - 1;
            if (video_seek_target > last) video_seek_target = last;
            ImGui::SliderScalar("Timeline", ImGuiDataType_U64,
                                &video_seek_target, &first, &last);
            if (ImGui::IsItemActive()) {
                video_scrubbing = true;
                video_player.pause();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                video_scrubbing = false;
                try { video_player.seek(video_seek_target); }
                catch (const std::exception& e) { video_status = e.what(); }
            } else if (ImGui::IsItemDeactivated()) video_scrubbing = false;
            ImGui::TextDisabled("Reported frames: %llu (decoder estimate)",
                static_cast<unsigned long long>(*metadata->reported_frame_count));
        } else {
            ImGui::TextDisabled("Total frame count unavailable: use absolute seek.");
        }
        if (ImGui::InputScalar("Seek index", ImGuiDataType_U64, &video_seek_target,
                               nullptr, nullptr, nullptr, ImGuiInputTextFlags_EnterReturnsTrue)) {
            try { video_player.seek(video_seek_target); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        if (ImGui::Button("Seek to frame", ImVec2(-1, 0))) {
            try { video_player.seek(video_seek_target); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        float playback_rate = static_cast<float>(video_player.playback_fps());
        if (ImGui::SliderFloat("Playback FPS", &playback_rate, 1.0f, 120.0f, "%.1f"))
            video_player.set_playback_fps(static_cast<double>(playback_rate));
        ImGui::TextDisabled("Decoder timing when available; otherwise nominal/30 FPS.");
        ImGui::TextDisabled("Audio ignored: no decoding, playback, or synchronization.");
        if (metadata) {
            if (metadata->rotation.clockwise_degrees)
                ImGui::TextDisabled("Rotation metadata: %d deg clockwise (0 may be unreported)",
                    *metadata->rotation.clockwise_degrees);
            else ImGui::TextDisabled("Rotation metadata: unavailable");
        }
        if (current && current->image.metadata().timestamp) {
            const auto timestamp = current->image.metadata().timestamp->value;
            const double seconds = std::chrono::duration<double>(timestamp).count();
            ImGui::Text("Media time: %.3f s (%s)", seconds,
                current->timestamp_source == chromascope::VideoTimestampSource::DecoderReported
                    ? "decoder reported" : "FPS estimate");
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::TextWrapped("%s", video_player.status().c_str());
        if (!video_status.empty()) ImGui::TextWrapped("%s", video_status.c_str());
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Video preview", ImVec2(0, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextUnformatted("VIDEO PREVIEW");
        ImGui::SameLine();
        if (ImGui::SmallButton("Fit video")) video_view.fit();
        ImGui::SameLine();
        ImGui::TextDisabled("Wheel: zoom | Middle/right drag: pan | Double-click: fit");
        ImGui::Separator();
        if (video_texture.id)
            draw_viewport_texture("##video_canvas", video_texture, video_view);
        else ImGui::TextWrapped("Open a video. The first decoded frame appears here.");
        ImGui::EndChild();
        if (browse_requested) {
            ImGui::OpenPopup("Select media");
            browse_requested = false;
        }
        draw_file_picker();
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
        ImGui::TextDisabled("v0.1.3  |  Image & recorded video  |  OpenCV 5.1.0 / ImGui 1.93.0");
        ImGui::Separator();
        if (ImGui::RadioButton("Still images", workspace_mode == 0)) {
            workspace_mode = 0;
            video_player.pause();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Recorded video", workspace_mode == 1))
            workspace_mode = 1;
        ImGui::Separator();
        if (workspace_mode == 1) {
            draw_video_ui();
            ImGui::End();
            return;
        }

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
                    if (selected_profile != "skittles" &&
                        filter_profiles.find(selected_profile) == filter_profiles.end())
                        filter_profiles.emplace(selected_profile,
                                                processors.default_profile(selected_profile));
                    analyze();
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (selected_profile == "skittles") {
            ImGui::TextDisabled("Legacy colored-circle detector.");
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
        } else {
            for (const auto& descriptor : descriptors) {
                if (descriptor.id != selected_profile) continue;
                ImGui::TextWrapped("%s", descriptor.description.c_str());
                auto& options = filter_profiles.at(selected_profile).numeric_options;
                for (const auto& option : descriptor.options) {
                    const std::string label = option.key;
                    if (option.integral) {
                        int current = static_cast<int>(options.at(option.key));
                        if (ImGui::SliderInt(label.c_str(), &current,
                                             static_cast<int>(option.minimum),
                                             static_cast<int>(option.maximum))) {
                            if ((option.key == "kernel_size" || option.key == "morph_kernel" ||
                                 option.key == "blur_kernel") && current % 2 == 0)
                                current = current < static_cast<int>(options.at(option.key))
                                    ? current - 1 : current + 1;
                            options.at(option.key) = current;
                        }
                    } else {
                        float current = static_cast<float>(options.at(option.key));
                        if (ImGui::SliderFloat(label.c_str(), &current,
                              static_cast<float>(option.minimum),
                              static_cast<float>(option.maximum), "%.1f"))
                            options.at(option.key) = current;
                    }
                }
            }
        }
        if (selected_profile == "hsv_segmentation") {
            ImGui::TextWrapped("HSV selects colors, not objects; backgrounds may match.");
            const auto& hsv_options = filter_profiles.at(selected_profile).numeric_options;
            if (hsv_options.at("min_hue") > hsv_options.at("max_hue"))
                ImGui::TextWrapped("Hue wraps across 179/0: both ends of the hue scale are selected.");
            ImGui::TextWrapped("Increase min_saturation or min_area to reduce noise.");
        }
        draw_detector_controls();
        ImGui::Separator();
        if (ImGui::Button("Re-run analysis", ImVec2(-1, 0))) analyze();
        if (pending_ticket != 0 && ImGui::Button("Cancel analysis", ImVec2(-1, 0))) {
            processing->cancel();
            pending_ticket = 0;
            have_result = false;
            generalized_result.reset();
            annotation_layer.reset();
            processed_texture.reset();
            source_overlay_texture.reset();
            processed_overlay_texture.reset();
            processed_image.release();
            status = "Analysis canceled.";
        }
        ImGui::Separator();

        ImGui::TextUnformatted("VIEW & EXPORT");
        if (ImGui::Combo("Preview mode", &preview_mode,
                         "Result\0Original\0Side by side\0"))
            refresh_overlay();
        if (ImGui::Checkbox("Synchronize comparison navigation", &synchronize_comparison) &&
            synchronize_comparison)
            comparison_processed_view.synchronize_from(comparison_original_view);
        if (ImGui::Checkbox("Show geometry overlays", &show_overlay))
            refresh_overlay();
        if (ImGui::Checkbox("Show labels", &show_labels)) refresh_overlay();
        if (is_geometric_profile()) {
            if (ImGui::Checkbox("Use mask as result background", &show_mask_preview))
                refresh_overlay();
            ImGui::SetItemTooltip("In comparisons, the right pane always shows processed pixels.");
        }
        if (annotation_layer && !annotation_layer->items().empty()) {
            bool changed = false;
            changed |= ImGui::Checkbox("Circles", &overlay_options.circles);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("Boxes", &overlay_options.boxes);
            ImGui::SameLine();
            changed |= ImGui::Checkbox("Contours", &overlay_options.polygons);
            if (ImGui::CollapsingHeader("Select an individual geometry")) {
                if (ImGui::Button("Clear selection")) {
                    overlay_options.selected_index.reset();
                    overlay_options.selected_only = false;
                    changed = true;
                }
                if (overlay_options.selected_index) {
                    changed |= ImGui::Checkbox("Only selected", &overlay_options.selected_only);
                }
                if (ImGui::BeginChild("Geometry items", ImVec2(0, 120),
                                      ImGuiChildFlags_Borders)) {
                    for (std::size_t i = 0; i < annotation_layer->items().size(); ++i) {
                        const auto& item = annotation_layer->items()[i];
                        const char* kind = std::holds_alternative<chromascope::CircleGeometry>(item.geometry)
                            ? "Circle" : std::holds_alternative<chromascope::BoxGeometry>(item.geometry)
                                ? "Box" : "Contour";
                        const std::string name = std::to_string(i + 1) + ": " + kind +
                            (item.text.empty() ? "" : " - " + item.text) + "##geometry" +
                            std::to_string(i);
                        if (ImGui::Selectable(name.c_str(), overlay_options.selected_index == i)) {
                            overlay_options.selected_index = i;
                            changed = true;
                        }
                    }
                }
                ImGui::EndChild();
            }
            if (changed) refresh_overlay();
        }
        ImGui::Combo("Export image", &export_base_mode,
                     "As configured\0Original pixels\0Processed pixels\0");
        ImGui::InputText("##export", export_path.data(), export_path.size());
        if (ImGui::Button("Export annotated image", ImVec2(-1, 0))) save();
        ImGui::Separator();
        ImGui::TextUnformatted("RESULTS");
        if (pending_ticket != 0) {
            ImGui::TextDisabled("Processing in background (latest request only)...");
        }
        if (have_result) {
            ImGui::Text("Size: %d x %d", input_width, input_height);
            ImGui::Text("Found: %zu", generalized_result->objects().size());
            if (is_geometric_profile()) {
                const auto& options = filter_profiles.at(selected_profile).numeric_options;
                const auto limit = static_cast<std::size_t>(options.at("max_objects"));
                if (generalized_result->objects().size() == limit)
                    ImGui::TextWrapped("At the %zu-region display limit; more candidates may exist.", limit);
            }
            ImGui::Text("Processing time: %.1f ms", analysis_elapsed_ms);
            if (generalized_result->processor_id() == "skittles") {
                for (std::size_t i = 0; i < chromascope::kColorCount; ++i) {
                    const auto color = static_cast<chromascope::CandyColor>(i);
                    ImGui::BulletText("%s: %d", chromascope::color_name(color).data(),
                                      color_counts[i]);
                }
            }
        } else ImGui::TextDisabled("No analysis yet.");
        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::EndChild();
        ImGui::SameLine();

        ImGui::BeginChild("Preview", ImVec2(0, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextUnformatted("IMAGE PREVIEW");
        ImGui::SameLine();
        ImGui::TextDisabled(preview_mode == 2 ? "Original | Processed" :
            preview_mode == 1 ? "Original" : "Result");
        ImGui::SameLine();
        if (ImGui::SmallButton("Fit to view")) {
            result_view.fit();
            comparison_original_view.fit();
            comparison_processed_view.fit();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Wheel: zoom | Middle/right drag: pan | Double-click: fit");
        ImGui::Separator();
        if (!original_texture.id) {
            ImGui::TextWrapped("Open one of the four supplied sample images to begin.");
        } else if (preview_mode == 2 && have_result) {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float pane_width = std::max(1.0f,
                (available.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
            ImGui::BeginChild("Original pane", ImVec2(pane_width, 0),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextUnformatted("Original (unmodified)");
            const bool original_changed = draw_viewport_texture(
                "##original_canvas", original_texture, comparison_original_view);
            if (synchronize_comparison && original_changed)
                comparison_processed_view.synchronize_from(comparison_original_view);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("Processed pane", ImVec2(0, 0),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextUnformatted("Processed output");
            const bool processed_changed = draw_viewport_texture(
                "##processed_canvas", comparison_processed_texture(),
                comparison_processed_view);
            if (synchronize_comparison && processed_changed)
                comparison_original_view.synchronize_from(comparison_processed_view);
            ImGui::EndChild();
        } else {
            draw_viewport_texture("##result_canvas", preview_mode == 1 || !have_result
                ? original_texture : result_preview_texture(), result_view);
        }
        ImGui::EndChild();
        if (browse_requested) {
            ImGui::OpenPopup("Select media");
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
            app.update();
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
