cmake_minimum_required(VERSION 3.24)

# Keep the source checkout free of generated artifacts and fetched dependencies.
# Existing out-of-source CMake commands remain supported alongside presets.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Run cmake --preset linux-release "
        "or configure with an explicit -B build directory outside the source root.")
endif()

project(ChromascopeIP VERSION 1.0.0 LANGUAGES CXX)

# M12.1: one authoritative release-candidate version for every frontend/package.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReleaseVersion.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/chromascope/ReleaseVersion.hpp" @ONLY)

include(CTest)
option(CHROMASCOPE_BUILD_GUI "Build the Dear ImGui application" ON)
option(CHROMASCOPE_BUILD_TESTS "Build native C++ tests" ON)
set(CHROMASCOPE_IMGUI_DIR "" CACHE PATH "Existing Dear ImGui v1.93.0 source directory (optional)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChromascopeQuality.cmake")

# A system OpenCV 4.x must NOT silently satisfy this dependency.
find_package(OpenCV 5.1.0 EXACT REQUIRED COMPONENTS core geometry imgproc imgcodecs videoio dnn)
message(STATUS "ChromascopeIP OpenCV: ${OpenCV_VERSION} at ${OpenCV_DIR}")
# An application relink does not rebuild the pre-existing OpenCV installation.
# Do not allow native tests (or a local build) to silently use a stale FFmpeg
# backend after source-only security patch updates.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChromascopeOpenCVPolicy.cmake")
    chromascope_require_linux_opencv_policy("${OpenCV_DIR}")
endif()

add_library(chromascope_core STATIC
    src/core/Detection.cpp
    src/core/ImageIO.cpp
    src/core/InstallResources.cpp
    src/core/ImageHeader.cpp
    src/core/ResourceBudget.cpp
    src/core/ImageFrame.cpp
    src/core/ImageFrameCV.cpp
    src/core/DetectionResult.cpp
    src/core/Annotation.cpp
    src/core/AnnotationBridge.cpp
    src/core/AnnotationView.cpp
    src/core/ImageViewport.cpp
    src/core/AnnotationRendererCV.cpp
    src/core/LegacyAnnotationAdapter.cpp
    src/core/DetectionResultBridge.cpp
    src/core/Processor.cpp
    src/core/SkittlesProcessor.cpp
    src/core/ImageFilters.cpp
    src/core/SegmentationProfiles.cpp
    src/core/ProcessingQueue.cpp
    src/core/VideoSource.cpp
    src/core/CameraSource.cpp
    src/core/CameraDiscovery.cpp
    src/core/AsyncCameraCapture.cpp
    src/core/PublicStreamPolicy.cpp
    src/core/PublicStreamReader.cpp
    src/core/HlsByteWindow.cpp
    src/core/LiveCameraRecording.cpp
    src/core/AsyncLiveCameraInference.cpp
    src/core/AsyncVideoDecoder.cpp
    src/core/VideoPlayer.cpp
    src/core/CpuDnnAdapter.cpp
    src/core/DnnTensorPolicy.cpp
    src/core/DnnPreprocessGeometry.cpp
    src/core/DnnPreprocessing.cpp
    src/core/DnnPostprocessing.cpp
    src/core/NanoDetDecoder.cpp
    src/core/DnnModelSha256.cpp
    src/core/DnnGuiSession.cpp
    src/core/ObjectFrameInference.cpp
    src/core/RecordedVideoObjectDetection.cpp
    src/core/RecordedVideoAnalysis.cpp
    src/core/AnnotatedVideoExport.cpp
    src/core/AsyncRecordedVideoDetection.cpp
)
find_package(Threads REQUIRED)
add_library(ChromascopeIP::Core ALIAS chromascope_core)
target_include_directories(chromascope_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(chromascope_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(chromascope_core PUBLIC ${OpenCV_LIBS} Threads::Threads)
target_compile_features(chromascope_core PUBLIC cxx_std_20)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # HTTPS is a deliberate, TLS-verified libcurl transport, NEVER FFmpeg URLs.
    find_package(CURL REQUIRED)
    target_link_libraries(chromascope_core PUBLIC CURL::libcurl)
    target_compile_definitions(chromascope_core PRIVATE CHROMASCOPE_SECURE_STREAM_CURL=1)
endif()
if(WIN32)
    target_link_libraries(chromascope_core PUBLIC ws2_32 winhttp)
    # SHGetKnownFolderPath(FOLDERID_Videos) and CoTaskMemFree.
    target_link_libraries(chromascope_core PUBLIC shell32 ole32)
endif()
chromascope_apply_quality(chromascope_core)

add_executable(chromascope-cli src/cli/main.cpp src/cli/CameraCliCapture.cpp
    src/cli/CameraCliDetection.cpp)
target_link_libraries(chromascope-cli PRIVATE ChromascopeIP::Core)
target_include_directories(chromascope-cli PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
chromascope_apply_quality(chromascope-cli)

if(CHROMASCOPE_BUILD_GUI)
    find_package(glfw3 3.3 REQUIRED)
    find_package(OpenGL REQUIRED)

    if(CHROMASCOPE_IMGUI_DIR)
        set(imgui_SOURCE_DIR "${CHROMASCOPE_IMGUI_DIR}")
    else()
        include(FetchContent)
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.93.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(imgui)
        # The shallow tag checkout is checked against the immutable v1.93.0
        # commit so a moved tag cannot silently change our GUI source.
        find_package(Git REQUIRED)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${imgui_SOURCE_DIR}" rev-parse HEAD
            OUTPUT_VARIABLE _chromascope_imgui_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _chromascope_imgui_git_result)
        if(NOT _chromascope_imgui_git_result EQUAL 0 OR
           NOT _chromascope_imgui_commit STREQUAL
               "c1aaac65e896a45f81f47a4d68f9959e0f86c859")
            message(FATAL_ERROR "Dear ImGui v1.93.0 source commit mismatch: ${_chromascope_imgui_commit}")
        endif()
    endif()

    if(NOT EXISTS "${imgui_SOURCE_DIR}/imgui.cpp")
        message(FATAL_ERROR "Dear ImGui sources unavailable. Set CHROMASCOPE_IMGUI_DIR to tag v1.93.0.")
    endif()
    # Fail if an arbitrary checkout is accidentally used.
    file(STRINGS "${imgui_SOURCE_DIR}/imgui.h" _imgui_version
        REGEX "^#define IMGUI_VERSION ")
    if(NOT _imgui_version MATCHES "\"1\\.93\\.0\"")
        message(FATAL_ERROR "ChromascopeIP requires Dear ImGui v1.93.0; found ${_imgui_version}")
    endif()

    add_library(chromascope_imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )
    target_include_directories(chromascope_imgui SYSTEM PUBLIC
        "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
    target_link_libraries(chromascope_imgui PUBLIC glfw OpenGL::GL)

    add_executable(chromascope-gui src/gui/main.cpp)
    target_link_libraries(chromascope-gui PRIVATE ChromascopeIP::Core chromascope_imgui)
    target_include_directories(chromascope-gui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    chromascope_apply_quality(chromascope-gui)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChromascopeInstall.cmake")

if(BUILD_TESTING AND CHROMASCOPE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
