cmake_minimum_required(VERSION 3.10)
project(H265Codec)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

add_executable(h265_demo
    src/main.cpp
    src/prediction.cpp
    src/residual.cpp
    src/transform.cpp
    src/quantization.cpp
    src/cabac.cpp
    src/bitstream.cpp
)
