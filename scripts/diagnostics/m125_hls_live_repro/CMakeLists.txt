cmake_minimum_required(VERSION 3.24)
project(chromascope_m125_hls_live_repro LANGUAGES CXX)
if(NOT DEFINED CHROMASCOPE_ROOT OR NOT IS_DIRECTORY "${CHROMASCOPE_ROOT}/include/chromascope")
    message(FATAL_ERROR "CHROMASCOPE_ROOT must reference the local ChromascopeIP repository")
endif()
find_package(Threads REQUIRED)
find_package(OpenCV 5.1.0 EXACT CONFIG REQUIRED COMPONENTS core imgproc videoio)
add_executable(chromascope_hls_live_repro
    probe.cpp
    "${CHROMASCOPE_ROOT}/src/core/HlsPrefetchBuffer.cpp"
    "${CHROMASCOPE_ROOT}/src/core/HlsByteWindow.cpp")
target_include_directories(chromascope_hls_live_repro PRIVATE "${CHROMASCOPE_ROOT}/include")
target_include_directories(chromascope_hls_live_repro SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(chromascope_hls_live_repro PRIVATE ${OpenCV_LIBS} Threads::Threads)
target_compile_features(chromascope_hls_live_repro PRIVATE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chromascope_hls_live_repro PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
