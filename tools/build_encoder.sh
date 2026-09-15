#!/usr/bin/env bash
# tools/build_encoder.sh
# Build encoder chính (tools/encoder.cpp).
#
# Cách dùng (Git Bash, đứng ở đâu cũng được):
#   bash tools/build_encoder.sh               -> chỉ build
#   bash tools/build_encoder.sh run [tham so] -> build xong chạy luôn, vd: run 32 --frames 2
#
# Thêm module mới: thêm đường dẫn file .cpp vào danh sách SOURCES bên dưới.

set -e                          # có lệnh nào lỗi thì dừng script ngay
cd "$(dirname "$0")/.."         # chuyển về thư mục gốc H265 (thư mục cha của tools/)

SOURCES=(
    tools/encoder.cpp
    src/picture.cpp
    src/block_ops.cpp
    src/intra_pred.cpp
    src/dct.cpp
    src/quant.cpp
    src/metrics.cpp
    src/rate.cpp
)

mkdir -p out
# -O2: bật tối ưu, encoder chạy nhanh hơn nhiều so với bản debug
# -static-libstdc++ -static-libgcc: gói thư viện C++ vào exe. Nếu không, Git Bash có thể nạp nhầm
#   libstdc++-6.dll cũ của Git (thiếu hàm std::filesystem) -> exe thoát ngay với mã 127, không in gì.
g++ -std=c++17 -Wall -Wextra -O2 -static-libstdc++ -static-libgcc -Iinclude "${SOURCES[@]}" -o out/encoder.exe
echo "Build OK: out/encoder.exe"

if [ "$1" = "run" ]; then
    ./out/encoder.exe "${@:2}"  # "${@:2}" = mọi tham số từ vị trí thứ 2 trở đi
fi
