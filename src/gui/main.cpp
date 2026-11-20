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
#include "chromascope/AsyncRecordedVideoDetection.hpp"
#include "chromascope/AnnotatedVideoExport.hpp"
#include "chromascope/AsyncCameraCapture.hpp"
#include "chromascope/PublicStreamReader.hpp"
#include "chromascope/LiveCameraRecording.hpp"
#include "chromascope/AsyncLiveCameraInference.hpp"
#include "chromascope/CameraDiscovery.hpp"
#include "chromascope/CameraPreviewPolicy.hpp"
#include "chromascope/LiveOverlayPolicy.hpp"
#include "chromascope/LiveObjectTracker.hpp"
#include "chromascope/LiveGuiControlPolicy.hpp"
#include "chromascope/GuiExperiencePolicy.hpp"
#include "chromascope/GuiPreferences.hpp"
#include "chromascope/GuiPreferenceStorage.hpp"
#include "chromascope/DnnGuiSession.hpp"
#include "chromascope/DnnPostprocessing.hpp"
#include "chromascope/InstallResources.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <opencv2/imgproc.hpp>
#include "chromascope/ReleaseVersion.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
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
#include <string_view>

namespace {

// Windows' legacy GL/gl.h does not declare GL_CLAMP_TO_EDGE (OpenGL 1.2).
// Our GLFW OpenGL 3.3 context supports this parameter; keep the token local
// rather than adding a global GL_* macro or changing the graphics backend.
constexpr GLint kTextureClampToEdge = 0x812F;

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
        // Avoid the Windows RPC macro spelling as a local identifier.
        cv::Mat resized_bgr, rgba;
        const int side = std::max(bgr.rows, bgr.cols);
        if (side > 1800) {
            const double scale = 1800.0 / side;
            cv::resize(bgr, resized_bgr, {}, scale, scale, cv::INTER_AREA);
        } else resized_bgr = bgr;
        cv::cvtColor(resized_bgr, rgba, cv::COLOR_BGR2RGBA);
        width = rgba.cols;
        height = rgba.rows;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kTextureClampToEdge);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kTextureClampToEdge);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
};

struct App {
    std::array<char, 4096> path{};
    std::filesystem::path still_opened_path;
    std::optional<std::filesystem::path> legacy_sample_directory;
    std::array<char, 4096> export_path{};
    std::array<char, 4096> video_path{};
    std::array<char, 4096> video_export_path{};
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
    chromascope::AsyncRecordedVideoDetection video_detection;
    std::filesystem::path video_opened_path;
    bool video_show_detections = true;
    int video_detection_profile = 0; // 0: COCO-80, 1: person class 0 only.
    float video_person_display_threshold = 0.50F;
    bool video_export_audio_ack = false;
    bool video_export_override_fps = false;
    float video_export_fps = 25.0F;
    float video_uncertainty_floor = 0.35F; // M5.8 diagnostic floor, not an unknown class
    std::atomic<bool> video_export_busy{false};
    std::atomic<std::uint64_t> video_export_count{0};
    std::mutex video_export_status_mutex;
    std::string video_export_status = "No video export started.";
    chromascope::VideoPerformanceMetrics video_export_perf; // guarded by status mutex
    chromascope::VideoStageStatistics video_overlay_draw_perf; // GUI-thread only
    Texture video_texture;
    chromascope::ImageViewport video_view;
    std::uint64_t video_seek_target = 0;
    bool video_scrubbing = false;
    // Seek at a bounded cadence while dragging; always commit the final frame.
    std::optional<chromascope::VideoPlayer::Clock::time_point> last_scrub_seek_at;
    std::optional<std::uint64_t> last_scrub_seek_index;
    bool video_use_source_fps = true;
    float video_manual_playback_fps = 30.0F;
    // M8.4: a third, explicitly opt-in camera workspace. Construction and
    // entering this workspace never open a device or request OS permission.
    // Capture thread owns driver I/O; texture uploads stay on the GUI thread.
    chromascope::AsyncCameraCapture camera_capture;
    // A separate, opt-in bounded writer; preview/inference do not own its output.
    chromascope::LiveCameraRecorder live_recorder;
    std::array<char, 4096> live_record_path{}; // Empty = OS Videos folder + timestamp.
    int live_record_format = 0; // AVI/MJPEG; MP4/H.264 or MPEG-4.
    std::string live_record_error;
    chromascope::AsyncLiveCameraInference live_inference;
    std::shared_ptr<const chromascope::LiveInferenceResult> live_displayed;
    chromascope::LiveObjectTracker live_tracker; // GUI-thread only, no media stored
    std::optional<chromascope::LiveTrackFrame> live_track_frame;
    bool live_enable_tracking = false; // default OFF; one session at a time
    bool live_inference_active = false;
    bool live_paused = false; // camera stays active; analyzed preview freezes
    int live_max_inference_fps = 0; // 0 = no additional cap, source clock pacing
    float live_display_confidence_threshold = 0.50F; // presentation-only gate
    int live_detection_profile = 0; // COCO objects / class-0 person only
    float live_confidence_threshold = 0.50F;
    float live_person_display_threshold = 0.50F;
    bool live_show_bounding_boxes = true;
    bool live_show_confidence_labels = true;
    int live_drop_strategy = 0; // 0: Keep newest (default); 1: Keep oldest queued
    int live_min_capture_spacing_ms = 0; // capture-monotonic pacing
    chromascope::CameraBackend camera_backend = chromascope::CameraBackend::PlatformDefault;
    chromascope::CameraDiscoveryReport camera_devices;
    chromascope::CameraActiveFormat camera_active_format;
    Texture camera_texture;
    chromascope::ImageViewport camera_view;
    std::uint64_t camera_generation = 0;
    std::uint64_t camera_frame_index = 0;
    std::chrono::nanoseconds camera_frame_time{0};
    int camera_index = 0;
    bool camera_frame_visible = false;
    std::string camera_status = "Camera is OFF. Select a device and press Start camera.";
    int workspace_mode = 0; // 0 = still image; 1 = recorded video; 2 = camera; 3 = HTTPS stream
    std::array<char, 2049> public_stream_url{}; // In-memory only; NEVER saved to preferences.
    std::array<char, 4097> public_stream_playlist_file{}; // Optional local master; never persisted.
    std::shared_ptr<std::atomic<bool>> public_stream_cancel;
    int public_stream_kind = 0; // Auto, image, unencrypted HLS, bounded MP4.
    int detector_profile = 0; // 0 = disabled; 1 = verified NanoDet raw/person detection
    chromascope::DnnGuiSession detector_session;
    chromascope::DnnGuiOptions detector_options;
    float still_person_display_threshold = 0.50F;
    // Per-class video settings are shared; still-image person uses class 0 only.
    int detector_selected_class = 0;
    std::array<std::optional<float>, chromascope::kNanoDetClassCount> detector_class_overrides{};
    bool picker_video = false;
    bool picker_stream_playlist = false;
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
    std::string status = "Browse for a local image to begin.";
    bool show_overlay = true;
    bool show_labels = true;
    bool show_mask_preview = false;
    bool have_result = false;
    bool browse_requested = false;
    // M10.1: presentation-only settings. No imgui.ini or other UI state file.
    float ui_content_scale = 1.0F;
    float sidebar_width_logical = 345.0F;
    bool help_requested = false;
    bool preferences_requested = false;
    bool export_requested = false;
    std::string preferences_status = "Preferences are not saved automatically.";
    // The last explicitly saved/loaded profile is the reset baseline. Values
    // not represented in schema v1 retain their original built-in defaults.
    // This snapshot never contains media/model paths or camera session state.
    std::optional<chromascope::GuiPreferences> active_preference_defaults;
    bool browser_show_hidden = false;
    std::filesystem::path browser_selected;
    std::string browser_status_message;
    // Last field: on App destruction jthread requests stop and joins before
    // callback-accessed status, mutex, and atomics are destroyed.
    std::jthread video_export_worker;
    int input_width = 0, input_height = 0;

    App() {
        processing = std::make_unique<chromascope::ProcessingQueue>(
            [this](const chromascope::ProcessorProfile& profile,
                   const chromascope::ImageFrame& frame, std::stop_token token) {
                return processors.run(profile, frame, token);
            });
        std::error_code cwd_error;
        const auto cwd = std::filesystem::current_path(cwd_error);
        legacy_sample_directory = chromascope::find_legacy_sample_directory(
            chromascope::running_executable_path(), cwd_error ? std::filesystem::path{} : cwd);
        if (legacy_sample_directory) {
            const auto sample = (*legacy_sample_directory / "s1.jpg").string();
            std::snprintf(path.data(), path.size(), "%s", sample.c_str());
        }
        std::snprintf(export_path.data(), export_path.size(), "%s", "annotated.jpg");
        std::snprintf(video_path.data(), video_path.size(), "%s", "recording.avi");
        std::snprintf(video_export_path.data(), video_export_path.size(), "%s", "annotated-video.avi");
        std::snprintf(detector_model_path.data(), detector_model_path.size(), "%s",
                      "models/object_detection_nanodet_2022nov.onnx");
        std::snprintf(browser_directory.data(), browser_directory.size(), "%s", ".");
    }

    // M10.2: only explicitly selected, non-sensitive values may be saved.
    // Never persist open file paths, model paths, camera selections, loaded
    // media, active camera/inference states, or tracking history.
    [[nodiscard]] chromascope::GuiPreferences collect_preferences() const {
        auto prefs = chromascope::default_gui_preferences();
        prefs.selected_profile = selected_profile;
        auto& ui = prefs.ui;
        ui["workspace"] = workspace_mode;
        ui["preview"] = preview_mode;
        ui["sidebar_width"] = sidebar_width_logical;
        ui["show_overlay"] = show_overlay;
        ui["show_labels"] = show_labels;
        ui["synchronize_comparison"] = synchronize_comparison;
        ui["show_mask_preview"] = show_mask_preview;
        ui["video_show_detections"] = video_show_detections;
        ui["video_detection_profile"] = video_detection_profile;
        ui["video_person_display_threshold"] = video_person_display_threshold;
        ui["live_detection_profile"] = live_detection_profile;
        ui["live_confidence_threshold"] = live_confidence_threshold;
        ui["live_person_display_threshold"] = live_person_display_threshold;
        ui["live_show_bounding_boxes"] = live_show_bounding_boxes;
        ui["live_show_confidence_labels"] = live_show_confidence_labels;
        ui["live_drop_strategy"] = live_drop_strategy;
        ui["live_min_capture_spacing_ms"] = live_min_capture_spacing_ms;
        ui["live_max_inference_fps"] = live_max_inference_fps;
        ui["live_display_confidence_threshold"] = live_display_confidence_threshold;
        prefs.profiles["skittles"] =
            chromascope::skittles_profile_from_settings(settings).numeric_options;
        for (const auto& [id, profile] : filter_profiles)
            prefs.profiles[id] = profile.numeric_options;
        return prefs;
    }

    void save_gui_preferences() {
        try {
            const auto file = chromascope::gui_preferences_path();
            if (!file) throw std::runtime_error("No valid OS configuration directory");
            const auto prefs = collect_preferences();
            chromascope::validate_gui_preferences(prefs);
            (void)processors.default_profile(prefs.selected_profile);
            for (const auto& [id, options] : prefs.profiles)
                (void)processors.create({id, options});
            chromascope::write_gui_preferences_file(
                *file, chromascope::serialize_gui_preferences(prefs));
            active_preference_defaults = prefs;
            preferences_status = "Preferences explicitly saved. No media/model paths stored.";
        } catch (const std::exception& error) {
            preferences_status = std::string("Preferences not saved: ") + error.what();
        }
    }

    void load_gui_preferences() {
        try {
            const auto file = chromascope::gui_preferences_path();
            if (!file) throw std::runtime_error("No valid OS configuration directory");
            const auto input = chromascope::read_gui_preferences_file(*file);
            if (!input) {
                active_preference_defaults.reset();
                preferences_status = "No saved preferences. Using defaults.";
                return;
            }
            const auto prefs = chromascope::parse_gui_preferences(*input);
            // Validate *everything* before altering a running GUI. Registered
            // processors enforce option bounds, relationships and unknown keys.
            (void)processors.default_profile(prefs.selected_profile);
            std::map<std::string, chromascope::ProcessorProfile, std::less<>> checked;
            for (const auto& [id, values] : prefs.profiles) {
                auto profile = processors.default_profile(id);
                for (const auto& [key, value] : values)
                    profile.numeric_options[key] = value;
                (void)processors.create(profile);
                checked.emplace(id, std::move(profile));
            }
            auto next_settings = settings;
            if (const auto it = checked.find("skittles"); it != checked.end()) {
                const auto& values = it->second.numeric_options;
                next_settings.max_dimension = static_cast<int>(values.at("max_dimension"));
                next_settings.accumulator_threshold = values.at("accumulator_threshold");
                next_settings.edge_threshold = values.at("edge_threshold");
                next_settings.min_distance_fraction = values.at("min_distance_fraction");
                next_settings.min_radius_fraction = values.at("min_radius_fraction");
                next_settings.max_radius_fraction = values.at("max_radius_fraction");
                next_settings.minimum_confidence = values.at("minimum_confidence");
                chromascope::validate_settings(next_settings);
            }
            // Commit validated values only. No side-effectful Start/Open call.
            settings = next_settings;
            selected_profile = prefs.selected_profile;
            checked.erase("skittles");
            filter_profiles = std::move(checked);
            if (selected_profile != "skittles" && !filter_profiles.contains(selected_profile))
                filter_profiles.emplace(selected_profile, processors.default_profile(selected_profile));
            const auto& ui = prefs.ui;
            workspace_mode = static_cast<int>(ui.at("workspace"));
            preview_mode = static_cast<int>(ui.at("preview"));
            sidebar_width_logical = static_cast<float>(ui.at("sidebar_width"));
            show_overlay = ui.at("show_overlay") != 0;
            show_labels = ui.at("show_labels") != 0;
            synchronize_comparison = ui.at("synchronize_comparison") != 0;
            show_mask_preview = ui.at("show_mask_preview") != 0;
            video_show_detections = ui.at("video_show_detections") != 0;
            video_detection_profile = static_cast<int>(ui.at("video_detection_profile"));
            video_person_display_threshold = static_cast<float>(ui.at("video_person_display_threshold"));
            live_detection_profile = static_cast<int>(ui.at("live_detection_profile"));
            live_confidence_threshold = static_cast<float>(ui.at("live_confidence_threshold"));
            live_person_display_threshold = static_cast<float>(ui.at("live_person_display_threshold"));
            live_show_bounding_boxes = ui.at("live_show_bounding_boxes") != 0;
            live_show_confidence_labels = ui.at("live_show_confidence_labels") != 0;
            live_drop_strategy = static_cast<int>(ui.at("live_drop_strategy"));
            live_min_capture_spacing_ms = static_cast<int>(ui.at("live_min_capture_spacing_ms"));
            live_max_inference_fps = static_cast<int>(ui.at("live_max_inference_fps"));
            live_display_confidence_threshold = static_cast<float>(ui.at("live_display_confidence_threshold"));
            // Do not restore runtime opt-ins or session state.
            live_enable_tracking = false;
            live_inference_active = false;
            live_paused = false;
            active_preference_defaults = prefs;
            preferences_status = "Validated schema v1 preferences loaded (no hardware activated).";
        } catch (const std::exception& error) {
            preferences_status = std::string("Preferences rejected; current values retained: ") + error.what();
        }
    }

