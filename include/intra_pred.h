// include/intra_pred.h
// Intra prediction: lấy mẫu tham chiếu (spec 8.4.4.2) và các mode dự đoán.
// Hiện có: DC. Cần làm tiếp: lọc mẫu tham chiếu, Planar, Angular.
#pragma once

#include <cstdint>
#include <vector>

#include "picture.h"   // Plane

// Số hiệu mode intra theo spec (cả nhóm thống nhất dùng)
constexpr int kModePlanar    = 0;
constexpr int kModeDc        = 1;
constexpr int kModeHor       = 10;
constexpr int kModeVer       = 26;
constexpr int kNumIntraModes = 35;   // mode 0..34

// Mẫu tham chiếu (reference samples) cho intra prediction của một block N x N (spec 8.4.4.2).
// Spec ký hiệu p[x][y]: x = cột, y = hàng, gốc (0,0) là pixel trên-trái của block.
struct RefSamples {
    int N = 0;
    int32_t corner = 0;           // p[-1][-1]
    std::vector<int32_t> top;     // top[i]  = p[i][-1], i = 0..2N-1  (0..N-1: trên,  N..2N-1: trên-phải)
    std::vector<int32_t> left;    // left[i] = p[-1][i], i = 0..2N-1  (0..N-1: trái,  N..2N-1: dưới-trái)
};

// Pixel (row, col) có dùng được làm tham chiếu cho block đang mã hoá tại (curTopRow, curLeftCol) không?
// Điều kiện: nằm trong ảnh VÀ thuộc CTU đã được mã hoá trước (thứ tự raster).
// Giả định tạm thời: mỗi CTU là một block, chưa chia nhỏ thành CU.
bool isAvailable(const Plane& plane, int ctuSize, int curTopRow, int curLeftCol, int row, int col);

// Lấy 4N+1 mẫu tham chiếu cho block N x N có góc trên-trái tại (topRow, leftCol),
// kèm thay thế mẫu không có sẵn (spec 8.4.4.2.2).
// LƯU Ý: plane truyền vào phải là ảnh ĐÃ TÁI TẠO (thứ decoder có), không phải ảnh gốc.
RefSamples getRefSamples(const Plane& plane, int topRow, int leftCol, int N);

// Giá trị DC = trung bình làm tròn của N mẫu trên và N mẫu trái (spec 8.4.4.2.5).
int computeDcValue(const RefSamples& ref);

// Dự đoán DC cho block N x N. isLuma = true thì áp dụng DC edge filter (khi N < 32).
std::vector<int32_t> predictDC(const RefSamples& ref, bool isLuma);
