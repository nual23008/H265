# H265

Encoder intra H.265/HEVC đơn giản, viết để học (không bám đúng HM).

- Vào: `Input/Input.yuv` (YUV 4:2:0 8-bit, 1920x1080, 7 frame).
- Mỗi CTU gồm luma 16x16 và hai chroma 8x8 (CTU = CU = TU). Ảnh được đệm lên 1088 hàng.
- Pipeline: dự đoán intra (lấy mẫu tham chiếu từ ảnh tái tạo) → residual → DCT số nguyên → lượng tử hoá theo QP → giải lượng tử → DCT nghịch → ảnh tái tạo.
- Ra: ảnh tái tạo `.yuv`, PSNR Y/U/V/YUV, số bit ước lượng theo entropy, `results.csv`, đường cong RD.
- Chưa có: Planar/H/V (hiện chỉ dự đoán DC), bitstream/CABAC, quadtree, deblocking/SAO, inter.

## Build và chạy (Git Bash)

```bash
bash tools/build_encoder.sh run 32 --frames 2   # build rồi mã hoá 2 frame đầu với QP 32
bash tools/run_rd.sh                            # QP 22/27/32/37, ghi Output/results.csv và Output/rd_curve.png
bash tools/run_tests.sh                         # toàn bộ test
bash tools/build_read_yuv.sh run                # chương trình học từng bước
```

Tham số encoder: `./out/encoder.exe [QP] [--frames K] [--out DIR] [--no-quant | --bypass]`.
`--bypass` bỏ qua DCT và quant, PSNR phải bằng `inf` (dùng để kiểm tra vòng lặp).

Xem ảnh tái tạo: `ffplay -f rawvideo -pixel_format yuv420p -video_size 1920x1080 Output/recon_qp32.yuv`

Hoặc dùng CMake:

```bash
cmake -S . -B build && cmake --build build && (cd build && ctest --output-on-failure)
```

## Cấu trúc

| Thư mục / file | Nội dung |
|---|---|
| `include/picture.h` | `Plane`, `Picture`, đọc/ghi frame, đệm ảnh, `getBlock` / `writeBlock` |
| `include/intra_pred.h` | `RefSamples`, `getRefSamples` (mẫu có sẵn + thay thế mẫu thiếu), `predictDC`, số hiệu mode |
| `include/block_ops.h` | residual, cộng block, SAD, phương sai |
| `include/dct.h` | DCT số nguyên thuận/nghịch, N = 4..32 |
| `include/quant.h` | `quantize` / `dequantize` theo QP |
| `include/metrics.h` | MSE, PSNR |
| `include/rate.h` | Histogram, ước lượng bit theo entropy |
| `include/cabac.h` | CABAC engine (chưa dùng trong pipeline) |
| `tools/encoder.cpp` | Vòng lặp encoder chính |
| `tools/read_yuv.cpp` | Chương trình học từng bước |
| `test/` | Unit test từng module (`check.h`: macro `CHECK`) |

## Quy ước

- Toạ độ dùng `row`, `col` (row = y, col = x trong spec). Pixel `[row * width + col]`.
- Mọi dữ liệu mức block là `std::vector<int32_t>` N*N phần tử xếp theo hàng.
- Số hiệu mode theo spec: Planar = 0, DC = 1, H = 10, V = 26 (`kModePlanar`, ...).
- Mẫu tham chiếu luôn lấy từ **ảnh tái tạo**, không lấy từ ảnh gốc.

## Ghép Intra Estimation / Prediction

Hướng dẫn chi tiết từng bước (cài công cụ, git, công thức, test có đáp án, bảng kết quả tham chiếu): [docs/INTRA_GUIDE.md](docs/INTRA_GUIDE.md).

Tóm tắt: trong `tools/encoder.cpp` có 2 hàm đánh dấu `TODO` cần thay:

```cpp
int chooseLumaMode(const std::vector<int32_t>& original, const RefSamples& ref);   // chọn mode (SAD / SSD)
std::vector<int32_t> predictBlock(const RefSamples& ref, int mode, bool isLuma);    // dự đoán theo mode
```

Sau khi thay: `bash tools/run_tests.sh` phải qua hết, và đường RD mới phải nằm trên đường `dc-only`.
