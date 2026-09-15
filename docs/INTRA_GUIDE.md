# Hướng dẫn làm phần Intra Estimation / Intra Prediction

Tài liệu này dành cho người làm khối **Intra-Picture Estimation** và **Intra-Picture Prediction** (hai khối màu xanh dương trong sơ đồ encoder). Encoder hiện đã chạy trọn vẹn nhưng chỉ có dự đoán **DC**. Việc của bạn là thêm **Planar, Horizontal (H), Vertical (V)** và phần **chọn mode**, rồi ghép vào encoder.

Làm lần lượt từ mục 1 đến mục 9. Mỗi mục đều có cách tự kiểm tra kết quả.

**Việc cần làm (tick khi xong):**

- [ ] 1. Cài công cụ, cấu hình git
- [ ] 2. Review Pull Request, tạo branch làm việc
- [ ] 3. Chạy thử code hiện có
- [ ] 4. Đọc hiểu `RefSamples` và `predictDC`
- [ ] 5. Viết `predictPlanar`, `predictVertical`, `predictHorizontal`, `predictIntra`
- [ ] 6. Viết `blockCost`, `chooseIntraMode`
- [ ] 7. Thêm test, chạy `run_tests.sh`
- [ ] 8. Ghép vào `tools/encoder.cpp` và `tools/run_rd.sh`
- [ ] 9. So kết quả với bảng tham chiếu, commit, mở Pull Request

---

## 1. Cài công cụ, cấu hình git

Cần có:

