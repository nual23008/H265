# Intra codec don gian cho file YUV420p

Chuong trinh doc toan bo cac frame trong file YUV420p. Moi block Y 8x8 duoc
thu voi ba nhom intra cua HEVC:

- `MODE_PLANAR` (mode 0).
- `MODE_DC` (mode 1) co boundary filtering.
- `ANGULAR` (mode 2..34), gom ca Horizontal mode 10 va Vertical mode 26.

Intra estimation tinh SAD cho du 35 mode va chon mode co SAD nho nhat.
Mode duoc chon tiep tuc di qua residual, integer DCT HEVC, quantization, dequantization,
IDCT va ghep thanh reconstructed frame. Kenh U va V duoc giu nguyen.

Du lieu YUV duoc luu bang hai struct `Plane` va `Picture` trong
`header/picture.h`. Transform 8x8 dung ma tran so nguyen HEVC va cac shift
cho du lieu 8-bit: forward `2, 9`, inverse `7, 12`. Ma khong dung `cos()`
hoac he so floating-point.

## Chia file

- `prediction.cpp`: `GetTopReference`, `GetLeftReference`, thay the mau thieu,
  lay 2N reference, Planar,
  DC, noi suy Angular va boundary filtering.
- `intra_estimation.cpp`: `CalculateSAD` va `EstimateBestIntraMode`.
- `residual.cpp`: `CalculateResidual`.
- `transform.cpp`: ma tran integer DCT 8x8 cua HEVC, forward va inverse.
- `quantization.cpp`: `Quantize` va `Dequantize`.
- `reconstruction.cpp`: `ReconstructPlane` ket noi tat ca cac buoc.
- `yuv_io.cpp`: `ReadYUV420Frame` va `WriteYUV420Frame`.
- `psnr.cpp`: `CalculatePSNR`.
- `main.cpp`: xu ly tat ca frame cua file.

Moi file `.cpp` co mot file khai bao cung ten trong thu muc `header`.

## Bien dich va chay Input.yuv

`Input.yuv` co dung luong tuong ung 7 frame YUV420p 1920x1080. Chay khong
co tham so se dung truc tiep file nay:

```bash
cmake -S . -B build
cmake --build build
./build/simple_codec_app
```

Output mac dinh la `Reconstructed.yuv`, `quantStep = 16`.

Co the truyen file va kich thuoc khac:

```bash
./build/simple_codec_app input.yuv output.yuv 1920 1080 16
```

Chuong trinh chi ho tro YUV420 planar 8-bit va width/height chia het cho 8.
