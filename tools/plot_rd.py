# tools/plot_rd.py
# Vẽ đường cong RD từ results.csv: trục hoành bpp (ước lượng entropy, Y + U + V),
# trục tung PSNR (trung bình các frame). Hai đồ thị: PSNR_Y và PSNR_YUV = (6Y + U + V) / 8.
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

    curves = defaultdict(list)           # predictor -> [(bpp, psnr_y, psnr_yuv, qp), ...]
    print("predictor   QP  frames     bpp   PSNR_Y   PSNR_U   PSNR_V PSNR_YUV")
    for (predictor, qp), rows in sorted(groups.items()):
        bpp      = mean([float(r["bpp"]) for r in rows])
        psnr_y   = mean([float(r["psnr_y"]) for r in rows])
        psnr_u   = mean([float(r["psnr_u"]) for r in rows])
        psnr_v   = mean([float(r["psnr_v"]) for r in rows])
        psnr_yuv = mean([float(r["psnr_yuv"]) for r in rows])
        curves[predictor].append((bpp, psnr_y, psnr_yuv, qp))
        print(f"{predictor:10s} {qp:3d} {len(rows):7d} {bpp:7.4f} {psnr_y:8.4f} {psnr_u:8.4f} {psnr_v:8.4f} {psnr_yuv:8.4f}")

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    for ax, index, label in ((axes[0], 1, "PSNR_Y (dB)"), (axes[1], 2, "PSNR_YUV = (6Y + U + V) / 8 (dB)")):
        for predictor, points in curves.items():
            points = sorted(points)      # sắp theo bpp tăng dần để nối đường
            ax.plot([p[0] for p in points], [p[index] for p in points], marker="o", label=predictor)
            for p in points:
                ax.annotate(f"QP {p[3]}", (p[0], p[index]), textcoords="offset points", xytext=(6, -12), fontsize=8)
        ax.set_xlabel("bpp (uoc luong entropy, Y + U + V)")
        ax.set_ylabel(label)
        ax.grid(True, alpha=0.3)
        ax.legend()
    fig.suptitle("Duong cong RD")
    fig.savefig(png_path, dpi=120, bbox_inches="tight")
    print("Da ghi", png_path)


if __name__ == "__main__":
    main()
