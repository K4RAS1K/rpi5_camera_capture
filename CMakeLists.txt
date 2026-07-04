cmake_minimum_required(VERSION 3.10)
project(CameraCapture)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(camera_capture
    src/main.cpp
    src/VideoCapture.cpp
)

target_include_directories(camera_capture
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(camera_capture
    pthread
)

message(STATUS "Сборка VideoCapture")