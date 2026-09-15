#!/usr/bin/env bash
# tools/build_encoder.sh
# Build encoder chính (tools/encoder.cpp).
#
# Cách dùng (Git Bash, đứng ở đâu cũng được):
#   bash tools/build_encoder.sh               -> chỉ build
#   bash tools/build_encoder.sh run [QP] [f]  -> build xong chạy luôn, truyền QP và frameIndex cho encoder
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
)

mkdir -p out
# -O2: bật tối ưu, encoder chạy nhanh hơn nhiều so với bản debug
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude "${SOURCES[@]}" -o out/encoder.exe
echo "Build OK: out/encoder.exe"

if [ "$1" = "run" ]; then
    ./out/encoder.exe "${@:2}"  # "${@:2}" = mọi tham số từ vị trí thứ 2 trở đi (QP, frameIndex)
fi