    void delete_saved_gui_preferences() {
        try {
            const auto file = chromascope::gui_preferences_path();
            if (!file) throw std::runtime_error("No valid OS configuration directory");
            chromascope::validate_preferences_path(*file);
            std::error_code error;
            const bool removed = std::filesystem::remove(*file, error);
            if (error) throw std::runtime_error("Could not remove preferences file");
            preferences_status = removed ? "Saved preferences deleted; current values unchanged." :
                                           "No saved preferences to delete.";
        } catch (const std::exception& error) {
            preferences_status = std::string("Preferences deletion failed: ") + error.what();
        }
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

    [[nodiscard]] bool live_workspace() const noexcept {
        return workspace_mode == 2 || workspace_mode == 3;
    }

    void cancel_live_inference() noexcept {
        live_inference.cancel();
        live_inference_active = false;
        live_paused = false;
        live_displayed.reset();
        live_track_frame.reset();
        live_tracker.reset(); // IDs never cross camera/inference/session boundaries
    }

    void stop_camera() noexcept {
        if (public_stream_cancel) public_stream_cancel->store(true);
        live_recorder.stop();
        cancel_live_inference();
        // stop() returns immediately even if native driver I/O is blocked;
        // STOPPING remains visible until the worker actually closes the camera.
        if (camera_capture.state() != chromascope::CameraCaptureState::Idle)
            camera_capture.stop();
        camera_generation = camera_capture.generation();
        camera_texture.reset();
        camera_view.fit();
        camera_frame_visible = false;
        camera_frame_index = 0;
        camera_frame_time = std::chrono::nanoseconds{0};
        camera_active_format = {};
        camera_status = "Stop requested; device release may still be in progress.";
    }

    void switch_workspace(int next_mode) {
        if (workspace_mode == next_mode) return;
        if (workspace_mode == 2) stop_camera(); // privacy: leaving stops capture
        if (workspace_mode == 3) stop_camera(); // leaving network mode stops remote requests
        if (next_mode != 1) video_player.pause();
        workspace_mode = next_mode;
    }

    void start_live_inference() {
        try {
            if (!live_workspace() ||
                camera_capture.state() != chromascope::CameraCaptureState::Capturing)
                throw std::invalid_argument("Start the live source before enabling inference");
            chromascope::DnnPostprocessOptions policy;
            policy.confidence_threshold = static_cast<double>(live_confidence_threshold);
            policy.nms_iou_threshold = detector_options.nms_iou_threshold;
            policy.max_detections = static_cast<std::size_t>(detector_options.max_detections);
            policy.uncertainty_floor = static_cast<double>(video_uncertainty_floor);
            policy.person_only = live_detection_profile == 1;
            policy.person_display_threshold = static_cast<double>(live_person_display_threshold);
            for (int i = 0; i < chromascope::kNanoDetClassCount; ++i)
                if (const auto value = detector_class_overrides[static_cast<std::size_t>(i)])
                    policy.class_thresholds.push_back({i, *value});
            chromascope::LiveInferenceScheduling scheduling;
            scheduling.drop_strategy = live_drop_strategy == 0
                ? chromascope::LiveInferenceDropStrategy::KeepNewest
                : chromascope::LiveInferenceDropStrategy::KeepOldest;
            scheduling.minimum_capture_spacing =
                std::chrono::milliseconds(chromascope::live_inference_spacing_ms(
                    live_max_inference_fps, live_min_capture_spacing_ms));
            const auto session = live_inference.start(camera_generation, detector_model_path.data(),
                                                      policy, scheduling);
            live_tracker.reset();
            live_track_frame.reset();
            if (live_enable_tracking) live_tracker.begin(camera_generation, session);
            live_displayed.reset();
            live_inference_active = true;
            live_paused = false;
            camera_status = "Live camera analysis requested (local, opt-in).";
        } catch (const std::exception& error) {
            camera_status = std::string("Live analysis rejected: ") + error.what();
        }
    }

    void toggle_live_pause() {
        if (!live_inference_active || !live_workspace()) return;
        const bool next = !live_paused;
        if (!live_inference.set_paused(next)) {
            camera_status = "Live inference is not available for pausing.";
            return;
        }
        live_paused = next;
        if (!next) {
            // Don't associate temporally distant boxes across a paused gap.
            live_track_frame.reset();
            live_tracker.reset();
            if (live_enable_tracking)
                live_tracker.begin(camera_generation, live_inference.snapshot().session);
        }
        camera_status = next ? "Inference paused (camera still active; preview frozen)." :
                               "Inference resumed (awaiting newly analyzed frames).";
    }

    void start_camera() {
        if (public_stream_cancel) public_stream_cancel->store(true);
        live_recorder.stop();
        cancel_live_inference();
        // Only called by the explicit user-facing Start camera button.
        // No probes, fallback, recording, inference or model loading.
        try {
            camera_texture.reset();
            camera_view.fit();
            camera_frame_visible = false;
            camera_frame_index = 0;
            camera_frame_time = std::chrono::nanoseconds{0};
            camera_active_format = {};
            camera_generation = camera_capture.start(camera_index, camera_backend);
            camera_status = "Camera opening requested (OS permission may be required).";
        } catch (const std::exception& error) {
            camera_status = std::string("Camera start rejected: ") + error.what();
        }
    }

    void start_public_stream() {
        live_recorder.stop();
        cancel_live_inference();
        try {
            public_stream_cancel = std::make_shared<std::atomic<bool>>(false);
            auto reader = chromascope::make_public_stream_reader(
                public_stream_url.data(), public_stream_cancel,
                static_cast<chromascope::PublicStreamMediaKind>(public_stream_kind),
                public_stream_playlist_file.data());
            camera_texture.reset();
            camera_view.fit();
            camera_frame_visible = false;
            camera_frame_index = 0;
            camera_frame_time = std::chrono::nanoseconds{0};
            camera_active_format = {};
            // The reader is only called on the existing bounded capture worker.
            camera_generation = camera_capture.start_with_reader(std::move(reader));
            camera_status = "Public HTTPS stream start requested. No local camera opened.";
        } catch (const std::exception& error) {
            camera_status = std::string("Stream rejected: ") + error.what();
        }
    }

    [[nodiscard]] chromascope::LiveRecordingPresentation live_record_presentation() const {
        chromascope::LiveRecordingPresentation presentation;
        if (!live_inference_active || !live_displayed || !camera_frame_visible ||
            !live_workspace()) return presentation;
        const auto snap = live_inference.snapshot();
        if (snap.stage != chromascope::LiveInferenceStage::Running ||
            snap.camera_generation != camera_generation ||
            snap.session != live_displayed->session ||
            live_displayed->camera_generation != camera_generation) return presentation;
        const auto& analyzed = live_displayed->frame;
        const auto stamp = analyzed.image.metadata().timestamp;
        if (!stamp || stamp->domain != chromascope::TimestampDomain::CaptureMonotonic ||
            camera_texture.source_width != analyzed.image.width() ||
            camera_texture.source_height != analyzed.image.height() ||
            !chromascope::live_overlay_matches(
                {camera_generation, snap.session, camera_frame_index,
                 camera_texture.source_width, camera_texture.source_height, camera_frame_time},
                {live_displayed->camera_generation, live_displayed->session,
                 analyzed.index, analyzed.image.width(), analyzed.image.height(), stamp->value}))
            return presentation;
        presentation.displayed = live_displayed;
        presentation.visible = {camera_generation, snap.session, camera_frame_index,
            camera_texture.source_width, camera_texture.source_height, camera_frame_time};
        // Matches the on-screen GUI's user-controlled visibility exactly.
        const auto detected_at = live_displayed->inference.detections.timestamp();
        presentation.show_boxes = live_show_bounding_boxes && detected_at &&
            detected_at->domain == stamp->domain && detected_at->value == stamp->value &&
            live_displayed->inference.detections.width() == analyzed.image.width() &&
            live_displayed->inference.detections.height() == analyzed.image.height();
        presentation.show_labels = presentation.show_boxes && live_show_confidence_labels;
        presentation.minimum_score = static_cast<double>(live_display_confidence_threshold);
        if (presentation.show_boxes && live_enable_tracking && live_track_frame &&
            chromascope::live_overlay_matches(presentation.visible, live_track_frame->key))
            presentation.track_ids = live_track_frame->detection_ids;
        return presentation;
    }

    void update_camera() {
        // Drain the bounded event queue and upload only the newest frame.
        // Never perform driver operations or GL operations on the worker.
        std::optional<chromascope::CameraFramePacket> newest;
        for (unsigned int i = 0; i < 32; ++i) {
            auto event = camera_capture.poll();
            if (!event) break;
            if (event->generation != camera_generation) continue;
            switch (event->kind) {
                case chromascope::CameraCaptureEventKind::Opened:
                    if (event->format) camera_active_format = *event->format;
                    camera_status = "Camera opened. Live preview is active.";
                    break;
                case chromascope::CameraCaptureEventKind::Frame:
                    if (chromascope::camera_preview_can_publish(
                            *event, camera_generation, camera_capture.state()) &&
                        live_workspace()) {
                        // Recording samples the CURRENT displayed inference frame
                        // (with matching boxes) when enabled, not stale boxes
                        // projected onto this newer raw camera capture.
                        // The incoming capture timestamp still drives file FPS.
                        if (live_recorder.accepting() && event->frame)
                            live_recorder.submit(*event->frame, live_record_presentation());
                        newest = std::move(event->frame);
                    }
                    break;
                case chromascope::CameraCaptureEventKind::Stopped:
                    live_recorder.stop();
                    cancel_live_inference();
                    camera_status = "Camera closed. No capture active.";
                    camera_texture.reset();
                    camera_frame_visible = false;
                    break;
                case chromascope::CameraCaptureEventKind::Error:
                    live_recorder.stop();
                    cancel_live_inference();
                    camera_status = "Camera error (" + std::string(
                        chromascope::camera_error_name(event->error_code.value_or(
                            chromascope::CameraErrorCode::ReadFailed))) + "): " + event->message;
                    camera_texture.reset();
                    camera_frame_visible = false;
                    break;
            }
        }
        // A stop/restart may race an already-popped frame. Recheck the
        // generation and state before publishing its pixels to the screen.
        if (newest && live_workspace() &&
            camera_capture.generation() == camera_generation &&
            camera_capture.state() == chromascope::CameraCaptureState::Capturing) {
            try {
                // Avoid a full-size CPU pixel copy on each GUI tick when the
                // result view is already showing exact analyzed pixels.
                // The inference worker owns the submitted frame pixels.
                if (!live_paused && (!live_inference_active || !live_displayed)) {
                    const auto image = chromascope::to_cv_mat_copy(newest->image);
                    const bool resized = camera_texture.source_width != image.cols ||
                                         camera_texture.source_height != image.rows;
                    camera_texture.upload(image);
                    if (resized) camera_view.fit();
                    camera_frame_index = newest->index;
                    camera_frame_time = newest->image.metadata().timestamp->value;
                    camera_frame_visible = true;
                }
                if (live_inference_active && !live_paused)
                    (void)live_inference.submit(camera_generation, std::move(*newest));
            } catch (const std::exception& error) {
                stop_camera();
                camera_status = std::string("Preview conversion failed: ") + error.what();
            }
        }
        if (live_inference_active && !live_paused && live_workspace() &&
            camera_capture.state() == chromascope::CameraCaptureState::Capturing) {
            const auto snapshot = live_inference.snapshot();
            if (snapshot.latest && snapshot.camera_generation == camera_generation &&
                snapshot.latest->camera_generation == camera_generation &&
                snapshot.latest->session == snapshot.session &&
                snapshot.latest != live_displayed) {
                try {
                    const auto image = chromascope::to_cv_mat_copy(snapshot.latest->frame.image);
                    const bool resized = camera_texture.source_width != image.cols ||
                                         camera_texture.source_height != image.rows;
                    camera_texture.upload(image); // GUI thread only
                    if (resized) camera_view.fit();
                    camera_frame_index = snapshot.latest->frame.index;
                    camera_frame_time = snapshot.latest->frame.image.metadata().timestamp->value;
                    camera_frame_visible = true;
                    live_track_frame.reset();
                    if (live_enable_tracking) {
                        const auto& packet = snapshot.latest->frame;
                        const auto stamp = packet.image.metadata().timestamp;
                        const auto& detections = snapshot.latest->inference.detections;
                        if (stamp && detections.timestamp() &&
                            stamp->domain == chromascope::TimestampDomain::CaptureMonotonic &&
                            detections.timestamp()->domain == stamp->domain &&
                            detections.timestamp()->value == stamp->value) {
                            const chromascope::LiveOverlayFrameKey key{
                                camera_generation, snapshot.session, packet.index,
                                packet.image.width(), packet.image.height(), stamp->value};
                            live_track_frame = live_tracker.update(key, detections);
                        }
                    }
                    live_displayed = snapshot.latest;
                } catch (const std::exception& error) {
                    cancel_live_inference();
                    camera_status = std::string("Live analysis preview failed: ") + error.what();
                }
            }
            if (snapshot.stage == chromascope::LiveInferenceStage::Failed) {
                live_displayed.reset();
                live_track_frame.reset();
                live_tracker.reset();
            }
        }
    }

    void update() {
        update_camera();
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
                               chromascope::ImageViewport& view,
                               const chromascope::DetectionResult* video_detections = nullptr,
                               bool detection_labels = true,
                               bool live_overlay = false,
                               const std::vector<std::uint64_t>* live_track_ids = nullptr,
                               double live_minimum_score = 0.0) {
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
        // Draw immutable detection boxes in the SAME source-image transform as
        // the video texture, so pan/zoom and preview downsampling stay aligned.
        // Only the GUI thread draws; the detector worker has no OpenGL access.
        if (video_detections &&
            video_detections->width() == texture.source_width &&
            video_detections->height() == texture.source_height) {
            const auto overlay_start = std::chrono::steady_clock::now();
            // M10.3: recorded-video and live camera share exactly the same
            // source-pixel clipping and canvas projection. Never paint stale
            // detections: callers still enforce their source-frame identity.
            for (const auto& projected : chromascope::make_live_overlay_boxes(
                     *video_detections, view, {size.x, size.y})) {
                const auto& object = video_detections->objects()[projected.detection_index];
                if (live_overlay && !chromascope::live_confidence_visible(
                        object.score, live_minimum_score)) continue;
                const ImVec2 a{origin.x + static_cast<float>(projected.canvas.minimum.x),
                               origin.y + static_cast<float>(projected.canvas.minimum.y)};
                const ImVec2 b{origin.x + static_cast<float>(projected.canvas.maximum.x),
                               origin.y + static_cast<float>(projected.canvas.maximum.y)};
                draw->AddRect(a, b, IM_COL32(45, 205, 235, 255), 0.0f, 0, 2.0f);
                if (detection_labels) {
                    auto caption = chromascope::object_confidence_label(object);
                    if (live_overlay) {
                        // Temporary IDs are visual and session-local, never exported.
                        if (live_track_ids && projected.detection_index < live_track_ids->size()) {
                            const auto id = (*live_track_ids)[projected.detection_index];
                            if (id != 0) caption += " | ID " + std::to_string(id);
                        }
                    }
                    const ImVec2 measured = ImGui::CalcTextSize(caption.c_str());
                    const auto at = chromascope::live_overlay_label_origin(
                        projected.canvas.minimum, {measured.x + 8.0, measured.y + 4.0},
                        {size.x, size.y});
                    const ImVec2 label_at{origin.x + static_cast<float>(at.x),
                                          origin.y + static_cast<float>(at.y)};
                    const ImVec2 label_end{label_at.x + measured.x + 8.0F,
                                           label_at.y + measured.y + 4.0F};
                    draw->AddRectFilled(label_at, label_end, IM_COL32(8, 35, 31, 230), 3.0F);
                    draw->AddText({label_at.x + 4.0F, label_at.y + 2.0F},
                                  IM_COL32(218, 255, 241, 255), caption.c_str());
                }
            }
            // Only video contributes to M6.6 overlay performance measurements.
            if (std::string_view(widget) == "##video_canvas")
                video_overlay_draw_perf.add_ns(static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - overlay_start).count()));
        }
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
            still_opened_path = file;
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

