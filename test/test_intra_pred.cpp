// test/test_intra_pred.cpp
// Test module intra_pred: mẫu có sẵn, lấy + thay thế mẫu tham chiếu, dự đoán DC.
// Dùng plane 6x6 với pixel(row, col) = row * 10 + col và "CTU" 2x2 (lưới 3x3 CTU), tính tay được.
#include "intra_pred.h"
#include "check.h"

#include <vector>

Plane makeRampPlane(int width, int height) {
    Plane plane;
    initPlane(plane, width, height);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            plane.data[plane.getIndex(row, col)] = static_cast<uint8_t>(row * 10 + col);
        }
    }
    return plane;
}

void testModeNumbers() {
    CHECK(kModePlanar == 0 && kModeDc == 1 && kModeHor == 10 && kModeVer == 26 && kNumIntraModes == 35);
}

void testIsAvailable() {
    Plane plane = makeRampPlane(6, 6);
    // Block đang mã hoá: CTU hàng 1, cột 1 (row 2, col 2), địa chỉ raster 4
    CHECK(isAvailable(plane, 2, 2, 2, 1, 1));      // CTU (0,0), addr 0: đã mã hoá
    CHECK(isAvailable(plane, 2, 2, 2, 1, 4));      // CTU (0,2), addr 2: trên-phải, đã mã hoá
    CHECK(isAvailable(plane, 2, 2, 2, 3, 1));      // CTU (1,0), addr 3: bên trái
    CHECK(!isAvailable(plane, 2, 2, 2, 4, 1));     // CTU (2,0), addr 6: dưới-trái, CHƯA mã hoá
    CHECK(!isAvailable(plane, 2, 2, 2, 2, 2));     // chính CTU đang mã hoá
    CHECK(!isAvailable(plane, 2, 2, 2, -1, 3));    // ngoài ảnh (hàng -1)
    CHECK(!isAvailable(plane, 2, 2, 2, 1, 6));     // ngoài ảnh (cột 6)
}

void testRefSamplesMiddle() {
    Plane plane = makeRampPlane(6, 6);
    RefSamples ref = getRefSamples(plane, 2, 2, 2);
    CHECK(ref.N == 2);
    CHECK(ref.corner == 11);
    CHECK(ref.top == (std::vector<int32_t>{12, 13, 14, 15}));   // trên + trên-phải đều có sẵn
    CHECK(ref.left == (std::vector<int32_t>{21, 31, 31, 31}));  // dưới-trái chưa có -> lặp mẫu 31
}

void testRefSamplesTopEdge() {
    Plane plane = makeRampPlane(6, 6);
    // Block ở hàng 0: không có hàng trên, không có góc; chỉ có cột trái (1, 11)
    // Quét từ dưới-trái lên: mẫu có sẵn đầu tiên là 11 -> dưới-trái = 11; góc và hàng trên lặp mẫu trước = 1
    RefSamples ref = getRefSamples(plane, 0, 2, 2);
    CHECK(ref.corner == 1);
    CHECK(ref.left == (std::vector<int32_t>{1, 11, 11, 11}));
    CHECK(ref.top == (std::vector<int32_t>{1, 1, 1, 1}));
}

void testRefSamplesNoneAvailable() {
    Plane plane = makeRampPlane(6, 6);
    RefSamples ref = getRefSamples(plane, 0, 0, 2);            // CTU đầu tiên: chưa có gì
    CHECK(ref.corner == 128);
    CHECK(ref.top == (std::vector<int32_t>(4, 128)));
    CHECK(ref.left == (std::vector<int32_t>(4, 128)));
}

void testDcPrediction() {
    Plane plane = makeRampPlane(6, 6);
    RefSamples ref = getRefSamples(plane, 2, 2, 2);
    // dcVal = (12 + 13 + 21 + 31 + 2) >> 2 = 19
    CHECK(computeDcValue(ref) == 19);
    // Luma có lọc biên: [0] = (21 + 2*19 + 12 + 2) >> 2 = 18, [1] = (13 + 3*19 + 2) >> 2 = 18,
    //                   [2] = (31 + 3*19 + 2) >> 2 = 22, [3] = 19
    CHECK(predictDC(ref, true) == (std::vector<int32_t>{18, 18, 22, 19}));
    // Chroma không lọc
    CHECK(predictDC(ref, false) == (std::vector<int32_t>(4, 19)));
}

int main() {
    testModeNumbers();
    testIsAvailable();
    testRefSamplesMiddle();
    testRefSamplesTopEdge();
    testRefSamplesNoneAvailable();
    testDcPrediction();
    return testResult("intra_pred");
}
