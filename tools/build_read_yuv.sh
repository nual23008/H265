#!/usr/bin/env bash
# tools/build_read_yuv.sh
# Build chương trình thử read_yuv.
#
# Cách dùng (Git Bash, đứng ở đâu cũng được):
#   bash tools/build_read_yuv.sh        -> chỉ build
#   bash tools/build_read_yuv.sh run    -> build xong chạy luôn
#
# Thêm module mới: thêm đường dẫn file .cpp vào danh sách SOURCES bên dưới.

set -e                          # có lệnh nào lỗi thì dừng script ngay
cd "$(dirname "$0")/.."         # chuyển về thư mục gốc H265 (thư mục cha của tools/)

SOURCES=(
    tools/read_yuv.cpp
    tools/debug_print.cpp
    src/picture.cpp
    src/block_ops.cpp
    src/intra_pred.cpp
    src/dct.cpp
    src/quant.cpp
)

mkdir -p out
# -static-libstdc++ -static-libgcc: gói thư viện C++ vào exe, tránh nạp nhầm DLL cũ trên PATH (xem build_encoder.sh)
g++ -std=c++17 -Wall -Wextra -g -static-libstdc++ -static-libgcc -Iinclude "${SOURCES[@]}" -o out/read_yuv.exe
echo "Build OK: out/read_yuv.exe"

if [ "$1" = "run" ]; then
    ./out/read_yuv.exe          # chạy từ thư mục gốc H265 nên đường dẫn Input/Input.yuv đúng
fi
