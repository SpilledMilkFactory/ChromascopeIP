cmake_minimum_required(VERSION 3.24)
project(ChromascopeIP VERSION 0.1.1 LANGUAGES CXX)

include(CTest)
option(CHROMASCOPE_BUILD_GUI "Build the Dear ImGui application" ON)
option(CHROMASCOPE_BUILD_TESTS "Build native C++ tests" ON)
set(CHROMASCOPE_IMGUI_DIR "" CACHE PATH "Existing Dear ImGui v1.93.0 source directory (optional)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A system OpenCV 4.x must NOT silently satisfy this dependency.
find_package(OpenCV 5.1.0 EXACT REQUIRED COMPONENTS core imgproc imgcodecs)
message(STATUS "ChromascopeIP OpenCV: ${OpenCV_VERSION} at ${OpenCV_DIR}")

add_library(chromascope_core STATIC
    src/core/Detection.cpp
    src/core/ImageIO.cpp
)
add_library(ChromascopeIP::Core ALIAS chromascope_core)
target_include_directories(chromascope_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(chromascope_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(chromascope_core PUBLIC ${OpenCV_LIBS})
target_compile_features(chromascope_core PUBLIC cxx_std_20)

add_executable(chromascope-cli src/cli/main.cpp)
target_link_libraries(chromascope-cli PRIVATE ChromascopeIP::Core)

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
    target_compile_definitions(chromascope-gui PRIVATE
        CHROMASCOPE_SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples/skittles")
endif()

if(BUILD_TESTING AND CHROMASCOPE_BUILD_TESTS)
    add_executable(chromascope-tests tests/test_detection.cpp)
    target_link_libraries(chromascope-tests PRIVATE ChromascopeIP::Core)
    target_compile_definitions(chromascope-tests PRIVATE
        CHROMASCOPE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples/skittles")
    add_test(NAME chromascope_core COMMAND chromascope-tests)
    set_tests_properties(chromascope_core PROPERTIES LABELS "core;synthetic;samples")

    add_test(NAME chromascope_cli_help COMMAND chromascope-cli --help)
    set_tests_properties(chromascope_cli_help PROPERTIES LABELS "cli;smoke")
    add_test(NAME chromascope_cli_sample COMMAND chromascope-cli
        --input "${CMAKE_CURRENT_SOURCE_DIR}/samples/skittles/s2.jpg")
    set_tests_properties(chromascope_cli_sample PROPERTIES LABELS "cli;samples")
    add_test(NAME chromascope_cli_bad_input COMMAND chromascope-cli
        --input "${CMAKE_CURRENT_SOURCE_DIR}/samples/skittles/absent.jpg")
    set_tests_properties(chromascope_cli_bad_input PROPERTIES
        WILL_FAIL TRUE LABELS "cli;negative")
endif()
