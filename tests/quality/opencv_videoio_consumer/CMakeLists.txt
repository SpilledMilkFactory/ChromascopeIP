cmake_minimum_required(VERSION 3.24)
project(chromascope_opencv_videoio_consumer LANGUAGES CXX)
# This intentionally configures independently of ChromascopeIP. Reject
# a stale still-image-only OpenCV 5.1.0 CMake installation.
find_package(OpenCV 5.1.0 EXACT CONFIG REQUIRED
    COMPONENTS core imgproc imgcodecs videoio)
add_executable(opencv_videoio_consumer
    "${CMAKE_CURRENT_LIST_DIR}/../../integration/test_videoio_backend.cpp")
target_include_directories(opencv_videoio_consumer SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(opencv_videoio_consumer PRIVATE ${OpenCV_LIBS})
target_compile_features(opencv_videoio_consumer PRIVATE cxx_std_20)
