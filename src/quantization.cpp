// src/quantization.cpp
// Tham chiếu HM: TComTrQuant.cpp (xQuant, xDeQuant), TComRom.cpp
#include "quantization.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

// Bảng bước lượng tử cơ sở (Qstep * 64) và nghịch đảo ((1 << 20) / inv)
const int kInvQuantScales[6] = {40, 45, 51, 57, 64, 72};
const int kQuantScales[6]    = {26214, 23302, 20560, 18396, 16384, 14564};

constexpr int kQuantShift  = 14; // Bù hệ số phóng đại 2^14 của kQuantScales
constexpr int kIQuantShift = 6;  // Bù hệ số phóng đại 2^6 của kInvQuantScales

// Dải động hệ số biến đổi chuẩn Main profile ([-32768, 32767])
constexpr int kMaxTrDynamicRange = 15;
constexpr int kCoeffMin = -(1 << kMaxTrDynamicRange);
constexpr int kCoeffMax =  (1 << kMaxTrDynamicRange) - 1;

// Độ lợi dư của biến đổi số nguyên cần triệt tiêu trong quantization
inline int transformShift(int log2TrSize) {
  return kMaxTrDynamicRange - kBitDepth - log2TrSize;
}

template <typename T>
inline T clamp3(T v, T lo, T hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

// ---------------------------------------------------------------------------
// QUANTIZATION: level = round(coeff / (Qstep * 2^transformShift))
// ---------------------------------------------------------------------------
Blk quantize(const Blk& coeff, const QpParam& qp, int log2TrSize) {
  const int n     = 1 << log2TrSize;
  const int shift = transformShift(log2TrSize);
  const int qbits = kQuantShift + qp.per + shift;

  // Offset làm tròn có dead-zone (~1/3 cho I-slice thay vì 1/2)
  const int add = 171 << (qbits - 9);

  Blk level(n * n, 0);
  for (int i = 0; i < n * n; ++i) {
    const int32_t c = coeff[i];

    // Dùng int64_t tránh tràn số khi tích đạt ~2^30 trước khi cộng add
    const int64_t tmp = static_cast<int64_t>(std::abs(c)) * kQuantScales[qp.rem];
    const int32_t mag = static_cast<int32_t>((tmp + add) >> qbits);

    const int32_t v = (c < 0) ? -mag : mag;
    level[i] = clamp3(v, kCoeffMin, kCoeffMax);
  }
  return level;
}

// ---------------------------------------------------------------------------
// DEQUANTIZATION: coeff' = level * Qstep * 2^transformShift
// ---------------------------------------------------------------------------
Blk dequantize(const Blk& level, const QpParam& qp, int log2TrSize) {
  const int n     = 1 << log2TrSize;
  const int shift = transformShift(log2TrSize);

  // Số bit cần dịch phải: kIQuantShift - (shift + qp.per)
  const int rightShift = kIQuantShift - (shift + qp.per);

  Blk coeff(n * n, 0);
  for (int i = 0; i < n * n; ++i) {
    const int64_t v = static_cast<int64_t>(level[i]) * kInvQuantScales[qp.rem];

    int64_t r;
    if (rightShift > 0) {
      r = (v + (1LL << (rightShift - 1))) >> rightShift; // Dịch phải có làm tròn
    } else {
      r = v << (-rightShift);                            // Dịch trái phục hồi thang
    }
    coeff[i] = static_cast<int32_t>(
        clamp3<int64_t>(r, kCoeffMin, kCoeffMax));
  }
  return coeff;
}

// ---------------------------------------------------------------------------
// Giá trị thực của Qstep để tham chiếu/kiểm tra
// ---------------------------------------------------------------------------
double qStep(int qp) {
  return (kInvQuantScales[qp % 6] / 64.0) * std::pow(2.0, qp / 6);
}