#!/usr/bin/env bash
# tools/run_tests.sh
# Build và chạy toàn bộ test: unit test từng module, test CABAC, và phép thử bypass của encoder.
#
# Cách dùng (Git Bash, đứng ở đâu cũng được):
#   bash tools/run_tests.sh
# Mã thoát 0 = mọi test đều qua.

set -e
cd "$(dirname "$0")/.."

CXXFLAGS=(-std=c++17 -Wall -Wextra -O1 -g -static-libstdc++ -static-libgcc -Iinclude)
CORE=(src/picture.cpp src/block_ops.cpp src/intra_pred.cpp src/dct.cpp src/quant.cpp src/metrics.cpp src/rate.cpp)
MODULE_TESTS=(picture block_ops intra_pred dct quant metrics rate)

mkdir -p out/tests/obj
failed=0

# Biên dịch các module MỘT lần thành file .o, mọi test chỉ cần liên kết lại
CORE_OBJECTS=()
for src in "${CORE[@]}"; do
    obj="out/tests/obj/$(basename "$src" .cpp).o"
    g++ "${CXXFLAGS[@]}" -c "$src" -o "$obj"
    CORE_OBJECTS+=("$obj")
done

# run_test <tên> <file nguồn / object...>: build rồi chạy, test lỗi thì tăng biến failed
run_test() {
    local name=$1
    shift
    g++ "${CXXFLAGS[@]}" "$@" -o "out/tests/$name.exe"
    if ! "./out/tests/$name.exe"; then
        failed=$((failed + 1))
    fi
}

for module in "${MODULE_TESTS[@]}"; do
    run_test "test_$module" "test/test_$module.cpp" "${CORE_OBJECTS[@]}"
done
run_test test_cabac test/test_cabac.cpp src/cabac.cpp

# Test tích hợp: bỏ qua T/Q thì ảnh tái tạo phải giống hệt ảnh gốc (PSNR vô cực)
bash tools/build_encoder.sh > /dev/null
rm -rf out/tests/encoder_bypass
if ./out/encoder.exe 32 --frames 1 --bypass --out out/tests/encoder_bypass | grep -q "PSNR_YUV = inf"; then
    echo "[PASS] encoder_bypass"
else
    echo "[FAIL] encoder_bypass"
    failed=$((failed + 1))
fi

echo
if [ "$failed" -eq 0 ]; then
    echo "Tat ca test deu qua."
else
    echo "$failed test that bai."
    exit 1
fi
