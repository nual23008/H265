# tools/plot_rd.py
# Vẽ đường cong RD từ results.csv: trục hoành bpp (ước lượng entropy), trục tung PSNR_Y (trung bình các frame).
# Mỗi giá trị cột "predictor" (vd dc-only, sau này sad / ssd) là một đường.
#
# Cách dùng: python tools/plot_rd.py [Output/results.csv] [Output/rd_curve.png]
import csv
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")            # không mở cửa sổ, chỉ ghi ra file ảnh
import matplotlib.pyplot as plt


def mean(values):
    return sum(values) / len(values)


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "Output/results.csv"
    png_path = sys.argv[2] if len(sys.argv) > 2 else "Output/rd_curve.png"

    # Gom các dòng (mỗi dòng = 1 frame) theo (predictor, qp)
    groups = defaultdict(list)
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            if row["tq_mode"] != "normal":   # no-quant / bypass chỉ để kiểm tra, không phải điểm RD
                continue
            groups[(row["predictor"], int(row["qp"]))].append(row)

    if not groups:
        print("Khong co dong tq_mode = normal trong", csv_path)
        return

    curves = defaultdict(list)           # predictor -> [(bpp, psnr, qp), ...]
    print("predictor   QP  frames     bpp   PSNR_Y   nnz/frame")
    for (predictor, qp), rows in sorted(groups.items()):
        bpp  = mean([float(r["bpp"]) for r in rows])
        psnr = mean([float(r["psnr_y"]) for r in rows])
        nnz  = mean([int(r["nnz"]) for r in rows])
        curves[predictor].append((bpp, psnr, qp))
        print(f"{predictor:10s} {qp:3d} {len(rows):7d} {bpp:7.4f} {psnr:8.4f} {nnz:11.0f}")

    fig, ax = plt.subplots(figsize=(7, 5))
    for predictor, points in curves.items():
        points.sort()                    # sắp theo bpp tăng dần để nối đường
        ax.plot([p[0] for p in points], [p[1] for p in points], marker="o", label=predictor)
        for bpp, psnr, qp in points:
            ax.annotate(f"QP {qp}", (bpp, psnr), textcoords="offset points", xytext=(6, -12), fontsize=8)

    ax.set_xlabel("bpp (uoc luong entropy, chi luma)")
    ax.set_ylabel("PSNR_Y (dB)")
    ax.set_title("Duong cong RD")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.savefig(png_path, dpi=120, bbox_inches="tight")
    print("Da ghi", png_path)


if __name__ == "__main__":
    main()