    // M7.6: Explicit in-memory session reset. Cooperative workers may still be
    // finishing OpenCV calls; generation invalidation blocks stale publication.
    // No previously saved export is deleted by this UI action.
    void clear_private_session() {
        public_stream_url.fill('\0');
        public_stream_playlist_file.fill('\0');
        stop_camera();
        live_enable_tracking = false; // reset opt-in alongside in-memory track history
        live_max_inference_fps = 0;
        live_display_confidence_threshold = 0.50F;
        camera_devices = {};
        processing->cancel();
        pending_ticket = 0;
        detector_session.unload();
        video_export_worker.request_stop();
        video_detection.cancel();
        video_player.close();
        video_opened_path.clear();
        video_texture.reset();
        video_view.fit();
        video_seek_target = 0;
        video_scrubbing = false;
        last_scrub_seek_at.reset();
        last_scrub_seek_index.reset();
        video_use_source_fps = true;
        video_manual_playback_fps = 30.0F;
        video_export_audio_ack = false;
        video_status.clear();
        input.release();
        input_frame.reset();
        still_opened_path.clear();
        processed_image.release();
        generalized_result.reset();
        annotation_layer.reset();
        original_texture.reset();
        processed_texture.reset();
        source_overlay_texture.reset();
        processed_overlay_texture.reset();
        have_result = false;
        color_counts.fill(0);
        analysis_elapsed_ms = 0.0;
        input_width = input_height = 0;
        overlay_options.selected_index.reset();
        overlay_options.selected_only = false;
        path.fill('\0');
        video_path.fill('\0');
        detector_model_path.fill('\0');
        export_path.fill('\0');
        video_export_path.fill('\0');
        browser_directory.fill('\0');
        status = "Session reset requested. Existing exports/files are not deleted; "
                 "in-flight OpenCV work may finish before resources are released.";
    }

    void save_person() {
        const auto snapshot = detector_session.snapshot();
        if (!input_frame || detector_profile == 0 ||
            snapshot.stage != chromascope::DnnGuiStage::Completed ||
            !snapshot.person_detections ||
            snapshot.person_detections->width() != input.cols ||
            snapshot.person_detections->height() != input.rows) {
            status = "Run person detection on this image before exporting person boxes.";
            return;
        }
        try {
            const std::filesystem::path destination(export_path.data());
            if (destination.empty() ||
                std::filesystem::weakly_canonical(destination) ==
                std::filesystem::weakly_canonical(still_opened_path))
                throw std::invalid_argument("Choose an output path different from the input image");
            const auto layer = chromascope::make_object_confidence_annotations(
                *snapshot.person_detections);
            chromascope::write_image(destination,
                chromascope::render_annotations(input, layer,
                    {.show_outlines = true, .show_labels = show_labels}));
            status = "Saved person boxes: " + destination.string();
        } catch (const std::exception& e) {
            status = std::string("Person box export failed: ") + e.what();
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
            video_detection.cancel();
            video_export_worker.request_stop(); // Do not publish a previous video's export.
            video_opened_path = file;
            video_texture.reset();
            video_view.fit();
            video_seek_target = 0;
            video_scrubbing = false;
            last_scrub_seek_at.reset();
            last_scrub_seek_index.reset();
            video_use_source_fps = true;
            video_manual_playback_fps = 30.0F;
            video_status.clear();
            std::snprintf(video_path.data(), video_path.size(), "%s", file.string().c_str());
            switch_workspace(1);
        } catch (const std::exception& exception) {
            video_status = std::string("Open failed: ") + exception.what();
        }
    }

    std::string video_status;

    template <std::size_t N>
    static bool set_path_buffer(std::array<char, N>& target,
                                const std::filesystem::path& source) {
        static_assert(N >= 2, "A path buffer must hold a character and terminator");
        const std::string string = source.string();
        if (string.empty() || string.size() >= target.size()) return false;
        std::snprintf(target.data(), target.size(), "%s", string.c_str());
        return true;
    }

    [[nodiscard]] chromascope::GuiPanelLayout panel_layout() const {
        const ImVec2 area = ImGui::GetContentRegionAvail();
        return chromascope::gui_panel_layout(area.x, area.y,
            ui_content_scale, sidebar_width_logical);
    }

    static ImVec2 panel_controls_size(const chromascope::GuiPanelLayout& layout) {
        return layout.stacked ? ImVec2(0.0F, layout.controls_height) :
                                ImVec2(layout.controls_width, 0.0F);
    }

    void next_panel(const chromascope::GuiPanelLayout& layout) {
        if (layout.stacked) return; // Child below, not beside, the controls.
        ImGui::SameLine(0.0F, 0.0F);
        const float height = layout.content_height;
        ImGui::InvisibleButton("##workspace_splitter", ImVec2(layout.splitter_width, height));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        const ImU32 color = ImGui::GetColorU32(ImGui::IsItemHovered() || ImGui::IsItemActive()
            ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
        const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
        const float middle = (lo.x + hi.x) * 0.5F;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(middle, lo.y), ImVec2(middle, hi.y),
                                            color, std::max(1.0F, ui_content_scale));
        if (ImGui::IsItemActive())
            sidebar_width_logical = chromascope::gui_sidebar_drag(sidebar_width_logical,
                ImGui::GetIO().MouseDelta.x, ui_content_scale);
        ImGui::SameLine(0.0F, 0.0F);
    }

    void handle_shortcuts() {
        const ImGuiIO& io = ImGui::GetIO();
        const bool text_input = io.WantTextInput || ImGui::IsAnyItemActive();
        const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
        const auto apply = [this, &io, text_input, popup](
            ImGuiKey key, chromascope::GuiShortcutKey symbol) {
            return ImGui::IsKeyPressed(key, false) ? chromascope::gui_shortcut(
                io.KeyCtrl, io.KeyShift, io.KeyAlt, io.KeySuper,
                text_input, popup, symbol) : chromascope::GuiShortcut::None;
        };
        using chromascope::GuiShortcut;
        const GuiShortcut actions[] = {
            apply(ImGuiKey_F1, chromascope::GuiShortcutKey::F1),
            apply(ImGuiKey_O, chromascope::GuiShortcutKey::O),
            apply(ImGuiKey_F, chromascope::GuiShortcutKey::F),
            apply(ImGuiKey_1, chromascope::GuiShortcutKey::One),
            apply(ImGuiKey_2, chromascope::GuiShortcutKey::Two),
            apply(ImGuiKey_3, chromascope::GuiShortcutKey::Three),
        };
        for (auto action : actions) {
            switch (action) {
            case GuiShortcut::Help: help_requested = true; break;
            case GuiShortcut::Browse:
                if (!live_workspace()) open_file_picker(workspace_mode == 1);
                break; // Ctrl+O never starts camera capture or model inference.
            case GuiShortcut::Fit:
                if (live_workspace()) camera_view.fit();
                else if (workspace_mode == 1) video_view.fit();
                else {
                    result_view.fit(); comparison_original_view.fit();
                    comparison_processed_view.fit();
                }
                break;
            case GuiShortcut::Still: switch_workspace(0); break;
            case GuiShortcut::Video: switch_workspace(1); break;
            case GuiShortcut::Camera: switch_workspace(2); break;
            case GuiShortcut::None: break;
            }
        }
    }

