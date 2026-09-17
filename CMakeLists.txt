cmake_minimum_required(VERSION 3.10)

project(simple_h265_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(simple_codec
    src/lib.cpp
    src/prediction.cpp
    src/residual.cpp
    src/transform.cpp
    src/quantization.cpp
)

target_include_directories(simple_codec PUBLIC header)

add_executable(simple_codec_app
    src/main.cpp
)

target_link_libraries(simple_codec_app PRIVATE simple_codec)