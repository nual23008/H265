cmake_minimum_required(VERSION 3.10)
project(H265Codec CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# MinGW: gói thư viện C++ vào exe để không nạp nhầm libstdc++-6.dll cũ trên PATH
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

include_directories(include)

# ---- Các module của encoder
add_library(h265core STATIC
    src/picture.cpp
    src/block_ops.cpp
    src/intra_pred.cpp
    src/dct.cpp
    src/quant.cpp
    src/metrics.cpp
    src/rate.cpp
)

# CABAC engine (chưa dùng trong pipeline, giữ lại cho việc ghi bitstream sau này)
add_library(cabac STATIC src/cabac.cpp)

# ---- Chương trình
add_executable(encoder tools/encoder.cpp)
target_link_libraries(encoder h265core)

add_executable(read_yuv tools/read_yuv.cpp tools/debug_print.cpp)
target_link_libraries(read_yuv h265core)

# ---- Test (chạy: ctest --output-on-failure trong thư mục build)
enable_testing()

foreach(module picture block_ops intra_pred dct quant metrics rate)
    add_executable(test_${module} test/test_${module}.cpp)
    target_link_libraries(test_${module} h265core)
    add_test(NAME ${module} COMMAND test_${module})
endforeach()

add_executable(test_cabac test/test_cabac.cpp)
target_link_libraries(test_cabac cabac)
# test_cabac dùng assert(): bỏ NDEBUG để assert vẫn chạy ở bản Release
target_compile_options(test_cabac PRIVATE -UNDEBUG)
add_test(NAME cabac COMMAND test_cabac)

# Test tích hợp: encoder đọc Input/Input.yuv nên chạy từ thư mục gốc; bypass thì PSNR phải vô cực
add_test(NAME encoder_bypass
         COMMAND encoder 32 --frames 1 --bypass --out ${CMAKE_BINARY_DIR}/test_output
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(encoder_bypass PROPERTIES PASS_REGULAR_EXPRESSION "PSNR_YUV = inf")