| Công cụ | Windows | Linux (Ubuntu) | Kiểm tra |
|---|---|---|---|
| Git + Bash | [Git for Windows](https://git-scm.com/download/win) (có sẵn **Git Bash**) | `sudo apt install git` | `git --version` |
| g++ hỗ trợ C++17 | MSYS2 / MinGW-w64 / Strawberry Perl | `sudo apt install build-essential` | `g++ --version` (từ 9 trở lên) |
| Python 3 + matplotlib | python.org, rồi `pip install matplotlib` | `sudo apt install python3-matplotlib` | `python -c "import matplotlib"` |

Trên Windows, **mọi lệnh trong tài liệu này gõ trong Git Bash** (trong VS Code: Terminal → New Terminal → chọn *Git Bash*), không gõ trong PowerShell hay cmd.

Cấu hình tên và email **một lần** trên máy (dùng email tài khoản GitHub của bạn), để commit không còn hiện tên mẫu "Tên GitHub của bạn":

```bash
git config --global user.name  "Ho Ten Cua Ban"
git config --global user.email "email-github-cua-ban@example.com"
git config --global --list        # kiểm tra
```

## 2. Review Pull Request, tạo branch làm việc

### 2.1 Review Pull Request của Quan

1. Mở trang repo trên GitHub → tab **Pull requests** → PR từ branch `quan/encoder-loop`.
2. Đọc phần mô tả, rồi sang tab **Files changed** để xem code. Chỗ nào chưa hiểu thì bấm dấu **+** cạnh dòng code để để lại comment.
3. Xong thì bấm **Review changes** → **Approve**. Quan sẽ bấm **Merge pull request**.

### 2.2 Lấy code về máy

Nếu **chưa có** repo trên máy:

```bash
git clone https://github.com/nual23008/H265.git
cd H265
```

Nếu **đã có** repo:

```bash
cd duong/dan/toi/H265
git status                 # phải thấy "nothing to commit"; nếu có file đang sửa dở, hỏi Quan trước khi làm tiếp
git fetch origin
```

### 2.3 Tạo branch làm việc

**Không dùng branch `luan` cũ**: branch đó chỉ còn commit "Clear luan branch" xoá toàn bộ file.

Sau khi PR đã được merge vào `main`:

```bash
git switch main
git pull                              # lấy code mới nhất của main
git switch -c luan/intra-modes        # tạo branch mới từ main và chuyển sang
```

Nếu PR **chưa merge** mà muốn bắt đầu luôn, tạo branch từ branch của Quan:

```bash
git switch -c luan/intra-modes origin/quan/encoder-loop
```

Khi đã thống nhất với Quan, xoá branch `luan` cũ cho gọn:

```bash
git branch -D luan                 # xoá trên máy (nếu có)
git push origin --delete luan      # xoá trên GitHub
```

## 3. Chạy thử code hiện có

Đứng ở thư mục gốc `H265`:

```bash
bash tools/run_tests.sh            # build và chạy toàn bộ test
```

Phải thấy 9 dòng `[PASS]` và cuối cùng là `Tat ca test deu qua.`

```bash
bash tools/build_encoder.sh run 32 --frames 1      # mã hoá 1 frame với QP 32
```

Kết quả mong đợi (chỉ có DC):

```
 frame       bits     bpp   PSNR_Y   PSNR_U   PSNR_V PSNR_YUV  time_ms
     0    2350946  1.1338  33.5925  37.9147  40.2323  34.9627      ...
Mode: planar = 0, dc = 8160, hor = 0, ver = 0
```

```bash
bash tools/run_rd.sh               # 7 frame x QP 22/27/32/37, khoảng 5 giây
```

Kết quả nằm trong thư mục `Output/`: `results.csv` (mở bằng Excel hoặc VS Code), `rd_curve.png` (đồ thị RD), `recon_qp<QP>.yuv` (ảnh tái tạo, xem bằng [YUView](https://github.com/IENT/YUView/releases) với định dạng YUV 4:2:0 8-bit, 1920x1080).

Nếu có lỗi, xem mục **Lỗi hay gặp** ở cuối tài liệu.

## 4. Đọc hiểu code liên quan

Chỉ cần đọc 3 file: `include/intra_pred.h`, `src/intra_pred.cpp`, và phần đầu `tools/encoder.cpp`.

### 4.1 Quy ước

- Block N x N là `std::vector<int32_t>` có N*N phần tử **xếp theo hàng**: phần tử hàng `r`, cột `c` nằm ở `pred[r * N + c]`.
- `row` / `r` là hàng (tương ứng `y` trong spec), `col` / `c` là cột (tương ứng `x`).
- Số hiệu mode theo spec, dùng hằng số trong `intra_pred.h`: `kModePlanar = 0`, `kModeDc = 1`, `kModeHor = 10`, `kModeVer = 26`.
- `isLuma = true` với Y, `false` với U, V.

### 4.2 `RefSamples`: mẫu tham chiếu

`getRefSamples(recon, topRow, leftCol, N)` đã làm sẵn mọi việc khó: chỉ lấy mẫu **đã mã hoá**, lấy từ **ảnh tái tạo**, và **thay thế** mẫu nằm ngoài ảnh hoặc chưa mã hoá. Bạn chỉ việc đọc 3 trường:

```
 corner   top[0]  top[1]  ...  top[N-1]  | top[N] ... top[2N-1]    <- hàng ngay trên block (và trên-phải)
 left[0]  +--------------------------------+
 left[1]  |                                |
  ...     |   block N x N: pred[r*N + c]   |
 left[N-1]+--------------------------------+
 left[N]    <- dưới-trái
  ...
 left[2N-1]
```

| Trường | Ý nghĩa | Kiểu |
|---|---|---|
| `ref.N` | kích thước block | `int` |
| `ref.corner` | mẫu góc trên-trái | `int32_t` |
| `ref.top[i]` | hàng ngay phía trên, `i = 0..2N-1` (`top[0]` sát góc) | `std::vector<int32_t>` |
| `ref.left[i]` | cột ngay bên trái, `i = 0..2N-1` (`left[0]` sát góc) | `std::vector<int32_t>` |

### 4.3 `predictDC`: làm mẫu

Mở `src/intra_pred.cpp`, đọc `computeDcValue` và `predictDC`. Ba hàm bạn viết có cùng khuôn: nhận `const RefSamples&`, trả về `std::vector<int32_t>` N*N phần tử.

Code cũ dùng kiểu `Pixel` / `Block` (Planar, V, H, angular) vẫn còn trong lịch sử git, xem được bằng:

```bash
git show 903eb90:src/prediction.cpp
git show 903eb90:src/angular_prediction.cpp
```

Công thức Planar trong code cũ dùng lại được. Vertical / Horizontal cũ **thiếu bộ lọc biên**; angular mode 26 / 10 cũ thì có bộ lọc và đúng.

## 5. Viết Planar, Vertical, Horizontal

### 5.1 Khai báo trong `include/intra_pred.h`

Thêm vào **cuối file**:

```cpp
// Dự đoán Planar (spec 8.4.4.2.4). Dùng chung cho luma và chroma.
std::vector<int32_t> predictPlanar(const RefSamples& ref);

// Dự đoán Vertical, mode 26 (spec 8.4.4.2.6). isLuma = true và N < 32 thì lọc cột đầu.
std::vector<int32_t> predictVertical(const RefSamples& ref, bool isLuma);

// Dự đoán Horizontal, mode 10 (spec 8.4.4.2.6). isLuma = true và N < 32 thì lọc hàng đầu.
std::vector<int32_t> predictHorizontal(const RefSamples& ref, bool isLuma);

// Gọi đúng hàm theo mode: kModePlanar, kModeDc, kModeHor, kModeVer.
std::vector<int32_t> predictIntra(const RefSamples& ref, int mode, bool isLuma);
```

### 5.2 Công thức

Dưới đây `N` là kích thước block, `log2N` là log2(N) (N = 16 thì log2N = 4), `r` là hàng, `c` là cột.

**Planar** (không có bộ lọc biên):

```
pred[r*N + c] = ( (N-1-c) * left[r] + (c+1) * top[N]
                + (N-1-r) * top[c]  + (r+1) * left[N]
                + N ) >> (log2N + 1)
```

Nghĩa: trung bình của hai phép nội suy tuyến tính, theo chiều ngang (giữa `left[r]` và mẫu trên-phải `top[N]`) và theo chiều dọc (giữa `top[c]` và mẫu dưới-trái `left[N]`).

**Vertical (mode 26)**:

```
pred[r*N + c] = top[c]                                          với mọi r, c
nếu isLuma và N < 32:                                           (lọc cột đầu, c = 0)
    pred[r*N + 0] = clip(top[0] + ((left[r] - corner) >> 1))    với mọi r
```

**Horizontal (mode 10)**:

```
pred[r*N + c] = left[r]                                         với mọi r, c
nếu isLuma và N < 32:                                           (lọc hàng đầu, r = 0)
    pred[0*N + c] = clip(left[0] + ((top[c] - corner) >> 1))    với mọi c
```

`clip(v)` = kẹp `v` về `[0, 255]`, dùng `std::clamp(v, 0, 255)` trong `<algorithm>`.

**Quan trọng:** phải dùng `>> 1`, **không** dùng `/ 2`. Với số âm hai phép này khác nhau: `-1 >> 1 = -1` (làm tròn xuống) nhưng `-1 / 2 = 0`. Spec dùng `>> 1`. Test ở mục 7 có trường hợp kiểm tra đúng chỗ này.

### 5.3 Khung code trong `src/intra_pred.cpp`

Thêm vào cuối file (tự viết phần thân theo công thức ở trên):

```cpp
std::vector<int32_t> predictPlanar(const RefSamples& ref) {
    const int N = ref.N;
    int log2N = 0;
    while ((1 << log2N) < N) {
        ++log2N;
    }
    std::vector<int32_t> pred(N * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            // TODO: pred[r * N + c] = ...
        }
    }
    return pred;
}

std::vector<int32_t> predictVertical(const RefSamples& ref, bool isLuma) {
    // TODO: chép top[c] xuống mọi hàng, rồi lọc cột đầu nếu isLuma && N < 32
}

std::vector<int32_t> predictHorizontal(const RefSamples& ref, bool isLuma) {
    // TODO: chép left[r] sang mọi cột, rồi lọc hàng đầu nếu isLuma && N < 32
}

std::vector<int32_t> predictIntra(const RefSamples& ref, int mode, bool isLuma) {
    switch (mode) {
        case kModePlanar: return predictPlanar(ref);
        case kModeDc:     return predictDC(ref, isLuma);
        case kModeHor:    return predictHorizontal(ref, isLuma);
        case kModeVer:    return predictVertical(ref, isLuma);
    }
    std::cerr << "Error: mode " << mode << " chua duoc ho tro." << std::endl;
    std::exit(1);
}
```

Nhớ thêm `#include <cstdlib>` (cho `std::exit`) và `#include <iostream>` (cho `std::cerr`) ở đầu `src/intra_pred.cpp`.

## 6. Chọn mode: `blockCost`, `chooseIntraMode`

### 6.1 Khai báo trong `include/intra_pred.h`

```cpp
// Tiêu chí so sánh prediction với block gốc
enum class CostType {
    Sad,   // Sum of Absolute Differences: tổng |gốc - dự đoán|
    Ssd,   // Sum of Squared Differences:  tổng (gốc - dự đoán)^2
};

// Chi phí của một prediction so với block gốc (cùng N*N phần tử)
long long blockCost(const std::vector<int32_t>& original, const std::vector<int32_t>& prediction, CostType cost);

// Thử 4 mode theo thứ tự Planar, DC, Hor, Ver (luma), trả về mode có chi phí NHỎ NHẤT.
// Hai mode bằng nhau thì giữ mode thử TRƯỚC.
int chooseIntraMode(const std::vector<int32_t>& original, const RefSamples& ref, CostType cost);
```

### 6.2 Yêu cầu

- `blockCost`: cộng dồn bằng `long long`. SSD của block 16x16 có thể tới 256 * 255^2 ≈ 16,6 triệu; khi cộng nhiều block, `int` sẽ tràn.
- `chooseIntraMode`: dự đoán với `isLuma = true`, thử **đúng thứ tự** `kModePlanar, kModeDc, kModeHor, kModeVer`, chỉ thay mode tốt nhất khi chi phí **nhỏ hơn hẳn** (`<`, không phải `<=`).

Thứ tự thử và phép so sánh `<` quyết định mode được chọn khi hai mode có chi phí bằng nhau. Làm đúng như trên thì kết quả của bạn sẽ **trùng từng con số** với bảng tham chiếu ở mục 9.

## 7. Thêm test

Mở `test/test_intra_pred.cpp`. File đã có hàm `makeRampPlane` tạo plane 6x6 với `pixel(row, col) = row * 10 + col`. Block 2x2 tại (row 2, col 2) có mẫu tham chiếu tính tay được:

```
corner = 11,  top = {12, 13, 14, 15},  left = {21, 31, 31, 31}
```

Thêm các hàm sau vào file, **trước** hàm `main`:

```cpp
// Tạo RefSamples bằng tay (không cần plane), dùng cho các trường hợp đặc biệt
RefSamples makeRef(int corner, std::vector<int32_t> top, std::vector<int32_t> left) {
    RefSamples ref;
    ref.N      = static_cast<int>(top.size()) / 2;
    ref.corner = corner;
    ref.top    = top;
    ref.left   = left;
    return ref;
}

void testPlanarVerticalHorizontal() {
    Plane plane = makeRampPlane(6, 6);
    RefSamples ref = getRefSamples(plane, 2, 2, 2);   // corner 11, top {12,13,14,15}, left {21,31,31,31}

    // Planar: [0] = (1*21 + 1*14 + 1*12 + 1*31 + 2) >> 2 = 20
    //         [1] = (0*21 + 2*14 + 1*13 + 1*31 + 2) >> 2 = 18
    //         [2] = (1*31 + 1*14 + 0*12 + 2*31 + 2) >> 2 = 27
    //         [3] = (0*31 + 2*14 + 0*13 + 2*31 + 2) >> 2 = 23
    CHECK(predictPlanar(ref) == (std::vector<int32_t>{20, 18, 27, 23}));

    // Vertical luma: cột đầu [0] = 12 + ((21 - 11) >> 1) = 17, [2] = 12 + ((31 - 11) >> 1) = 22
    CHECK(predictVertical(ref, true)  == (std::vector<int32_t>{17, 13, 22, 13}));
    CHECK(predictVertical(ref, false) == (std::vector<int32_t>{12, 13, 12, 13}));   // chroma: không lọc

    // Horizontal luma: hàng đầu [0] = 21 + ((12 - 11) >> 1) = 21, [1] = 21 + ((13 - 11) >> 1) = 22
    CHECK(predictHorizontal(ref, true)  == (std::vector<int32_t>{21, 22, 31, 31}));
    CHECK(predictHorizontal(ref, false) == (std::vector<int32_t>{21, 21, 31, 31}));

    // predictIntra phải gọi đúng hàm theo mode
    CHECK(predictIntra(ref, kModePlanar, true) == predictPlanar(ref));
    CHECK(predictIntra(ref, kModeDc, true)     == predictDC(ref, true));
    CHECK(predictIntra(ref, kModeHor, false)   == predictHorizontal(ref, false));
    CHECK(predictIntra(ref, kModeVer, true)    == predictVertical(ref, true));
}

void testBoundaryFilterEdgeCases() {
    // Hiệu âm phải làm tròn XUỐNG: (99 - 100) >> 1 = -1  (nếu dùng / 2 sẽ ra 0 -> sai)
    RefSamples ref = makeRef(100, {90, 110, 0, 0}, {99, 250, 0, 0});
    // Vertical: [0] = 90 + ((99 - 100) >> 1) = 89,  [2] = 90 + ((250 - 100) >> 1) = 165
    CHECK(predictVertical(ref, true) == (std::vector<int32_t>{89, 110, 165, 110}));
    // Horizontal: [0] = 99 + ((90 - 100) >> 1) = 94,  [1] = 99 + ((110 - 100) >> 1) = 104
    CHECK(predictHorizontal(ref, true) == (std::vector<int32_t>{94, 104, 250, 250}));

    // Clip: 255 + ((255 - 0) >> 1) = 382 -> 255
    RefSamples bright = makeRef(0, {255, 0, 0, 0}, {255, 255, 0, 0});
    CHECK(predictVertical(bright, true) == (std::vector<int32_t>{255, 0, 255, 0}));
}

void testChooseIntraMode() {
    Plane plane = makeRampPlane(6, 6);
    RefSamples ref = getRefSamples(plane, 2, 2, 2);

    // Block gốc trùng đúng prediction Vertical -> chi phí V = 0, các mode khác > 0
    const std::vector<int32_t> original = {17, 13, 22, 13};
    CHECK(blockCost(original, predictPlanar(ref), CostType::Sad) == 23);         // |17-20|+|13-18|+|22-27|+|13-23|
    CHECK(blockCost(original, predictDC(ref, true), CostType::Sad) == 12);       // DC = {18, 18, 22, 19}
    CHECK(blockCost(original, predictHorizontal(ref, true), CostType::Sad) == 40);
    CHECK(blockCost(original, predictDC(ref, true), CostType::Ssd) == 1 + 25 + 0 + 36);
    CHECK(chooseIntraMode(original, ref, CostType::Sad) == kModeVer);
    CHECK(chooseIntraMode(original, ref, CostType::Ssd) == kModeVer);

    // Tham chiếu phẳng, block phẳng: mọi mode đều có chi phí 0 -> giữ mode thử đầu tiên (Planar)
    RefSamples flat = makeRef(50, {50, 50, 50, 50}, {50, 50, 50, 50});
    CHECK(chooseIntraMode({50, 50, 50, 50}, flat, CostType::Sad) == kModePlanar);
}
```

Rồi thêm 3 dòng gọi trong `main`, trước `return testResult("intra_pred");`:

```cpp
    testPlanarVerticalHorizontal();
    testBoundaryFilterEdgeCases();
    testChooseIntraMode();
```

Chạy:

```bash
bash tools/run_tests.sh
```

Nếu có dòng `CHECK failed`, dòng đó chỉ ra file và số dòng của test sai. So công thức của bạn với phép tính tay ghi trong comment ngay phía trên.

## 8. Ghép vào encoder

Tất cả thay đổi ở mục này nằm trong `tools/encoder.cpp` và `tools/run_rd.sh`.

### 8.1 Xoá 2 hàm tạm

Xoá **cả khối** từ dòng `// ===================== Chỗ ghép code của teammate ...` đến dòng `// =====...` kết thúc ngay sau hàm `predictBlock` (gồm hàm `chooseLumaMode` và `predictBlock`).

### 8.2 Dùng hàm mới

Trong `encodeChromaBlock`, đổi `predictBlock` thành `predictIntra`:

```cpp
    return codeResidual(recon, topRow, leftCol, N, orgBlock, predictIntra(ref, mode, false), qp, tqMode);
```

Trong `encodeCtu`, thêm tham số `CostType cost` và dùng hàm mới:

```cpp
CtuResult encodeCtu(const Picture& padded, Picture& recon, int topRow, int leftCol, int ctuSize, int qp, TqMode tqMode,
                    CostType cost) {
    ...
    result.lumaMode           = chooseIntraMode(orgY, refY, cost);
    result.sentY = codeResidual(recon.Y, topRow, leftCol, ctuSize, orgY,
                                predictIntra(refY, result.lumaMode, true), qp, tqMode);
```

Trong `encodeFrame`, thêm tham số `CostType cost` và truyền tiếp xuống `encodeCtu`:

```cpp
FrameResult encodeFrame(const Picture& picture, const Picture& padded, Picture& recon, int ctuSize, int qp, TqMode tqMode,
                        CostType cost) {
    ...
            CtuResult ctu = encodeCtu(padded, recon, ctuRow * ctuSize, ctuCol * ctuSize, ctuSize, qp, tqMode, cost);
```

### 8.3 Thêm cờ `--cost sad|ssd` trong `main`

Ngay dưới dòng `TqMode tqMode = TqMode::Normal;` thêm:

```cpp
    CostType cost = CostType::Sad;
```

Trong vòng lặp đọc tham số, thêm nhánh này **ngay trước** nhánh `} else if (arg == "--frames" && i + 1 < argc) {`:

```cpp
        } else if (arg == "--cost" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "sad") {
                cost = CostType::Sad;
            } else if (value == "ssd") {
                cost = CostType::Ssd;
            } else {
                std::cerr << "Error: --cost chi nhan sad hoac ssd." << std::endl;
                return 1;
            }
```

Ngay sau khối kiểm tra `if (qp < 0 || qp > 51) { ... }` thêm:

```cpp
    const std::string costName = (cost == CostType::Ssd) ? "ssd" : "sad";
```

Đổi tên file recon để SAD và SSD không ghi đè lên nhau:

```cpp
    std::string reconPath = outDir + "/recon_qp" + std::to_string(qp) + "_" + costName;
```

Truyền `cost` khi gọi `encodeFrame`:

```cpp
        const FrameResult r = encodeFrame(picture, padded, recon, CTU_SIZE, qp, tqMode, cost);
```

Cột `predictor` trong CSV: đổi `",dc-only,"` thành:

```cpp
                << frameIndex << ',' << qp << ',' << tqModeTag(tqMode) << ',' << costName << ','
```

Cuối cùng, sửa dòng hướng dẫn ở đầu file thành `./out/encoder.exe [QP] [--frames K] [--out DIR] [--cost sad|ssd] [--no-quant | --bypass]`.

### 8.4 Chạy cả SAD và SSD trong `tools/run_rd.sh`

Thay vòng lặp `for qp in ...` bằng:

```bash
for cost in sad ssd; do
    for qp in "${QPS[@]}"; do
        echo
        ./out/encoder.exe "$qp" "${FRAME_ARGS[@]}" --cost "$cost" --out "$OUT_DIR"
    done
done
```

Không cần sửa `build_encoder.sh` hay `CMakeLists.txt`, vì bạn chỉ sửa file đã có sẵn trong danh sách build.

## 9. Kiểm tra kết quả, commit, mở Pull Request

### 9.1 Ba bước kiểm tra

**(1)** Mọi test phải qua:

```bash
bash tools/run_tests.sh
```

**(2)** Bỏ qua DCT và quant thì ảnh tái tạo vẫn phải giống hệt ảnh gốc:

```bash
./out/encoder.exe 32 --frames 1 --bypass --out Output/check
```

Dòng `Trung binh` phải có `PSNR_YUV = inf`.

**(3)** So với bảng tham chiếu:

```bash
bash tools/run_rd.sh
```

Làm đúng như mục 5–8 thì các số trung bình 7 frame phải **trùng khớp** với bảng dưới. Nếu PSNR và bpp gần đúng nhưng số mode lệch, hãy xem lại thứ tự thử mode và phép so sánh `<` ở mục 6.

| predictor | QP | bpp | PSNR_Y | PSNR_YUV | Mode (planar / dc / hor / ver), cộng 7 frame |
|---|---|---|---|---|---|
| dc-only (trước khi ghép) | 32 | 1.1339 | 33.5926 | 34.9600 | 0 / 57120 / 0 / 0 |
| sad | 22 | 2.4140 | 41.9621 | 42.9909 | 19016 / 11102 / 17278 / 9724 |
| sad | 27 | 1.7001 | 37.7095 | 38.8798 | 19184 / 11399 / 17007 / 9530 |
| sad | 32 | 1.1029 | 33.6204 | 34.9931 | 18441 / 11957 / 17154 / 9568 |
| sad | 37 | 0.6598 | 29.9651 | 31.5681 | 18153 / 12568 / 16884 / 9515 |
| ssd | 22 | 2.4053 | 41.9671 | 42.9963 | 20033 / 13027 / 15754 / 8306 |
| ssd | 27 | 1.6929 | 37.7171 | 38.8832 | 19932 / 13281 / 15740 / 8167 |
| ssd | 32 | 1.0967 | 33.6278 | 34.9996 | 19103 / 13629 / 15948 / 8440 |
| ssd | 37 | 0.6548 | 29.9755 | 31.5853 | 18742 / 14071 / 15727 / 8580 |

Cách đọc: cùng QP, 4 mode giúp **giảm khoảng 3–4 % số bit** mà PSNR vẫn giữ hoặc tăng nhẹ, nên trên đồ thị RD các đường `sad`/`ssd` nằm lệch sang trái-lên so với `dc-only`. SSD tốt hơn SAD một chút vì PSNR cũng đo bằng bình phương sai số.

### 9.2 Commit và push

```bash
git status                                   # xem những file đã sửa
git add include/intra_pred.h src/intra_pred.cpp test/test_intra_pred.cpp tools/encoder.cpp tools/run_rd.sh
git commit -m "add planar, horizontal, vertical prediction and SAD/SSD mode decision"
git push -u origin luan/intra-modes          # lần đầu push branch mới cần -u
```

Commit message viết rõ đã làm gì. Không dùng "Update".

### 9.3 Mở Pull Request

1. Lên GitHub, bấm **Compare & pull request** (hoặc tab Pull requests → New pull request, base `main`, compare `luan/intra-modes`).
2. Mô tả ngắn: đã thêm gì, kết quả `run_tests.sh`, dán bảng `predictor / QP / bpp / PSNR_YUV` mà `run_rd.sh` in ra.
3. Gán Quan làm reviewer.

### 9.4 Checklist trước khi mở PR

- [ ] `bash tools/run_tests.sh` qua hết
- [ ] `--bypass` cho `PSNR_YUV = inf`
- [ ] Bảng `run_rd.sh` khớp bảng tham chiếu ở mục 9.1
- [ ] Không sửa hoặc xoá file ngoài 5 file ở mục 9.2 (nếu cần, hỏi Quan trước)
- [ ] `git log` hiện đúng tên của bạn, commit message rõ ràng

## Lỗi hay gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
|---|---|---|
| Chạy `.exe` không in gì, Git Bash báo mã thoát 127 | Nạp nhầm DLL thư viện C++ | Luôn build bằng script trong `tools/` (đã có `-static-libstdc++`), không tự gõ lệnh `g++` thiếu cờ này |
| `bash: ./out/encoder.exe: No such file or directory` | Chưa build, hoặc không đứng ở thư mục gốc `H265` | `cd` về thư mục `H265`, chạy `bash tools/build_encoder.sh` |
| `Error: Could not open file 'Input/Input.yuv'` | Chạy từ thư mục khác | Đứng ở thư mục gốc `H265` rồi chạy |
| `Python was not found; run without arguments to install from the Microsoft Store` | Windows gọi nhầm lối tắt `python3` của Store | Cài Python từ python.org, dùng lệnh `python` |
| `undefined reference to predictPlanar` | Có khai báo trong `.h` nhưng chưa định nghĩa trong `.cpp`, hoặc sai chữ ký | So từng chữ giữa `intra_pred.h` và `intra_pred.cpp` |
| Test Vertical/Horizontal sai ở `testBoundaryFilterEdgeCases` | Dùng `/ 2` thay vì `>> 1`, hoặc quên clip | Xem mục 5.2 |
| PSNR chroma tụt, số liệu lệch bảng | Bật bộ lọc biên cho chroma | Chroma luôn gọi với `isLuma = false` |
| Số mode lệch bảng, PSNR gần đúng | Sai thứ tự thử mode hoặc dùng `<=` | Xem mục 6.2 |
| PSNR cao bất thường so với bảng | Lấy mẫu tham chiếu từ ảnh gốc thay vì `recon` | Chỉ dùng `RefSamples` truyền vào, không đọc plane gốc |

## Phần mở rộng (không bắt buộc)

Chỉ làm sau khi mục 1–9 đã xong và PR đã được merge.

- **Lọc mẫu tham chiếu** (spec 8.4.4.2.3): trước khi dự đoán Planar cho luma N >= 8, làm mượt `top`, `left` bằng bộ lọc [1, 2, 1] / 4.
- **Angular mode 2..34**: code angular cũ đã đúng (`git show 903eb90:src/angular_prediction.cpp`), có thể chuyển sang kiểu `RefSamples`, rồi thêm các mode vào `chooseIntraMode`.
- **Chi phí có tính số bit của mode**: `cost = SAD + lambda * bitMode`, gần với cách HM chọn mode hơn.
