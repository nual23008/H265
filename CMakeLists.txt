cmake_minimum_required(VERSION 3.10)
project(H265Codec)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

add_executable(h265_demo
    src/main.cpp
    src/prediction.cpp
    src/angular_prediction.cpp
    src/residual.cpp
    src/transform.cpp
    src/quantization.cpp
    src/cabac.cpp
    src/bitstream.cpp
    src/lib.cpp
)

enable_testing()

add_executable(test_cabac
    test/test_cabac.cpp
    src/cabac.cpp
)

add_test(NAME cabac_round_trip COMMAND test_cabac)

add_executable(angular_prediction_demo
    tools/angular_prediction_demo.cpp
    src/angular_prediction.cpp
)

add_executable(test_angular_prediction
    test/test_angular_prediction.cpp
    src/angular_prediction.cpp
)
add_test(NAME angular_prediction COMMAND test_angular_prediction)
