// src/transform.cpp
// // Tham khảo Tham khảo từ https://github.com/listenlink/HM.git: TComTrQuant.cpp:860 (xTrMxN), :927 (xITrMxN),
//               TComRom.cpp:384 (DEFINE_DCT8x8_MATRIX), TComRom.h:82 (shift)
#include "transform.h"

#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
// Ma trận DCT số nguyên 8x8 của HEVC.
// HM sinh nó bằng macro DEFINE_DCT8x8_MATRIX(a..g) với
//   a=64  b=83  c=36  d=89  e=75  f=50  g=18
// Đây là xấp xỉ số nguyên của DCT-II đã nhân thang 64 = 2^6.
//
// Hàng k = hàm cơ sở tần số k:
//   hàng 0 toàn 64      -> thành phần một chiều (DC), giá trị trung bình
//   hàng càng xuống dưới -> đổi dấu càng nhiều lần -> tần số càng cao
// ---------------------------------------------------------------------------
const int32_t kT8[kN * kN] = {
  64,  64,  64,  64,  64,  64,  64,  64,
  89,  75,  50,  18, -18, -50, -75, -89,
  83,  36, -36, -83, -83, -36,  36,  83,
  75, -18, -89, -50,  50,  89,  18, -75,
  64, -64, -64,  64,  64, -64, -64,  64,
  50, -89,  18,  75, -75, -18,  89, -50,
  36, -83,  83, -36, -36,  83, -83,  36,
  18, -50,  75, -89,  89, -75,  50, -18,
};

// Thang của ma trận: các phần tử đã nhân 2^6.
constexpr int kMatrixShift = 6;          // HM: g_transformMatrixShift = {6, 6}
constexpr int kMaxTrDynamicRange = 15;
constexpr int kClipMin = -(1 << kMaxTrDynamicRange);
constexpr int kClipMax =  (1 << kMaxTrDynamicRange) - 1;

// T trực giao nhưng KHÔNG trực chuẩn: T * T^T = 2^15 * I
//   (hàng 0 có 8 phần tử bằng 64 -> chuẩn^2 = 8 * 64^2 = 2^15)
// Nên biến đổi 2-D phóng đại 2^15. Hai lần dịch phải dưới đây lấy đi 11 bit,
// còn dư 2^4 = 2^transformShift -- đúng phần mà quantize() chia nốt.
constexpr int kFwdShift1 = (kLog2N + kBitDepth + kMatrixShift) - kMaxTrDynamicRange;  // 2
constexpr int kFwdShift2 = kLog2N + kMatrixShift;                                     // 9

// Biến đổi nghịch phóng đại 2^15, dịch phải 7 + 12 = 19 -> thu nhỏ 2^-4,
// vừa đúng triệt tiêu phần dư của biến đổi thuận.
constexpr int kInvShift1 = kMatrixShift + 1;                                          // 7
constexpr int kInvShift2 = (kMatrixShift + kMaxTrDynamicRange - 1) - kBitDepth;       // 12

inline int32_t clipCoeff(int64_t v) {
  return static_cast<int32_t>(std::max<int64_t>(kClipMin, std::min<int64_t>(kClipMax, v)));
}

inline int64_t rshift(int64_t v, int s) {
  return (v + (1LL << (s - 1))) >> s;   // dịch phải có làm tròn
}

}  // namespace

const int32_t* transformMatrix8() { return kT8; }

// ---------------------------------------------------------------------------
// BIẾN ĐỔI THUẬN:  Y = T * X * T^T
//
// Tách thành 2 lượt 1-D. Thứ tự giống HM: HÀNG trước (shift nhỏ, giữ độ chính
// xác), rồi CỘT (shift lớn, đưa về dải động cuối cùng).
//   lượt 1:  A = X * T^T   (biến đổi từng hàng)
//   lượt 2:  Y = T * A     (biến đổi từng cột)
// ---------------------------------------------------------------------------
Blk forwardTransform(const Blk& residual) {
  Blk a = makeBlk();
  Blk y = makeBlk();

  // Lượt 1 -- theo hàng.  A[r][c] = sum_i X[r][i] * T[c][i]
  for (int r = 0; r < kN; ++r) {
    for (int c = 0; c < kN; ++c) {
      int64_t s = 0;
      for (int i = 0; i < kN; ++i) s += static_cast<int64_t>(residual[at(i, r)]) * kT8[at(i, c)];
      a[at(c, r)] = static_cast<int32_t>(rshift(s, kFwdShift1));
    }
  }

  // Lượt 2 -- theo cột.  Y[r][c] = sum_i T[r][i] * A[i][c]
  for (int r = 0; r < kN; ++r) {
    for (int c = 0; c < kN; ++c) {
      int64_t s = 0;
      for (int i = 0; i < kN; ++i) s += static_cast<int64_t>(kT8[at(i, r)]) * a[at(c, i)];
      y[at(c, r)] = static_cast<int32_t>(rshift(s, kFwdShift2));
    }
  }
  return y;
}

// ---------------------------------------------------------------------------
// BIẾN ĐỔI NGHỊCH:  X' = T^T * Y * T
//
// Thứ tự ngược lại với biến đổi thuận: CỘT trước, rồi HÀNG.
//   lượt 1:  A = T^T * Y   (biến đổi từng cột)
//   lượt 2:  X' = A * T    (biến đổi từng hàng)
// Có chặn biên (clip) ở cả hai lượt -- HM ghi rõ việc chặn ở lượt 2 không nằm
// trong chuẩn, chỉ để bảo vệ kiểu dữ liệu 16-bit.
// ---------------------------------------------------------------------------
Blk inverseTransform(const Blk& coeff) {
  Blk a = makeBlk();
  Blk x = makeBlk();

  // Lượt 1 -- theo cột.  A[r][c] = sum_i T[i][r] * Y[i][c]
  for (int r = 0; r < kN; ++r) {
    for (int c = 0; c < kN; ++c) {
      int64_t s = 0;
      for (int i = 0; i < kN; ++i) s += static_cast<int64_t>(kT8[at(r, i)]) * coeff[at(c, i)];
      a[at(c, r)] = clipCoeff(rshift(s, kInvShift1));
    }
  }

  // Lượt 2 -- theo hàng.  X'[r][c] = sum_i A[r][i] * T[i][c]
  for (int r = 0; r < kN; ++r) {
    for (int c = 0; c < kN; ++c) {
      int64_t s = 0;
      for (int i = 0; i < kN; ++i) s += static_cast<int64_t>(a[at(i, r)]) * kT8[at(c, i)];
      x[at(c, r)] = clipCoeff(rshift(s, kInvShift2));
    }
  }
  return x;
}