    void draw_help() {
        if (help_requested) {
            ImGui::OpenPopup("ChromascopeIP Help");
            help_requested = false;
        }
        const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize(ImVec2(std::min(590.0F * ui_content_scale, work.x * 0.94F),
                                        std::min(490.0F * ui_content_scale, work.y * 0.92F)),
                                 ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("ChromascopeIP Help", nullptr,
                                    ImGuiWindowFlags_NoSavedSettings)) return;
        ImGui::TextUnformatted("KEYBOARD & WORKSPACE HELP (M10.1)");
        ImGui::Separator();
        if (workspace_mode == 0) {
            ImGui::TextWrapped("STILL IMAGE: Browse/Open loads a local image and runs the "
                "selected classical profile. ONNX detection requires explicit model verification "
                "and the Detect people action. Preview and exports remain separate.");
        } else if (workspace_mode == 1) {
            ImGui::TextWrapped("RECORDED VIDEO: Open a local video, then Play/Pause or seek. "
                "CPU detection and exports are separate explicit actions; AVI output has no audio.");
        } else {
            ImGui::TextWrapped("LIVE CAMERA: Navigating here does not open a device. "
                "Select backend/device, press Start camera, and separately start CPU inference. "
                "Recording requires a separate Start recording action after the camera opens. "
                "Use Export to select AVI/MP4 output or an optional destination. "
                "Stop/leave workspace requests camera release and finalizes recording. "
                "Pause freezes analysis, not capture or recording.");
        }
        ImGui::Separator();
        ImGui::TextWrapped("Ctrl+1: Still images | Ctrl+2: Recorded video | Ctrl+3: Live camera");
        ImGui::TextWrapped("Ctrl+O: Browse still images or recorded videos (not camera)");
        ImGui::TextWrapped("Ctrl+F: Fit the current image/video/camera preview");
        ImGui::TextWrapped("F1: Open this help. Shortcuts do not run while editing text or in a modal dialog.");
        ImGui::Separator();
        ImGui::TextWrapped("Resize the application window or drag the separator between controls "
            "and preview. On narrow windows, controls stack above the preview. "
            "Controls scroll independently. DPI scale follows the active monitor.");
        ImGui::TextWrapped("Browser: choose a folder, single-click a compatible file to select; "
            "double-click it or press Open selected to load. Models require explicit SHA-256 "
            "verification and a separate Load action.");
        ImGui::TextWrapped("View navigation: mouse wheel zoom; middle/right-drag pan; "
            "double-click preview to fit. Camera selection never starts capture or inference.");
        ImGui::Separator();
        ImGui::TextWrapped("Local-only first-party workflows: no telemetry or automatic camera "
            "access. Session clearing does not delete previously exported files. "
            "See docs/USER_GUIDE.md and docs/PRIVACY_GUIDE.md.");
        if (ImGui::Button("Close help")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Both in-app dialogs use the same small top-right close control.
    // Modal backdrop clicks dismiss the dialog without acting on the workspace.
    // Ignore the opening mouse press so a click on the toolbar cannot
    // immediately dismiss a newly opened popup.
    static bool modal_header(const char* heading, const char* close_id,
                             const char* close_tooltip) {
        const ImVec2 top_left = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);
        const bool backdrop_clicked = !ImGui::IsWindowAppearing() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseHoveringRect(top_left, bottom_right, false);

        ImGui::TextUnformatted(heading);
        ImGui::SameLine();
        const float close_width = ImGui::GetFrameHeight();
        const float close_x = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - close_width;
        if (ImGui::GetCursorPosX() < close_x)
            ImGui::SetCursorPosX(close_x);
        const bool close_clicked = ImGui::Button(close_id, ImVec2(close_width, 0.0F));
        ImGui::SetItemTooltip("%s", close_tooltip);
        if (close_clicked || backdrop_clicked) {
            ImGui::CloseCurrentPopup();
            return true;
        }
        ImGui::Separator();
        return false;
    }

    void draw_preferences() {
        if (preferences_requested) {
            ImGui::OpenPopup("ChromascopeIP Preferences");
            preferences_requested = false;
        }
        const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize(ImVec2(std::min(620.0F * ui_content_scale, work.x * 0.94F),
                                        std::min(440.0F * ui_content_scale, work.y * 0.92F)),
                                 ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("ChromascopeIP Preferences", nullptr,
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoTitleBar)) return;
        if (modal_header("Preferences", "x##close_preferences", "Close Preferences")) {
            ImGui::EndPopup();
            return;
        }
        ImGui::TextUnformatted("Local privacy and session controls");
        ImGui::Separator();
        ImGui::TextWrapped("This app's first-party runtime has no upload, "
            "telemetry, face recognition, or identity matching feature. "
            "Person boxes are COCO class 0 predictions, not identities.");
        ImGui::TextWrapped("Images, model weights, decoded frames, camera "
            "preview pixels and caches stay local. Live inference requires "
            "a separate Start action. Live recording is a separate, explicit "
            "local action; no recording or uploads occur automatically. "
            "File exports are explicit; AVI exports discard audio.");
        ImGui::TextDisabled("No automatic ImGui layout/log file is written.");
        ImGui::Separator();
        ImGui::TextWrapped("Schema v1 preferences: saved only on request. Profile and "
            "non-sensitive display settings only; no file paths, model paths, "
            "camera activation, images or tracking IDs are stored.");
        if (ImGui::Button("Save preferences")) save_gui_preferences();
        ImGui::SameLine();
        ImGui::BeginDisabled(camera_capture.state() == chromascope::CameraCaptureState::Capturing ||
                             live_inference_active || video_export_busy.load());
        if (ImGui::Button("Reload saved preferences")) load_gui_preferences();
        ImGui::EndDisabled();
        if (ImGui::Button("Delete saved preferences")) delete_saved_gui_preferences();
        ImGui::TextWrapped("%s", preferences_status.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("Privacy details: docs/PRIVACY_GUIDE.md");
        ImGui::EndPopup();
    }

    void start_live_recording() {
        try {
            if (!live_workspace() ||
                camera_capture.state() != chromascope::CameraCaptureState::Capturing)
                throw std::invalid_argument("Start the live source before recording");
            const auto format = live_record_format == 0
                ? chromascope::LiveRecordingFormat::AVI
                : chromascope::LiveRecordingFormat::MP4;
            const std::filesystem::path custom(live_record_path.data());
            auto destination = chromascope::resolve_live_recording_path(custom, format);
            // Create the user's default Videos folder only after an explicit
            // Start recording action, not on startup, browsing or modal open.
            if (custom.empty()) {
                std::error_code error;
                std::filesystem::create_directories(destination.parent_path(), error);
                if (error) throw std::filesystem::filesystem_error(
                    "Cannot create Videos folder", destination.parent_path(), error);
            }
            const double reported = camera_active_format.fps;
            const double fps = std::isfinite(reported) && reported >= 1.0 && reported <= 120.0
                ? reported : 30.0;
            live_recorder.start(destination, format, fps);
            live_record_error.clear();
        } catch (const std::exception& error) {
            live_record_error = std::string("Recording not started: ") + error.what();
        }
    }

    void draw_live_record_actions() {
        const auto rec = live_recorder.status();
        const bool running = live_recorder.accepting();
        const bool finalizing = rec.stage == chromascope::LiveRecordingStage::Finalizing;
        ImGui::BeginDisabled(running || finalizing ||
            camera_capture.state() != chromascope::CameraCaptureState::Capturing);
        if (ImGui::Button("Start recording", ImVec2(-1, 0))) start_live_recording();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!running);
        if (ImGui::Button("Stop recording", ImVec2(-1, 0))) live_recorder.stop();
        ImGui::EndDisabled();
        ImGui::Text("Frames encoded: %llu | dropped: %llu",
            static_cast<unsigned long long>(rec.frames_written),
            static_cast<unsigned long long>(rec.dropped_frames));
        ImGui::TextWrapped("%s", rec.message.c_str());
        if (!live_record_error.empty()) ImGui::TextWrapped("%s", live_record_error.c_str());
    }

    void draw_live_record_controls(bool in_modal) {
        if (in_modal) {
            ImGui::TextWrapped("Camera video is saved locally without audio. Visible live "
                "detection boxes and labels are burned into the recording while inference "
                "is active. Turn inference off to record raw footage. The recorded image "
                "follows the analyzed preview while inference runs.");
            const auto record_state = live_recorder.status();
            ImGui::BeginDisabled(live_recorder.accepting() ||
                record_state.stage == chromascope::LiveRecordingStage::Finalizing);
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::InputText("##live_record_destination", live_record_path.data(), live_record_path.size());
            ImGui::TextDisabled("Optional filename or directory; blank uses your OS Videos folder.");
            ImGui::Combo("Recording format", &live_record_format,
                "AVI (MJPEG)\0MP4 (H.264 / MPEG-4)\0");
            ImGui::EndDisabled();
            if (live_record_path[0] == '\0') {
                try {
                    const auto format = live_record_format == 0
                        ? chromascope::LiveRecordingFormat::AVI
                        : chromascope::LiveRecordingFormat::MP4;
                    ImGui::TextWrapped("Default: %s",
                        chromascope::default_live_recording_path(format).string().c_str());
                } catch (const std::exception& error) {
                    ImGui::TextWrapped("Default unavailable: %s", error.what());
                }
            }
            ImGui::TextDisabled("MP4 encoder availability depends on the installed OS backend.");
        }
        draw_live_record_actions();
    }

    // One mode-aware export entry; no export operations execute when opening
    // the dialog. Source-specific callbacks stay unchanged and explicit.
    void draw_export() {
        if (export_requested) {
            ImGui::OpenPopup("ChromascopeIP Export");
            export_requested = false;
        }
        const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize(ImVec2(std::min(620.0F * ui_content_scale, work.x * 0.94F),
                                        std::min(490.0F * ui_content_scale, work.y * 0.92F)),
                                 ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("ChromascopeIP Export", nullptr,
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoTitleBar)) return;
        if (modal_header("Export", "x##close_export", "Close Export")) {
            ImGui::EndPopup();
            return;
        }
        if (workspace_mode == 0) {
            ImGui::TextUnformatted("STILL IMAGE EXPORT");
            draw_still_export_controls();
        } else if (workspace_mode == 1) {
            ImGui::TextUnformatted("ANNOTATED VIDEO EXPORT (M6.4)");
            draw_video_export_controls();
        } else {
            ImGui::TextUnformatted(workspace_mode == 3 ? "HTTPS LIVE STREAM RECORDING" : "LOCAL LIVE CAMERA RECORDING");
            draw_live_record_controls(true);
        }
        ImGui::EndPopup();
    }

    void open_file_picker(bool for_video = false, bool for_model = false,
                          bool for_stream_playlist = false) {
        picker_video = for_video;
        picker_model = for_model;
        picker_stream_playlist = for_stream_playlist;
        browser_selected.clear();
        browser_status_message.clear();
        const std::filesystem::path selected(for_stream_playlist ? public_stream_playlist_file.data() :
            (for_model ? detector_model_path.data() :
            (for_video ? video_path.data() : path.data())));
        const auto dir = selected.has_parent_path() ? selected.parent_path()
                                                    : std::filesystem::path(".");
        if (!set_path_buffer(browser_directory, dir))
            browser_status_message = "Directory path too long for the picker.";
        browse_requested = true;
    }

    void accept_browser_selection(const std::filesystem::path& selected) {
        const auto kind = picker_stream_playlist ? chromascope::GuiPickerKind::HlsPlaylist :
            (picker_model ? chromascope::GuiPickerKind::Model :
            (picker_video ? chromascope::GuiPickerKind::Video : chromascope::GuiPickerKind::Image));
        auto extension = selected.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::error_code error;
        if (!chromascope::gui_picker_extension(extension, kind) ||
            std::filesystem::is_symlink(selected, error) || error ||
            !std::filesystem::is_regular_file(selected, error) || error) {
            browser_status_message = "Select an existing, regular supported file (not a symlink).";
            return;
        }
        if (picker_stream_playlist) {
            std::error_code ec;
            const auto absolute = std::filesystem::absolute(selected, ec);
            if (ec || !set_path_buffer(public_stream_playlist_file, absolute)) {
                browser_status_message = "Playlist path is too long.";
                return;
            }
        } else if (picker_model) {
            if (!set_path_buffer(detector_model_path, selected)) {
                browser_status_message = "Model path is too long.";
                return;
            }
            detector_session.unload();
            video_detection.cancel();
        } else if (picker_video) open_video(selected);
        else load(selected);
        ImGui::CloseCurrentPopup();
    }

    void draw_file_picker() {
        const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize(ImVec2(std::min(760.0F * ui_content_scale, work.x * 0.95F),
                                        std::min(560.0F * ui_content_scale, work.y * 0.93F)),
                                 ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Select media", nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) return;
        ImGui::TextWrapped("%s", picker_stream_playlist ? "Choose downloaded HLS master (.m3u8)" :
            (picker_model ? "Choose a local ONNX model (.onnx)" :
            (picker_video ? "Choose a recorded video" : "Choose a photograph")));
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##directory", browser_directory.data(), browser_directory.size());
        const auto navigate = [this](const std::filesystem::path& next) {
            if (!set_path_buffer(browser_directory, next)) {
                browser_status_message = "Directory path is too long.";
                return;
            }
            browser_selected.clear();
            browser_status_message.clear();
        };
        if (ImGui::Button("Go to directory"))
            navigate(std::filesystem::path(browser_directory.data()));
        ImGui::SameLine();
        if (ImGui::Button("Up")) {
            const auto parent = std::filesystem::path(browser_directory.data()).parent_path();
            navigate(parent.empty() ? std::filesystem::path(".") : parent);
        }
        ImGui::SameLine();
        if (ImGui::Button("Current folder")) navigate(std::filesystem::path("."));
        ImGui::SameLine();
        ImGui::Checkbox("Show hidden", &browser_show_hidden);
        ImGui::Separator();

        const auto dir = std::filesystem::path(browser_directory.data());
        const auto kind = picker_stream_playlist ? chromascope::GuiPickerKind::HlsPlaylist :
            (picker_model ? chromascope::GuiPickerKind::Model :
            (picker_video ? chromascope::GuiPickerKind::Video : chromascope::GuiPickerKind::Image));
        std::error_code error;
        std::filesystem::directory_iterator cursor(dir, error);
        constexpr std::size_t maximum_entries = 1024;
        std::vector<std::pair<std::filesystem::path, bool>> entries;
        if (!error) {
            const std::filesystem::directory_iterator end;
            std::size_t examined = 0;
            for (; cursor != end && entries.size() < maximum_entries &&
                   examined < 4096; cursor.increment(error), ++examined) {
                if (error) break;
                const auto& entry = *cursor;
                const auto filename = entry.path().filename().string();
                if (filename.empty() || (!browser_show_hidden && filename[0] == '.')) continue;
                std::error_code entry_error;
                if (entry.is_symlink(entry_error) || entry_error) continue;
                const bool directory = entry.is_directory(entry_error);
                if (entry_error) continue;
                if (!directory) {
                    if (!entry.is_regular_file(entry_error) || entry_error) continue;
                    auto extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (!chromascope::gui_picker_extension(extension, kind)) continue;
                }
                entries.emplace_back(entry.path(), directory);
            }
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first.filename().string() < b.first.filename().string();
        });
        if (error) ImGui::TextWrapped("Directory unavailable: %s", error.message().c_str());
        else if (entries.size() == maximum_entries)
            ImGui::TextDisabled("Showing at most 1024 supported entries; narrow the directory.");
        if (ImGui::BeginChild("Files", ImVec2(0.0F, -94.0F * ui_content_scale),
                               ImGuiChildFlags_Borders)) {
            for (const auto& item : entries) {
                const auto& entry = item.first;
                const std::string label = std::string(item.second ? "[Folder] " : "[File] ") +
                    entry.filename().string();
                if (ImGui::Selectable(label.c_str(), browser_selected == entry)) {
                    if (item.second) {
                        navigate(entry);
                        break;
                    }
                    browser_selected = entry;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        accept_browser_selection(entry);
                        break;
                    }
                }
            }
        }
        ImGui::EndChild();
        if (!browser_status_message.empty())
            ImGui::TextWrapped("%s", browser_status_message.c_str());
        else if (!browser_selected.empty())
            ImGui::TextWrapped("Selected: %s", browser_selected.filename().string().c_str());
        else ImGui::TextDisabled("Single-click to select; double-click or Open selected to load.");
        ImGui::BeginDisabled(browser_selected.empty());
        if (ImGui::Button("Open selected")) accept_browser_selection(browser_selected);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Right-click on a slider to restore its active preference-profile value
    // (if persisted), otherwise the control's existing application default.
    // Return true on reset just as on a drag so existing effects and
    // cancellation paths are executed without duplicating business logic.
    [[nodiscard]] double slider_ui_default(std::string_view key, double fallback) const {
        if (active_preference_defaults) {
            const auto it = active_preference_defaults->ui.find(key);
            if (it != active_preference_defaults->ui.end()) return it->second;
        }
        return fallback;
    }

    [[nodiscard]] double slider_profile_default(std::string_view profile,
                                                 std::string_view key, double fallback) const {
        if (active_preference_defaults) {
            const auto found = active_preference_defaults->profiles.find(profile);
            if (found != active_preference_defaults->profiles.end()) {
                const auto value = found->second.find(key);
                if (value != found->second.end()) return value->second;
            }
        }
        return fallback;
    }

    // A popup is attached to the actual ImGui slider item, not its label text.
    static bool slider_reset_menu() {
        bool reset = false;
        if (ImGui::BeginPopupContextItem()) {
            reset = ImGui::MenuItem("Reset to Default Value");
            ImGui::EndPopup();
        }
        return reset;
    }

    // Reserve enough room for both the slider and its label. When a narrow
    // sidebar cannot accommodate both, move the wrapped label above a
    // full-width slider instead of clipping the description at the right edge.
    static bool control_label_above(const char* label) {
        const float available = ImGui::GetContentRegionAvail().x;
        const float label_width = ImGui::CalcTextSize(label, nullptr, true).x;
        const float minimum_track = std::max(110.0F, 5.0F * ImGui::GetFrameHeight());
        if (available - label_width - ImGui::GetStyle().ItemInnerSpacing.x < minimum_track) {
            ImGui::TextWrapped("%s", label);
            ImGui::SetNextItemWidth(-1.0F);
            return true;
        }
        ImGui::SetNextItemWidth(available - label_width - ImGui::GetStyle().ItemInnerSpacing.x);
        return false;
    }

    static bool slider_float(const char* label, float* current, float minimum,
                             float maximum, float default_value,
                             const char* format = "%.3f") {
        ImGui::PushID(label);
        const bool wrapped = control_label_above(label);
        bool changed = ImGui::SliderFloat(wrapped ? "##value" : label,
                                         current, minimum, maximum, format);
        if (slider_reset_menu() && *current != default_value) {
            *current = default_value;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    static bool slider_int(const char* label, int* current, int minimum,
                           int maximum, int default_value) {
        ImGui::PushID(label);
        const bool wrapped = control_label_above(label);
        bool changed = ImGui::SliderInt(wrapped ? "##value" : label,
                                       current, minimum, maximum);
        if (slider_reset_menu() && *current != default_value) {
            *current = default_value;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    // Combo boxes and labeled text/numeric inputs also need room for their
    // labels when the user drags the splitter to its narrowest position.
    static bool responsive_combo(const char* label, int* value, const char* items) {
        ImGui::PushID(label);
        const bool wrapped = control_label_above(label);
        const bool changed = ImGui::Combo(wrapped ? "##value" : label, value, items);
        ImGui::PopID();
        return changed;
    }

    static bool responsive_input_text(const char* label, char* value, std::size_t capacity) {
        ImGui::PushID(label);
        const bool wrapped = control_label_above(label);
        const bool changed = ImGui::InputText(wrapped ? "##value" : label, value, capacity);
        ImGui::PopID();
        return changed;
    }

    static bool responsive_input_int(const char* label, int* value) {
        ImGui::PushID(label);
        const bool wrapped = control_label_above(label);
        const bool changed = ImGui::InputInt(wrapped ? "##value" : label, value);
        ImGui::PopID();
        return changed;
    }

    static bool responsive_seek_input(const char* label, std::uint64_t* value) {
        ImGui::PushID(label);
        const bool wrapped = control_label_above(label);
        const bool changed = ImGui::InputScalar(wrapped ? "##value" : label,
            ImGuiDataType_U64, value, nullptr, nullptr, nullptr,
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopID();
        return changed;
    }

    // Equal-sized buttons in a row, irrespective of the sidebar splitter.
    static float control_button_width(int columns) {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        return std::max(1.0F,
            (ImGui::GetContentRegionAvail().x - spacing * (columns - 1)) / columns);
    }

    void draw_detector_controls() {
        ImGui::Separator();
        ImGui::TextUnformatted("PERSON DETECTION / MODEL (M7.5)");
        if (responsive_combo("Detector profile", &detector_profile,
                         "Disabled (classical profiles)\0NanoDet COCO-80 / CPU\0")) {
            detector_session.unload();
        }
        if (detector_profile == 0) {
            ImGui::TextDisabled("Classical image processing is unchanged.");
            return;
        }
        ImGui::TextWrapped("NanoDet-Plus-M 1.5x, 416 x 416, 80 classes. "
                           "ONNX weights are not bundled.");
        if (responsive_input_text("ONNX path", detector_model_path.data(),
                             detector_model_path.size())) {
            // Editing a path invalidates the prior verified identity immediately.
            detector_session.unload();
            video_detection.cancel();
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
        ImGui::TextWrapped("Only COCO-80 classes are supported. Unfamiliar objects can be "
                           "misidentified; confidence is not calibrated probability.");
        if (ImGui::CollapsingHeader("Supported NanoDet classes (80)")) {
            for (int id = 0; id < chromascope::kNanoDetClassCount; ++id)
                ImGui::Text("%02d  %s", id,
                            std::string(chromascope::nanodet_class_label(id)).c_str());
        }
        float confidence = static_cast<float>(detector_options.confidence_threshold);
        if (slider_float("Confidence cutoff", &confidence, 0.0f, 1.0f,
                         static_cast<float>(chromascope::DnnGuiOptions{}.confidence_threshold), "%.2f")) {
            detector_options.confidence_threshold = confidence;
            detector_session.clear_results();
        }
        float iou = static_cast<float>(detector_options.nms_iou_threshold);
        if (slider_float("NMS IoU cutoff", &iou, 0.0f, 1.0f,
                         static_cast<float>(chromascope::DnnGuiOptions{}.nms_iou_threshold), "%.2f")) {
            detector_options.nms_iou_threshold = iou;
            detector_session.clear_results();
        }
        if (slider_int("Max detections", &detector_options.max_detections, 1, 512,
                       chromascope::DnnGuiOptions{}.max_detections))
            detector_session.clear_results();
        if (slider_float("Still-image person display threshold", &still_person_display_threshold,
                             0.0F, 1.0F, 0.50F, "%.2f"))
            detector_session.clear_results();
        ImGui::TextDisabled("Person boxes require BOTH confidence and display thresholds.");
        ImGui::Text("Effective person cutoff: %.2f",
            static_cast<double>(std::max({still_person_display_threshold,
                static_cast<float>(detector_options.confidence_threshold),
                detector_class_overrides[0].value_or(0.0F)})));
        if (ImGui::CollapsingHeader("Per-class thresholds (recorded-video detection)")) {
            slider_int("COCO class ID", &detector_selected_class, 0,
                       chromascope::kNanoDetClassCount - 1, 0);
            ImGui::Text("Class: %s", std::string(chromascope::nanodet_class_label(
                detector_selected_class)).c_str());
            auto& override_score = detector_class_overrides[
                static_cast<std::size_t>(detector_selected_class)];
            bool enabled = override_score.has_value();
            if (ImGui::Checkbox("Class override", &enabled)) {
                if (enabled) override_score = 0.75F;
                else override_score.reset();
                if (detector_selected_class == 0) detector_session.clear_results();
            }
            if (override_score.has_value()) {
                float score = *override_score;
                if (slider_float("Class confidence", &score,
                                 0.0F, 1.0F, 0.75F, "%.2f")) {
                    override_score = score;
                    if (detector_selected_class == 0) detector_session.clear_results();
                }
            }
            ImGui::TextDisabled("Class 0 override also applies to still-image person boxes.");
        }
        ImGui::TextDisabled("Raw tensor diagnostics ignore the confidence sliders.");
        ImGui::TextDisabled("Person boxes use the same class-0 postprocessor as CLI/video.");
        ImGui::TextDisabled("Select People in image above to find this workflow again.");
        const bool ready = detector_session.ready_for(detector_model_path.data());
        ImGui::BeginDisabled(!ready || !input_frame || busy);
        if (ImGui::Button("Detect people in still image", ImVec2(-1, 0))) {
            try {
                chromascope::validate_dnn_gui_options(detector_options);
                chromascope::DnnPostprocessOptions policy;
                policy.person_only = true;
                policy.person_display_threshold = static_cast<double>(still_person_display_threshold);
                policy.confidence_threshold = detector_options.confidence_threshold;
                policy.nms_iou_threshold = detector_options.nms_iou_threshold;
                policy.max_detections = static_cast<std::size_t>(detector_options.max_detections);
                if (const auto score = detector_class_overrides[0])
                    policy.class_thresholds.push_back({0, static_cast<double>(*score)});
                detector_session.run_person(input_frame, policy);
            } catch (const std::exception& e) {
                status = std::string("Person detection rejected: ") + e.what();
            }
        }
        if (ImGui::Button("Run raw CPU inference (diagnostics)", ImVec2(-1, 0))) {
            try {
                chromascope::validate_dnn_gui_options(detector_options);
                detector_session.run(input_frame);
            } catch (const std::exception& e) {
                status = std::string("Raw CPU inference rejected: ") + e.what();
            }
        }
        ImGui::EndDisabled();
        if (busy && ImGui::Button("Cancel inference / load", ImVec2(-1, 0)))
            detector_session.cancel();
        if (!busy && ImGui::Button("Unload model", ImVec2(-1, 0)))
            detector_session.unload();
        if (state.stage == chromascope::DnnGuiStage::Completed) {
            if (state.person_detections) {
                ImGui::Text("Person detection: %.1f ms", state.elapsed_ms);
                ImGui::Text("Accepted people: %zu", state.person_detections->objects().size());
                ImGui::Text("Weak person candidates (diagnostic): %zu",
                            state.person_summary.weak_candidates);
                ImGui::TextDisabled("Only COCO class 0; scores are not probabilities.");
            } else ImGui::Text("Raw inference: %.1f ms", state.elapsed_ms);
            for (std::size_t i = 0; i < state.output_shapes.size(); ++i) {
                std::string shape;
                for (int dim : state.output_shapes[i]) {
                    if (!shape.empty()) shape += " x ";
                    shape += std::to_string(dim);
                }
                ImGui::Text("Tensor %zu: [%s]", i + 1, shape.c_str());
            }
        }
        ImGui::TextWrapped("Boxes are original-image coordinates with percent confidence. "
                           "Uncertain and other-class predictions are not person detections.");
    }

    void start_video_detection() {
        try {
            if (video_opened_path.empty() || !video_player.has_source())
                throw std::invalid_argument("Open a recorded video before detecting objects");
            chromascope::DnnPostprocessOptions policy;
            policy.confidence_threshold = detector_options.confidence_threshold;
            policy.nms_iou_threshold = detector_options.nms_iou_threshold;
            policy.max_detections = static_cast<std::size_t>(detector_options.max_detections);
            policy.uncertainty_floor = static_cast<double>(video_uncertainty_floor);
            policy.person_only = video_detection_profile == 1;
            policy.person_display_threshold = static_cast<double>(video_person_display_threshold);
            for (int i = 0; i < chromascope::kNanoDetClassCount; ++i) {
                if (const auto value = detector_class_overrides[static_cast<std::size_t>(i)])
                    policy.class_thresholds.push_back({i, *value});
            }
            (void)video_detection.start(video_opened_path, detector_model_path.data(), policy);
            video_overlay_draw_perf = {};
            video_status.clear();
        } catch (const std::exception& e) {
            video_status = std::string("Video detection rejected: ") + e.what();
        }
    }

    void start_video_export() {
        if (video_export_busy.load()) return;
        try {
            if (video_opened_path.empty() || !video_player.has_source())
                throw std::invalid_argument("Open a recorded video before exporting");
            if (!video_export_audio_ack)
                throw std::invalid_argument("Confirm that source audio will be dropped");
            const std::filesystem::path destination(video_export_path.data());
            if (destination.extension() != ".avi")
                throw std::invalid_argument("MJPEG export requires .avi output");
            chromascope::DnnPostprocessOptions policy;
            policy.confidence_threshold = detector_options.confidence_threshold;
            policy.nms_iou_threshold = detector_options.nms_iou_threshold;
            policy.max_detections = static_cast<std::size_t>(detector_options.max_detections);
            policy.uncertainty_floor = static_cast<double>(video_uncertainty_floor);
            policy.person_only = video_detection_profile == 1;
            policy.person_display_threshold = static_cast<double>(video_person_display_threshold);
            for (int i = 0; i < chromascope::kNanoDetClassCount; ++i)
                if (const auto value = detector_class_overrides[static_cast<std::size_t>(i)])
                    policy.class_thresholds.push_back({i, *value});
            chromascope::validate_dnn_postprocess_options(policy);
            // Joining a completed job cannot block on a running inference.
            if (video_export_worker.joinable()) video_export_worker.join();
            video_detection.cancel(); // Prevent two NanoDet workers from competing for CPU.
            video_export_count.store(0);
            const auto input_file = video_opened_path;
            const std::filesystem::path model(detector_model_path.data());
            const std::optional<double> fps = video_export_override_fps
                ? std::optional<double>{static_cast<double>(video_export_fps)} : std::nullopt;
            video_export_busy.store(true);
            {
                std::lock_guard lock(video_export_status_mutex);
                video_export_status = "Encoding video on a background worker...";
                video_export_perf = {};
            }
            video_export_worker = std::jthread(
                [this, input_file, destination, model, policy, fps](std::stop_token stop) {
                std::string message;
                try {
                    auto source = chromascope::VideoSource::open(input_file);
                    chromascope::RecordedVideoObjectDetector detector(model, policy);
                    const auto summary = chromascope::export_annotated_video(
                        source, destination,
                        [&](const chromascope::VideoFramePacket& packet) {
                            return detector.detect(packet);
                        }, {.max_decoded_frames = 100000, .output_fps = fps,
                            .show_labels = true,
                            .canceled = [&] { return stop.stop_requested(); },
                            .progress = [this](std::uint64_t count) {
                                video_export_count.store(count);
                            },
                            .timing_progress = [this](const chromascope::VideoPerformanceMetrics& perf) {
                                std::lock_guard lock(video_export_status_mutex);
                                video_export_perf = perf;
                            }});
                    message = "Saved silent MJPEG/AVI: " + destination.string() +
                              " (" + std::to_string(summary.frames_written) + " frames, " +
                              std::to_string(summary.confidence.weak_candidates) +
                              " weak candidates, " +
                              std::to_string(summary.confidence.uncertain_only_frames) +
                              " uncertain-only frames)";
                } catch (const std::exception& e) {
                    message = std::string("Video export not completed: ") + e.what();
                }
                {
                    std::lock_guard lock(video_export_status_mutex);
                    video_export_status = std::move(message);
                }
                video_export_busy.store(false);
            });
        } catch (const std::exception& e) {
            video_export_busy.store(false);
            std::lock_guard lock(video_export_status_mutex);
            video_export_status = std::string("Video export rejected: ") + e.what();
        }
    }

    // Original still-image export settings and callbacks, moved unchanged
    // out of the sidebar and into the shared mode-aware Export dialog.
    void draw_still_export_controls() {
        ImGui::Combo("Export image", &export_base_mode,
                     "As configured\0Original pixels\0Processed pixels\0");
        ImGui::InputText("##export", export_path.data(), export_path.size());
        if (ImGui::Button("Export annotated image", ImVec2(-1, 0))) save();
        const auto still_person_snapshot = detector_session.snapshot();
        const bool person_available = detector_profile != 0 &&
            still_person_snapshot.stage == chromascope::DnnGuiStage::Completed &&
            still_person_snapshot.person_detections && input_frame;
        ImGui::BeginDisabled(!person_available);
        if (ImGui::Button("Export person boxes on original", ImVec2(-1, 0))) save_person();
        ImGui::EndDisabled();
    }

    // The original AVI export widgets and handlers, now hosted by Export.
    // Cancel and progress remain accessible while a background export runs.
    void draw_video_export_controls() {
        const bool has_source = video_player.has_source();
        ImGui::TextDisabled("Codec: MJPEG / AVI | Timing: constant FPS");
        ImGui::InputText("AVI destination", video_export_path.data(), video_export_path.size());
        ImGui::Checkbox("Discard source audio (silent AVI)", &video_export_audio_ack);
        ImGui::Checkbox("Override nominal source FPS", &video_export_override_fps);
        if (video_export_override_fps)
            slider_float("Output FPS", &video_export_fps, 1.0F, 120.0F, 25.0F, "%.1f");
        ImGui::TextDisabled("Source VFR timestamps are not retained in AVI.");
        ImGui::BeginDisabled(!has_source || video_export_busy.load() || !video_export_audio_ack);
        if (ImGui::Button("Export annotated AVI", ImVec2(-1, 0))) start_video_export();
        ImGui::EndDisabled();
        if (video_export_busy.load() &&
            ImGui::Button("Cancel AVI export", ImVec2(-1, 0)))
            video_export_worker.request_stop();
        ImGui::Text("Frames encoded: %llu",
            static_cast<unsigned long long>(video_export_count.load()));
        {
            std::lock_guard lock(video_export_status_mutex);
            ImGui::TextWrapped("%s", video_export_status.c_str());
            const auto show_stage = [](const char* label, const chromascope::VideoStageStatistics& stats) {
                if (stats.measured()) ImGui::Text("%s: %.3f ms/frame, %.1f ms total (%llu samples)",
                    label, stats.mean_ms(), stats.total_ms(),
                    static_cast<unsigned long long>(stats.samples));
                else ImGui::TextDisabled("%s: not measured", label);
            };
            show_stage("AVI decode", video_export_perf.decode);
            show_stage("AVI inference", video_export_perf.inference);
            show_stage("AVI overlay", video_export_perf.overlay);
            show_stage("AVI encode", video_export_perf.encode);
        }
        ImGui::TextDisabled("CPU stage wall time; model loading, file closing/publishing and GPU time excluded.");
    }

    void draw_video_ui() {
        const auto layout = panel_layout();
        ImGui::BeginChild("Video controls", panel_controls_size(layout), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("RECORDED VIDEO");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##video_path", video_path.data(), video_path.size());
        const float video_file_button_width = control_button_width(2);
        if (ImGui::Button("Open video", ImVec2(video_file_button_width, 0.0F)))
            open_video(video_path.data());
        ImGui::SetItemTooltip("Open the local recording at the path above. "
                              "Video decoding runs on a bounded background worker.");
        ImGui::SameLine();
        if (ImGui::Button("Browse video...", ImVec2(-1.0F, 0.0F)))
            open_file_picker(true);
        ImGui::SetItemTooltip("Browse for a local video file instead of typing "
                              "or pasting its absolute path.");
        ImGui::Separator();

        const bool has_source = video_player.has_source();
        ImGui::BeginDisabled(!has_source);
        const float transport_button_width = control_button_width(3);
        if (ImGui::Button(video_player.playing() ? "Pause" : "Play",
                          ImVec2(transport_button_width, 0.0F))) {
            if (video_player.playing()) video_player.pause();
            else video_player.play();
        }
        ImGui::SameLine();
        if (ImGui::Button("< Frame", ImVec2(transport_button_width, 0.0F))) {
            try { video_player.step_backward(); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Frame >", ImVec2(-1.0F, 0.0F))) {
            try { video_player.step_forward(); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        const float seek_button_width = control_button_width(2);
        if (ImGui::Button("Beginning", ImVec2(seek_button_width, 0.0F))) {
            try { video_player.seek(0); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Close video", ImVec2(-1, 0))) {
            video_export_worker.request_stop();
            video_detection.cancel();
            video_opened_path.clear();
            video_player.close();
            video_scrubbing = false;
            last_scrub_seek_at.reset();
            last_scrub_seek_index.reset();
            video_use_source_fps = true;
            video_manual_playback_fps = 30.0F;
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
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::SliderScalar("Timeline", ImGuiDataType_U64,
                                &video_seek_target, &first, &last);
            const bool timeline_active = ImGui::IsItemActive();
            const bool timeline_deactivated_after_edit = ImGui::IsItemDeactivatedAfterEdit();
            const bool timeline_deactivated = ImGui::IsItemDeactivated();
            const bool timeline_reset = slider_reset_menu();
            if (timeline_reset) video_seek_target = first;
            if (timeline_active) {
                // VideoPlayer::seek() enqueues work on the bounded decoder thread.
                // Throttle requests so dragging previews intermediate frames
                // instead of repeatedly cancelling every in-flight decode.
                video_scrubbing = true;
                video_player.pause();
                const auto now = chromascope::VideoPlayer::Clock::now();
                constexpr auto scrub_interval = std::chrono::milliseconds(150);
                if ((!last_scrub_seek_index || *last_scrub_seek_index != video_seek_target) &&
                    (!last_scrub_seek_at || now - *last_scrub_seek_at >= scrub_interval)) {
                    try {
                        video_player.seek(video_seek_target);
                        last_scrub_seek_index = video_seek_target;
                        last_scrub_seek_at = now;
                    } catch (const std::exception& e) { video_status = e.what(); }
                }
            }
            if (timeline_deactivated_after_edit || timeline_reset) {
                video_scrubbing = false;
                video_player.pause();
                // Always commit the final chosen frame, including an unissued
                // target skipped by the throttled preview requests.
                if (timeline_reset || !last_scrub_seek_index ||
                    *last_scrub_seek_index != video_seek_target) {
                    try { video_player.seek(video_seek_target); }
                    catch (const std::exception& e) { video_status = e.what(); }
                }
                last_scrub_seek_at.reset();
                last_scrub_seek_index.reset();
            } else if (timeline_deactivated) {
                video_scrubbing = false;
                last_scrub_seek_at.reset();
                last_scrub_seek_index.reset();
            }
            ImGui::TextDisabled("Reported frames: %llu (decoder estimate)",
                static_cast<unsigned long long>(*metadata->reported_frame_count));
        } else {
            ImGui::TextDisabled("Total frame count unavailable: use absolute seek.");
        }
        if (responsive_seek_input("Seek index", &video_seek_target)) {
            try { video_player.seek(video_seek_target); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        if (ImGui::Button("Seek to frame", ImVec2(-1, 0))) {
            try { video_player.seek(video_seek_target); }
            catch (const std::exception& e) { video_status = e.what(); }
        }
        float default_playback_fps = 30.0F;
        if (metadata && metadata->frames_per_second &&
            std::isfinite(*metadata->frames_per_second) &&
            *metadata->frames_per_second >= 1.0 && *metadata->frames_per_second <= 120.0)
            default_playback_fps = static_cast<float>(*metadata->frames_per_second);
        if (ImGui::Checkbox("Use Source FPS", &video_use_source_fps)) {
            if (video_use_source_fps) {
                video_player.clear_playback_fps_override();
            } else {
                // Start manual mode at the source's current effective FPS,
                // with no unexpected speed jump on the first toggle.
                video_manual_playback_fps = static_cast<float>(video_player.playback_fps());
                video_player.set_playback_fps(video_manual_playback_fps);
            }
        }
        float playback_rate = video_use_source_fps
            ? static_cast<float>(video_player.playback_fps()) : video_manual_playback_fps;
        ImGui::BeginDisabled(video_use_source_fps);
        if (slider_float("Playback FPS", &playback_rate, 1.0f, 120.0f,
                         default_playback_fps, "%.1f")) {
            video_manual_playback_fps = playback_rate;
            video_player.set_playback_fps(static_cast<double>(video_manual_playback_fps));
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Source mode follows media timestamps; manual FPS overrides playback pacing.");
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
        ImGui::TextUnformatted("RECORDED-VIDEO DETECTION / PEOPLE (M7.5)");
        if (responsive_combo("Video detection profile", &video_detection_profile,
                "COCO-80 objects\0Person only (COCO ID 0)\0")) {
            video_detection.cancel();
            video_export_worker.request_stop();
        }
        if (video_detection_profile == 1) {
            if (slider_float("Person display threshold", &video_person_display_threshold,
                             0.0F, 1.0F,
                             static_cast<float>(slider_ui_default("video_person_display_threshold", 0.50)), "%.2f")) {
                video_detection.cancel();
                video_export_worker.request_stop();
            }
            ImGui::TextDisabled("A person must pass BOTH its confidence and display thresholds.");
            ImGui::TextDisabled("Other classes are excluded from boxes AND weak-candidate diagnostics.");
        }
        ImGui::TextWrapped("NanoDet COCO-80 CPU (416 x 416). Model weights are not bundled. "
                           "Only the 80 listed classes can be named. Unsupported objects "
                           "may be missed or misidentified; scores are not calibrated probabilities.");
        if (ImGui::CollapsingHeader("Video supported COCO classes (80)")) {
            for (int id = 0; id < chromascope::kNanoDetClassCount; ++id)
                ImGui::Text("%02d  %s", id,
                    std::string(chromascope::nanodet_class_label(id)).c_str());
        }
        responsive_input_text("ONNX model", detector_model_path.data(),
                         detector_model_path.size());
        if (ImGui::IsItemEdited()) {
            video_detection.cancel();
            video_export_worker.request_stop();
        }
        if (ImGui::Button("Browse detector model...", ImVec2(-1, 0)))
            open_file_picker(false, true);
        float confidence = static_cast<float>(detector_options.confidence_threshold);
        bool video_policy_changed = false;
        if (slider_float("Video confidence", &confidence, 0.0f, 1.0f,
                         static_cast<float>(chromascope::DnnGuiOptions{}.confidence_threshold), "%.2f")) {
            detector_options.confidence_threshold = confidence;
            video_policy_changed = true;
        }
        float iou = static_cast<float>(detector_options.nms_iou_threshold);
        if (slider_float("Video NMS IoU", &iou, 0.0f, 1.0f,
                         static_cast<float>(chromascope::DnnGuiOptions{}.nms_iou_threshold), "%.2f")) {
            detector_options.nms_iou_threshold = iou;
            video_policy_changed = true;
        }
        video_policy_changed |= slider_int("Video max boxes", &detector_options.max_detections, 1, 512,
                                             chromascope::DnnGuiOptions{}.max_detections);
        video_policy_changed |= slider_float("Video weak-candidate floor", &video_uncertainty_floor,
                                               0.0F, 1.0F, 0.35F, "%.2f");
        ImGui::TextDisabled("Diagnostic only: weak scores do not become boxes or an unknown class.");
        if (ImGui::CollapsingHeader("Video per-class confidence")) {
            slider_int("Video class ID", &detector_selected_class, 0,
                       chromascope::kNanoDetClassCount - 1, 0);
            ImGui::Text("COCO: %s", std::string(chromascope::nanodet_class_label(
                detector_selected_class)).c_str());
            auto& threshold = detector_class_overrides[
                static_cast<std::size_t>(detector_selected_class)];
            bool enabled = threshold.has_value();
            if (ImGui::Checkbox("Override this class", &enabled)) {
                if (enabled) threshold = 0.75F;
                else threshold.reset();
                video_policy_changed = true;
            }
            if (threshold) {
                float value = *threshold;
                if (slider_float("Video class confidence", &value, 0.0F, 1.0F, 0.75F, "%.2f")) {
                    threshold = value;
                    video_policy_changed = true;
                }
            }
            ImGui::TextDisabled("Changes apply to the next analysis or AVI export.");
        }
        if (video_policy_changed) {
            video_detection.cancel();
            video_export_worker.request_stop();
            // This confidence/class policy is also used by still-image DNN.
            detector_session.clear_results();
        }
        std::size_t active_overrides = 0;
        for (const auto& item : detector_class_overrides)
            if (item) ++active_overrides;
        ImGui::TextDisabled("%zu per-class override(s) configured (next run).", active_overrides);
        if (video_detection_profile == 1)
            ImGui::TextDisabled("Only the person (ID 0) override affects person-only results.");
        ImGui::Checkbox("Show matched-frame overlays", &video_show_detections);
        if (video_detection_profile == 1) {
            ImGui::TextDisabled("Person overlays: source-space boxes + confidence; cached frames only.");
            ImGui::TextDisabled("Show/Hide is presentation-only, not a different detection result.");
        }
        ImGui::BeginDisabled(!has_source);
        if (ImGui::Button("Analyze recording", ImVec2(-1, 0))) start_video_detection();
        ImGui::EndDisabled();
        const auto progress = video_detection.snapshot();
        const bool working = progress.stage == chromascope::VideoDetectionStage::Processing ||
                             progress.stage == chromascope::VideoDetectionStage::Queued;
        if (working && ImGui::Button("Cancel video detection", ImVec2(-1, 0)))
            video_detection.cancel();
        if (progress.reported_total_frames && *progress.reported_total_frames > 0) {
            const float fraction = std::clamp(static_cast<float>(progress.processed_frames) /
                static_cast<float>(*progress.reported_total_frames), 0.0f, 1.0f);
            ImGui::ProgressBar(fraction, ImVec2(-1, 0));
            ImGui::Text("Processed: %llu / %llu frames",
                static_cast<unsigned long long>(progress.processed_frames),
                static_cast<unsigned long long>(*progress.reported_total_frames));
        } else {
            ImGui::Text("Processed: %llu frames (total unknown)",
                static_cast<unsigned long long>(progress.processed_frames));
        }
        ImGui::TextWrapped("%s", progress.message.c_str());
        if (progress.confidence.observed_frames != 0) {
            ImGui::Text("Weak candidates: %llu across %llu processed frames",
                static_cast<unsigned long long>(progress.confidence.weak_candidates),
                static_cast<unsigned long long>(progress.confidence.observed_frames));
            ImGui::Text("Frames with weak predictions but no accepted boxes: %llu",
                static_cast<unsigned long long>(progress.confidence.uncertain_only_frames));
            ImGui::TextDisabled("These are candidate/frame counts, not tracked unknown objects.");
        }
        ImGui::TextDisabled("Recent detection cache: %zu / %zu frames",
            progress.cached_frames, chromascope::AsyncRecordedVideoDetection::kMaxCachedFrames);
        if (progress.performance.decode.measured() && progress.performance.inference.measured()) {
            ImGui::Text("Analysis: decode %.3f ms/frame | inference %.3f ms/frame",
                progress.performance.decode.mean_ms(), progress.performance.inference.mean_ms());
            ImGui::TextDisabled("Completed samples: %llu (decode), %llu (inference)",
                static_cast<unsigned long long>(progress.performance.decode.samples),
                static_cast<unsigned long long>(progress.performance.inference.samples));
        }
        if (video_overlay_draw_perf.measured())
            ImGui::Text("GUI overlay submission: %.3f ms/draw (%llu repaint samples)",
                video_overlay_draw_perf.mean_ms(),
                static_cast<unsigned long long>(video_overlay_draw_perf.samples));
        ImGui::TextDisabled("Decoder and NanoDet run off-thread; cached overlays only.");
        ImGui::TextDisabled("Frames not yet processed or evicted show no overlay.");
        ImGui::Separator();
        ImGui::TextWrapped("%s", video_player.status().c_str());
        if (!video_status.empty()) ImGui::TextWrapped("%s", video_status.c_str());
        ImGui::EndChild();
        next_panel(layout);
        ImGui::BeginChild("Video preview", ImVec2(0, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextUnformatted("VIDEO PREVIEW");
        ImGui::SameLine();
        if (ImGui::SmallButton("Fit video")) video_view.fit();
        ImGui::SameLine();
        ImGui::TextDisabled("Wheel: zoom | Middle/right drag: pan | Double-click: fit");
        ImGui::Separator();
        if (video_texture.id && current) {
            std::shared_ptr<const chromascope::RecordedVideoObjectDetection> matched;
            if (video_show_detections && current)
                matched = video_detection.overlay_for(*current);
            if (matched) {
                ImGui::Text("Matched frame %llu: %zu accepted %s (%.1f ms)",
                    static_cast<unsigned long long>(matched->frame_index),
                    matched->inference.detections.objects().size(),
                    video_detection_profile == 1 ? "people" : "objects", matched->elapsed_ms);
                ImGui::Text("Confidence status: %s | weak candidates: %zu",
                    std::string(matched->inference.summary.status()).c_str(),
                    matched->inference.summary.weak_candidates);
                if (matched->inference.summary.status() == "uncertain")
                    ImGui::TextDisabled("Weak predictions rejected; no confirmed unknown object.");
            } else {
                ImGui::TextDisabled("No cached detection for the current frame.");
            }
            draw_viewport_texture("##video_canvas", video_texture, video_view,
                matched ? &matched->inference.detections : nullptr);
        } else ImGui::TextWrapped("Open a video. The first decoded frame appears here.");
        ImGui::EndChild();
        if (browse_requested) {
            ImGui::OpenPopup("Select media");
            browse_requested = false;
        }
        draw_file_picker();
    }

    void draw_camera_ui(bool remote = false) {
        const auto layout = panel_layout();
        ImGui::BeginChild(remote ? "Stream controls" : "Camera controls",
                          panel_controls_size(layout), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(remote ? "LIVE STREAMED VIDEO (HTTPS)" : "LOCAL LIVE CAMERA");
        if (remote)
            ImGui::TextWrapped("Explicit opt-in only. No scripts, cookies, redirects,"
                " webpage embeds, tracking integrations or local-camera access."
                " Public media servers will see your public IP address."
                " No stream URL is saved in Preferences.");
        else
            ImGui::TextWrapped("Camera and CPU detection are separately opt-in. "
                "Nothing is recorded, uploaded, or inferred automatically.");
        ImGui::Separator();
        const auto current_state = camera_capture.state();
        const auto badge = chromascope::camera_preview_badge(current_state);
        const ImVec4 color = badge.may_be_active
            ? ImVec4(1.0F, 0.40F, 0.36F, 1.0F)
            : ImVec4(0.52F, 0.78F, 0.62F, 1.0F);
        ImGui::TextColored(color, "%s", remote ? (badge.may_be_active ? "STREAM ACTIVE" : "STREAM OFF") : badge.label);
        if (remote && camera_status == "Camera is OFF. Select a device and press Start camera.")
            ImGui::TextWrapped("Stream is OFF. Paste a direct public HTTPS media URL and press Start stream.");
        else ImGui::TextWrapped("%s", camera_status.c_str());
        if (current_state == chromascope::CameraCaptureState::Stopping ||
            current_state == chromascope::CameraCaptureState::Failed) {
            ImGui::TextWrapped(remote ? "A remote TLS request may need to finish or time out before the reader closes."
                                      : "Driver I/O can block. The camera may remain active "
                                        "until the worker confirms device release.");
        }
        ImGui::Separator();
        const bool capturing = current_state == chromascope::CameraCaptureState::Capturing;
        if (remote) {
            ImGui::TextUnformatted("PUBLIC HTTPS FEED URL");
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::InputTextWithHint("##public_stream_url", "https://public.example.org/feed.m3u8",
                                     public_stream_url.data(), public_stream_url.size());
            responsive_combo("Direct media type", &public_stream_kind,
                "Automatic (by URL extension)\0JPEG/PNG/MJPEG snapshot\0"
                "HLS MPEG-TS (.m3u8)\0Bounded MP4\0");
            ImGui::TextUnformatted("DOWNLOADED HLS MASTER (OPTIONAL)");
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::InputTextWithHint("##public_stream_playlist_file", "Absolute path to playlist.m3u8",
                                     public_stream_playlist_file.data(), public_stream_playlist_file.size());
            if (ImGui::Button("Browse downloaded playlist...", ImVec2(-1, 0)))
                open_file_picker(false, false, true);
            ImGui::SetItemTooltip("The original HTTPS feed URL is still required to resolve"
                " the downloaded playlist's relative references. No file is uploaded.");
            ImGui::TextWrapped("No iframe pages or scripts. Same-origin, unencrypted"
                " HLS MPEG-TS only. Single-variant master playlists are supported."
                " Downloaded playlists must be paired with their original HTTPS URL."
                " Encrypted or cross-origin resources remain blocked.");
            const bool opening = current_state == chromascope::CameraCaptureState::Starting;
            const bool capturing = current_state == chromascope::CameraCaptureState::Capturing;
            const bool stopping = current_state == chromascope::CameraCaptureState::Stopping;
            ImGui::BeginDisabled(opening || capturing || stopping || public_stream_url[0] == '\0');
            if (ImGui::Button("Start stream", ImVec2(-1, 0))) start_public_stream();
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!opening && !capturing);
            if (ImGui::Button("Stop stream", ImVec2(-1, 0))) stop_camera();
            ImGui::EndDisabled();
            ImGui::TextDisabled("HTTPS requests stop when leaving this mode."
                " A pending network read may complete before Stop finishes.");
        } else {
        ImGui::TextUnformatted("CAMERA BACKEND");
        const auto backends = chromascope::camera_backend_info();
        const auto backend_name = chromascope::camera_backend_name(camera_backend);
        ImGui::BeginDisabled(current_state == chromascope::CameraCaptureState::Starting ||
                             current_state == chromascope::CameraCaptureState::Capturing ||
                             current_state == chromascope::CameraCaptureState::Stopping);
        if (ImGui::BeginCombo("##camera_backend", backend_name.data())) {
            const bool is_default = camera_backend == chromascope::CameraBackend::PlatformDefault;
            if (ImGui::Selectable("System default (explicit native backend)", is_default))
                camera_backend = chromascope::CameraBackend::PlatformDefault;
            for (const auto& backend : backends) {
                ImGui::BeginDisabled(!backend.supported_on_host || !backend.registered_for_camera);
                if (ImGui::Selectable(backend.name.data(), camera_backend == backend.backend))
                    camera_backend = backend.backend;
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("V4L2 on Linux; MSMF on Windows; DShow is explicit only.");
        if (ImGui::Button("List devices (no opening)", ImVec2(-1, 0))) {
            try {
                camera_devices = chromascope::discover_cameras(
                    {.backend = camera_backend, .max_indices = 16, .allow_device_open = false});
            } catch (const std::exception& error) {
                camera_devices = {};
                camera_status = std::string("Passive camera listing failed: ") + error.what();
            }
        }
        if (!camera_devices.notice.empty())
            ImGui::TextWrapped("%s", camera_devices.notice.c_str());
        for (const auto& device : camera_devices.devices) {
            const std::string label = device.label + " (index " +
                std::to_string(device.index) + ")##camera_device_" +
                std::to_string(device.index);
            if (ImGui::Selectable(label.c_str(), camera_index == device.index))
                camera_index = device.index;
        }
        ImGui::TextDisabled("Discovery is unverified. Windows: enter an index manually.");
        responsive_input_int("Device index", &camera_index);
        ImGui::TextDisabled("OpenCV device index, 0 to 255; indices are backend-specific.");
        ImGui::TextDisabled("Format: driver-selected (M8.2 strict format API retained).");
        const bool opening = current_state == chromascope::CameraCaptureState::Starting;
        const bool capturing = current_state == chromascope::CameraCaptureState::Capturing;
        const bool stopping = current_state == chromascope::CameraCaptureState::Stopping;
        ImGui::BeginDisabled(opening || capturing || stopping || camera_index < 0 || camera_index > 255);
        if (ImGui::Button("Start camera", ImVec2(-1, 0))) start_camera();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!opening && !capturing);
        if (ImGui::Button("Stop camera", ImVec2(-1, 0))) stop_camera();
        ImGui::EndDisabled();
        } // local camera controls versus private HTTPS stream controls
        ImGui::Separator();
        ImGui::TextUnformatted("LIVE RECORDING (OPT-IN)");
        ImGui::TextDisabled("Destination and AVI/MP4 format: Export menu.");
        draw_live_record_controls(false);
        ImGui::Separator();
        ImGui::TextUnformatted("LIVE OBJECT / PERSON DETECTION (OPT-IN)");
        ImGui::TextWrapped("Only pinned local NanoDet COCO-80 inference. "
                           "Person mode detects class 0; optional temporary box tracking is NOT identification.");
        ImGui::BeginDisabled(live_inference_active);
        responsive_combo("Live detection profile", &live_detection_profile,
            "COCO-80 objects\0Person only (COCO ID 0)\0");
        slider_float("Live confidence cutoff", &live_confidence_threshold,
            0.01F, 1.0F, static_cast<float>(slider_ui_default("live_confidence_threshold", 0.50)), "%.2f");
        if (live_detection_profile == 1)
            slider_float("Live person display cutoff", &live_person_display_threshold,
                0.01F, 1.0F, static_cast<float>(slider_ui_default("live_person_display_threshold", 0.50)), "%.2f");
        responsive_combo("Inference frame-drop policy", &live_drop_strategy,
            "Keep newest (replace pending)\0Keep oldest (discard incoming)\0");
        slider_int("Minimum capture spacing (ms)",
            &live_min_capture_spacing_ms, 0, 1000,
            static_cast<int>(slider_ui_default("live_min_capture_spacing_ms", 0)));
        slider_int("Max inference submission FPS (0=uncapped)",
            &live_max_inference_fps, 0, 60,
            static_cast<int>(slider_ui_default("live_max_inference_fps", 0)));
        responsive_input_text("Local ONNX model", detector_model_path.data(), detector_model_path.size());
        ImGui::Checkbox("Enable temporary track IDs", &live_enable_tracking);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(live_inference_active || !capturing);
        if (ImGui::Button("Start live inference", ImVec2(-1, 0))) start_live_inference();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!live_inference_active);
        if (ImGui::Button("Stop live inference", ImVec2(-1, 0))) {
            cancel_live_inference();
            camera_status = "Live inference stopped; camera preview remains on.";
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!live_inference_active || !capturing);
        if (ImGui::Button(live_paused ? "Resume live inference" : "Pause live inference",
                          ImVec2(-1, 0))) toggle_live_pause();
        ImGui::EndDisabled();
        slider_float("Live overlay display confidence", &live_display_confidence_threshold,
                         0.01F, 1.0F,
                         static_cast<float>(slider_ui_default("live_display_confidence_threshold", 0.50)), "%.2f");
        ImGui::TextDisabled("Display-only threshold; lower values cannot recover model-filtered boxes.");
        ImGui::TextDisabled("FPS cap limits inference submissions, not the live source frame rate.");
        const auto live_state = live_inference.snapshot();
        ImGui::TextWrapped("%s", live_state.message.c_str());
        ImGui::Text("Inference accepted: %llu | analyzed: %llu",
            static_cast<unsigned long long>(live_state.accepted_frames),
            static_cast<unsigned long long>(live_state.processed_frames));
        ImGui::Text("Dropped: replaced=%llu | busy=%llu | paced=%llu | stale=%llu",
            static_cast<unsigned long long>(live_state.replaced_pending_frames),
            static_cast<unsigned long long>(live_state.dropped_busy_frames),
            static_cast<unsigned long long>(live_state.dropped_pacing_frames),
            static_cast<unsigned long long>(live_state.dropped_stale_frames));
        ImGui::Text("Inference state: %s | effective minimum spacing: %lld ms",
            live_state.paused ? "PAUSED" : (live_inference_active ? "ACTIVE" : "OFF"),
            static_cast<long long>(live_state.scheduling.minimum_capture_spacing.count()));
        ImGui::Text("Scheduler: %s | pending: %s | CPU forward: %s",
            live_state.scheduling.drop_strategy == chromascope::LiveInferenceDropStrategy::KeepNewest
                ? "Keep newest" : "Keep oldest",
            live_state.has_pending_frame ? "yes" : "no",
            live_state.inference_busy ? "busy" : "idle");
        ImGui::Checkbox("Show live bounding boxes", &live_show_bounding_boxes);
        ImGui::BeginDisabled(!live_show_bounding_boxes);
        ImGui::Checkbox("Show live confidence labels", &live_show_confidence_labels);
        ImGui::EndDisabled();
        ImGui::TextDisabled("Labels report model confidence (not calibrated probability).");
        ImGui::TextDisabled("Track IDs only match nearby class-consistent boxes in this session; not identity or re-ID.");
        if (live_enable_tracking && live_track_frame && live_inference_active)
            ImGui::Text("Temporary tracks: %zu active | %zu matched | %zu new",
                live_track_frame->active_tracks, live_track_frame->matched_tracks,
                live_track_frame->created_tracks);
        ImGui::TextDisabled("One pending frame + one worker. Spacing uses source capture timestamps.");
        ImGui::TextDisabled("In-flight CPU forward cannot be forcibly interrupted.");
        ImGui::Separator();
        ImGui::Text("Completed frames dropped: %llu",
            static_cast<unsigned long long>(camera_capture.dropped_frames()));
        ImGui::Text("Worker queue: %zu frame(s), %zu bytes",
            camera_capture.queued_frames(), camera_capture.queued_pixel_bytes());
        if (camera_frame_visible) {
            ImGui::Text("Frame %llu | Capture monotonic %.3f s",
                static_cast<unsigned long long>(camera_frame_index),
                static_cast<double>(camera_frame_time.count()) / 1'000'000'000.0);
        }
        if (camera_active_format.width > 0 && camera_active_format.height > 0)
            ImGui::Text("Negotiated: %d x %d | reported %.1f fps",
                camera_active_format.width, camera_active_format.height, camera_active_format.fps);
        ImGui::TextWrapped(remote ? "Stop or leave this mode to cancel new requests and clear pixels."
                                 : "Stop or switch workspaces to request device release. "
                                   "Session reset also stops the preview and clears pixels.");
        ImGui::EndChild();
        next_panel(layout);
        ImGui::BeginChild(remote ? "Live stream preview" : "Live camera preview", ImVec2(0, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::TextUnformatted(remote ? "LIVE STREAM PREVIEW" : "LIVE PREVIEW");
        ImGui::SameLine();
        const auto preview_badge = chromascope::camera_preview_badge(camera_capture.state());
        ImGui::TextColored(preview_badge.may_be_active
            ? ImVec4(1.0F, 0.40F, 0.36F, 1.0F)
            : ImVec4(0.52F, 0.78F, 0.62F, 1.0F), "%s", remote ?
                (preview_badge.may_be_active ? "STREAM ACTIVE" : "STREAM OFF") : preview_badge.label);
        ImGui::SameLine();
        if (ImGui::SmallButton(remote ? "Fit stream" : "Fit camera")) camera_view.fit();
        ImGui::Separator();
        if (live_inference_active && live_displayed) {
            if (live_paused) ImGui::TextDisabled("PAUSED: last analyzed frame frozen; camera remains active.");
            ImGui::Text("Analyzed frame %llu | %.1f ms | %zu %s | %s",
                static_cast<unsigned long long>(live_displayed->frame.index),
                live_displayed->elapsed_ms,
                live_displayed->inference.detections.objects().size(),
                live_detection_profile == 1 ? "people" : "objects",
                std::string(live_displayed->inference.summary.status()).c_str());
            ImGui::TextDisabled("Preview shows analyzed pixels (not the newest captured frame).");
        }
        // M9.3: fail closed on any session, generation, timestamp/domain,
        // frame-index or source-geometry mismatch. Never paint a cached result
        // over pixels from another live camera frame or inference session.
        const chromascope::DetectionResult* synchronized_boxes = nullptr;
        if (live_show_bounding_boxes && live_inference_active && live_displayed &&
            live_state.stage == chromascope::LiveInferenceStage::Running &&
            live_state.camera_generation == camera_generation && camera_frame_visible) {
            const auto& frame = live_displayed->frame;
            const auto stamp = frame.image.metadata().timestamp;
            const auto result_stamp = live_displayed->inference.detections.timestamp();
            if (stamp && result_stamp &&
                stamp->domain == chromascope::TimestampDomain::CaptureMonotonic &&
                result_stamp->domain == chromascope::TimestampDomain::CaptureMonotonic &&
                result_stamp->value == stamp->value &&
                live_displayed->inference.detections.width() == frame.image.width() &&
                live_displayed->inference.detections.height() == frame.image.height() &&
                chromascope::live_overlay_matches(
                    {camera_generation, live_state.session, camera_frame_index,
                     camera_texture.source_width, camera_texture.source_height, camera_frame_time},
                    {live_displayed->camera_generation, live_displayed->session,
                     frame.index, frame.image.width(), frame.image.height(), stamp->value}))
                synchronized_boxes = &live_displayed->inference.detections;
        }
        const std::vector<std::uint64_t>* synchronized_track_ids = nullptr;
        if (synchronized_boxes && live_enable_tracking && live_track_frame && live_displayed &&
            chromascope::live_overlay_matches(
                {camera_generation, live_state.session, camera_frame_index,
                 camera_texture.source_width, camera_texture.source_height, camera_frame_time},
                live_track_frame->key))
            synchronized_track_ids = &live_track_frame->detection_ids;
        if (camera_frame_visible && camera_texture.id &&
            camera_capture.state() == chromascope::CameraCaptureState::Capturing)
            draw_viewport_texture("##camera_canvas", camera_texture, camera_view,
                synchronized_boxes, live_show_confidence_labels, true, synchronized_track_ids,
                static_cast<double>(live_display_confidence_threshold));
        else
            ImGui::TextWrapped(remote ? "No decoded stream frame. Paste a public HTTPS media URL and press Start stream."
                                      : "No live camera frame. Choose a camera and press Start camera. "
                                        "An OS permission prompt may appear.");
        ImGui::EndChild();
        if (remote) {
            if (browse_requested) {
                ImGui::OpenPopup("Select media");
                browse_requested = false;
            }
            draw_file_picker();
        }
    }

    void draw_ui() {
        handle_shortcuts();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("ChromascopeIP Workspace", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // v1.0.1 UI polish: two compact action rows with menu-style
        // Preferences and Export entries and dedicated modal dialogs.
        // Narrow windows wrap controls as needed; behavior is unchanged.
        const ImGuiStyle& toolbar_style = ImGui::GetStyle();
        const float toolbar_right = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x -
            toolbar_style.WindowPadding.x;
        const auto text_width = [](const char* label) {
            return ImGui::CalcTextSize(label).x;
        };
        const auto radio_width = [&](const char* label) {
            return ImGui::GetFrameHeight() + toolbar_style.ItemInnerSpacing.x + text_width(label);
        };
        const auto small_button_width = [&](const char* label) {
            // Conservative estimate also permits spacing at non-default DPI.
            return text_width(label) + 2.0F * toolbar_style.FramePadding.x;
        };
        const auto same_line_if_fits = [&](float next_width) {
            if (ImGui::GetItemRectMax().x + toolbar_style.ItemSpacing.x + next_width > toolbar_right)
                return false;
            ImGui::SameLine();
            return true;
        };
        const auto toolbar_divider_if_fits = [&](float next_width) {
            const float divider_width = text_width("|") +
                2.0F * toolbar_style.ItemSpacing.x;
            if (same_line_if_fits(divider_width + next_width)) {
                ImGui::TextDisabled("|");
                ImGui::SameLine();
            }
        };

        ImGui::TextColored(ImVec4(0.44f, 0.79f, 0.96f, 1.0f), "ChromascopeIP");
        ImGui::SameLine();
        ImGui::TextDisabled("v%s", CHROMASCOPE_RELEASE_VERSION);
        toolbar_divider_if_fits(small_button_width("Preferences"));
        // A text-highlighted selectable looks like a traditional menu entry,
        // but opens a modal rather than an additional dropdown/menu bar.
        if (ImGui::Selectable("Preferences", false, ImGuiSelectableFlags_None,
                              ImVec2(small_button_width("Preferences"), ImGui::GetFrameHeight())))
            preferences_requested = true;
        toolbar_divider_if_fits(small_button_width("Export"));
        if (ImGui::Selectable("Export", false, ImGuiSelectableFlags_None,
                              ImVec2(small_button_width("Export"), ImGui::GetFrameHeight())))
            export_requested = true;
        toolbar_divider_if_fits(radio_width("Still Images"));
        if (ImGui::RadioButton("Still Images", workspace_mode == 0))
            switch_workspace(0);
        same_line_if_fits(radio_width("Recorded Video"));
        if (ImGui::RadioButton("Recorded Video", workspace_mode == 1))
            switch_workspace(1);
        same_line_if_fits(radio_width("Live Camera"));
        if (ImGui::RadioButton("Live Camera", workspace_mode == 2))
            switch_workspace(2); // no implicit device opening
        same_line_if_fits(radio_width("Live Streamed Video"));
        if (ImGui::RadioButton("Live Streamed Video", workspace_mode == 3))
            switch_workspace(3); // NO implicit DNS query or network access
        const auto camera_badge = chromascope::camera_preview_badge(camera_capture.state());
        same_line_if_fits(text_width(camera_badge.label));
        ImGui::TextColored(camera_badge.may_be_active
            ? ImVec4(1.0F, 0.40F, 0.36F, 1.0F)
            : ImVec4(0.52F, 0.78F, 0.62F, 1.0F), "%s",
            workspace_mode == 3 ? (camera_badge.may_be_active ? "STREAM ACTIVE" : "STREAM OFF") : camera_badge.label);
        ImGui::SetItemTooltip("The selected live source opens only after an explicit Start action. Leaving a live workspace requests Stop.");
        const auto record_state = live_recorder.status().stage;
        const bool recording_now = record_state == chromascope::LiveRecordingStage::Starting ||
            record_state == chromascope::LiveRecordingStage::Recording;
        const bool saving_now = record_state == chromascope::LiveRecordingStage::Finalizing;
        const char* recording_label = recording_now ? "RECORDING" :
            (saving_now ? "SAVING" : "NOT RECORDING");
        same_line_if_fits(text_width(recording_label));
        ImGui::TextColored(recording_now ? ImVec4(1.0F, 0.40F, 0.36F, 1.0F) :
            (saving_now ? ImVec4(0.98F, 0.75F, 0.30F, 1.0F) :
                ImVec4(0.52F, 0.78F, 0.62F, 1.0F)), "%s", recording_label);
        ImGui::SetItemTooltip("Recording starts only after an explicit Start recording action.");
        toolbar_divider_if_fits(small_button_width("Help (F1)"));
        if (ImGui::Selectable("Help (F1)", false, ImGuiSelectableFlags_None,
                              ImVec2(small_button_width("Help (F1)"), ImGui::GetFrameHeight())))
            help_requested = true;

        // Second row: session clearing precedes both people-workflow shortcuts.
        // It invokes the exact same explicit session-clearing action as before.
        // The normal ImGui vertical layout starts the next row automatically;
        // NewLine here created an unintended empty row above these actions.
        if (ImGui::Button("Clear loaded media, model and results"))
            clear_private_session();
        ImGui::SetItemTooltip("Clearing cancels pending work and releases displayed\n"
            "resources, but in-flight OpenCV work can finish.\n"
            "Previously saved files, terminal output and OS caches remain.");

        // M7.5: same person quick-entry actions and status, now on row two.
        // These only select policies; no model or camera is loaded implicitly.
        toolbar_divider_if_fits(small_button_width("People in image"));
        if (ImGui::Button("People in image")) {
            switch_workspace(0);
            show_overlay = true;
            show_labels = true;
            if (detector_profile == 0) {
                detector_profile = 1;
                detector_session.clear_results();
            }
        }
        same_line_if_fits(small_button_width("People in video"));
        if (ImGui::Button("People in video")) {
            switch_workspace(1);
            video_show_detections = true;
            if (video_detection_profile != 1) {
                video_detection.cancel();
                video_export_worker.request_stop();
                video_detection_profile = 1;
            }
        }
        char person_status[128]{};
        if (workspace_mode == 1 && video_detection_profile == 1) {
            std::snprintf(person_status, sizeof(person_status), "PERSON ONLY | COCO 0 | cutoff >= %.2f",
                static_cast<double>(std::max({video_person_display_threshold,
                    static_cast<float>(detector_options.confidence_threshold),
                    detector_class_overrides[0].value_or(0.0F)})));
        } else if (workspace_mode == 0 && detector_profile != 0) {
            std::snprintf(person_status, sizeof(person_status), "STILL PEOPLE | COCO 0 | cutoff >= %.2f",
                static_cast<double>(std::max({still_person_display_threshold,
                    static_cast<float>(detector_options.confidence_threshold),
                    detector_class_overrides[0].value_or(0.0F)})));
        } else {
            std::snprintf(person_status, sizeof(person_status),
                "Select a people workflow; no automatic model loading");
        }
        same_line_if_fits(text_width(person_status));
        ImGui::TextDisabled("%s", person_status);
        draw_help();
        draw_preferences();
        draw_export();
        ImGui::Separator();
        if (workspace_mode == 1) {
            draw_video_ui();
            ImGui::End();
            return;
        }
        if (workspace_mode == 2 || workspace_mode == 3) {
            draw_camera_ui(workspace_mode == 3);
            ImGui::End();
            return;
        }

        const auto layout = panel_layout();
        ImGui::BeginChild("Controls", panel_controls_size(layout), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("INPUT IMAGE");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##path", path.data(), path.size());
        const float image_file_button_width = control_button_width(2);
        if (ImGui::Button("Open image", ImVec2(image_file_button_width, 0.0F)))
            load(path.data());
        ImGui::SetItemTooltip("Open the local image at the absolute path entered above.");
        ImGui::SameLine();
        if (ImGui::Button("Browse...", ImVec2(-1.0F, 0.0F))) open_file_picker();
        ImGui::SetItemTooltip("Browse folders to select a local image file "
                              "instead of typing or pasting its absolute path.");
        ImGui::Spacing();
        ImGui::TextUnformatted("ORIGINAL SKITTLES SAMPLES");
        if (!legacy_sample_directory)
            ImGui::TextDisabled("Samples are not redistributed; browse for your own media.");
        ImGui::BeginDisabled(!legacy_sample_directory.has_value());
        // Keep all four samples equally sized while the left splitter resizes.
        // The final button consumes remaining width to avoid rounding gaps.
        const float sample_spacing = ImGui::GetStyle().ItemSpacing.x;
        const float sample_width = std::max(1.0F,
            (ImGui::GetContentRegionAvail().x - 3.0F * sample_spacing) / 4.0F);
        for (int n = 1; n <= 4; ++n) {
            const std::string button = "s" + std::to_string(n) + ".jpg";
            if (ImGui::Button(button.c_str(), ImVec2(n == 4 ? -1.0F : sample_width, 0.0F))) {
                load(*legacy_sample_directory / button);
            }
            if (n != 4) ImGui::SameLine();
        }
        ImGui::EndDisabled();
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
            if (slider_float("Circle threshold", &accumulator, 18.0f, 55.0f,
                             static_cast<float>(slider_profile_default("skittles", "accumulator_threshold",
                                                                          chromascope::Settings{}.accumulator_threshold)), "%.0f"))
                settings.accumulator_threshold = accumulator;
            ImGui::SetItemTooltip("Lower values: more candidates; more false positives.");
            float minimum_confidence = static_cast<float>(settings.minimum_confidence);
            if (slider_float("Color confidence", &minimum_confidence, 0.0f, 0.95f,
                             static_cast<float>(slider_profile_default("skittles", "minimum_confidence",
                                                                          chromascope::Settings{}.minimum_confidence)), "%.2f"))
                settings.minimum_confidence = minimum_confidence;
            int max_size = settings.max_dimension;
            if (slider_int("Analysis max size", &max_size, 512, 2000,
                           static_cast<int>(slider_profile_default("skittles", "max_dimension",
                                                                           chromascope::Settings{}.max_dimension))))
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
                        if (slider_int(label.c_str(), &current,
                                       static_cast<int>(option.minimum),
                                       static_cast<int>(option.maximum),
                                       static_cast<int>(slider_profile_default(selected_profile,
                                                                              option.key, option.default_value)))) {
                            if ((option.key == "kernel_size" || option.key == "morph_kernel" ||
                                 option.key == "blur_kernel") && current % 2 == 0)
                                current = current < static_cast<int>(options.at(option.key))
                                    ? current - 1 : current + 1;
                            options.at(option.key) = current;
                        }
                    } else {
                        float current = static_cast<float>(options.at(option.key));
                        if (slider_float(label.c_str(), &current,
                                       static_cast<float>(option.minimum),
                                       static_cast<float>(option.maximum),
                                       static_cast<float>(slider_profile_default(selected_profile,
                                                                                option.key, option.default_value)), "%.1f"))
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

        ImGui::TextUnformatted("VIEW");
        if (responsive_combo("Preview mode", &preview_mode,
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
                if (ImGui::BeginChild("Geometry items", ImVec2(0.0F, 120.0F * ui_content_scale),
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
        next_panel(layout);

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
        const auto person_snapshot = detector_session.snapshot();
        const chromascope::DetectionResult* still_people =
            detector_profile != 0 && show_overlay &&
            person_snapshot.stage == chromascope::DnnGuiStage::Completed &&
            person_snapshot.person_detections
            ? person_snapshot.person_detections.get() : nullptr;
        if (!original_texture.id) {
            ImGui::TextWrapped("Browse for a local image to begin. Historical sample images "
                               "are only available in authorized development checkouts.");
        } else if (preview_mode == 2 && have_result) {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float pane_width = std::max(1.0f,
                (available.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
            ImGui::BeginChild("Original pane", ImVec2(pane_width, 0),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextUnformatted("Original (unmodified)");
            const bool original_changed = draw_viewport_texture(
                "##original_canvas", original_texture, comparison_original_view,
                still_people, show_labels);
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
            draw_viewport_texture("##result_canvas",
                still_people || preview_mode == 1 || !have_result
                    ? original_texture : result_preview_texture(),
                result_view, still_people, show_labels);
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
    glfwSetWindowSizeLimits(window, 500, 420, GLFW_DONT_CARE, GLFW_DONT_CARE);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // M7.6: do not create imgui.ini or an ImGui log in the working directory.
    // This is a runtime default, not a promise to erase old copies or OS caches.
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().LogFilename = nullptr;
    ImGui::StyleColorsDark();
    const ImGuiStyle base_style = ImGui::GetStyle();
    float previous_scale = 0.0F;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    {
        App app;
        app.load_gui_preferences(); // local config only; never starts device or inference
        if (argc > 1) app.load(argv[1]);
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            // GLFW reports content scale for the current monitor, including
            // when a running window is moved between differently scaled screens.
            float xscale = 1.0F, yscale = 1.0F;
            glfwGetWindowContentScale(window, &xscale, &yscale);
            const float current_scale = chromascope::gui_content_scale(xscale, yscale);
            if (std::abs(current_scale - previous_scale) > 0.01F) {
                ImGui::GetStyle() = base_style; // never compound scaling each frame
                ImGui::GetStyle().ScaleAllSizes(current_scale);
                ImGui::GetStyle().FontScaleDpi = current_scale;
                previous_scale = current_scale;
            }
            app.ui_content_scale = current_scale;
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
