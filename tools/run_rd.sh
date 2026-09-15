#!/usr/bin/env bash
# tools/run_rd.sh
# Chạy encoder với 4 QP chuẩn rồi vẽ đường cong RD (PSNR_Y theo bpp).
#
# Cách dùng (Git Bash, đứng ở đâu cũng được):
#   bash tools/run_rd.sh        -> mã hoá mọi frame
#   bash tools/run_rd.sh 2      -> chỉ mã hoá 2 frame đầu (chạy nhanh để thử)
#
# Kết quả trong Output/: recon_qp<QP>.yuv, results.csv, rd_curve.png

set -e
cd "$(dirname "$0")/.."

QPS=(22 27 32 37)               # bộ QP trong điều kiện thử nghiệm chuẩn của JCT-VC
OUT_DIR=Output

bash tools/build_encoder.sh

rm -f "$OUT_DIR/results.csv"    # xoá kết quả lần chạy trước, tránh dòng bị lặp

FRAME_ARGS=()
if [ -n "$1" ]; then
    FRAME_ARGS=(--frames "$1")
fi

for qp in "${QPS[@]}"; do
    echo
    ./out/encoder.exe "$qp" "${FRAME_ARGS[@]}" --out "$OUT_DIR"
done

echo
python tools/plot_rd.py "$OUT_DIR/results.csv" "$OUT_DIR/rd_curve.png"
