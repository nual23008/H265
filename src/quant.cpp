// src/quant.cpp
// Chuyển từ bản cũ (commit a591d9d, src/quantization.cpp) sang vector<int32_t> và tham số N.
// Tham khảo HM: TComTrQuant.cpp (xQuant, xDeQuant), TComRom.cpp (g_quantScales, g_invQuantScales).
#include "quant.h"

#include <algorithm>   // std::min, std::max
#include <cassert>
#include <cmath>       // std::pow
#include <cstdlib>     // std::abs

namespace {

constexpr int kBitDepth = 8;

// Qstep(QP) = (kInvQuantScales[QP % 6] / 64) * 2^(QP / 6)
// 6 giá trị gốc ứng với QP = 0..5; qua mỗi chu kỳ 6 QP thì nhân đôi.
const int kInvQuantScales[6] = {40, 45, 51, 57, 64, 72};          // dùng khi dequantize (phép NHÂN)

// kQuantScales[i] ≈ 2^20 / kInvQuantScales[i]: thay phép CHIA cho Qstep bằng phép nhân rồi dịch phải.
const int kQuantScales[6] = {26214, 23302, 20560, 18396, 16384, 14564};

// Phần phóng đại thừa của DCT số nguyên: forwardDct cho hệ số lớn gấp 2^transformShift = 128/N
// lần DCT trực chuẩn. Quantize phải chia nốt phần này, dequantize nhân trả lại.
int transformShift(int N) {
    assert((N == 4 || N == 8 || N == 16 || N == 32) && "quant chi ho tro N = 4, 8, 16, 32");
    int log2N = 0;
    while ((1 << log2N) < N) ++log2N;
    return 15 - kBitDepth - log2N;     // N = 16: 3,  N = 8: 4
}

int32_t clip16(int64_t value) {
    return static_cast<int32_t>(std::max<int64_t>(-32768, std::min<int64_t>(32767, value)));
}

}  // namespace

double qStep(int qp) {
    return kInvQuantScales[qp % 6] / 64.0 * std::pow(2.0, qp / 6);
}

// level = sign(c) * ((|c| * kQuantScales[QP%6] + offset) >> qbits)
//   1/Qstep = (64 / kInvQuantScales) / 2^per = (kQuantScales / 2^14) / 2^per
//   => chia cho Qstep * 2^transformShift = nhân kQuantScales rồi dịch phải qbits = 14 + per + transformShift
std::vector<int32_t> quantize(const std::vector<int32_t>& coeff, int qp, int N) {
    assert(qp >= 0 && qp <= 51);
    const int qbits      = 14 + qp / 6 + transformShift(N);
    const int64_t offset = 171LL << (qbits - 9);    // 171/512 ≈ 1/3 (HM dùng cho intra). Làm tròn 1/2 sẽ là 256.
    const int scale      = kQuantScales[qp % 6];

    std::vector<int32_t> level(N * N);
    for (int i = 0; i < N * N; ++i) {
        // Làm việc với trị tuyệt đối để số âm và dương được làm tròn đối xứng
        int64_t magnitude = (static_cast<int64_t>(std::abs(coeff[i])) * scale + offset) >> qbits;
        level[i] = clip16(coeff[i] < 0 ? -magnitude : magnitude);
    }
    return level;
}

// coeff' = level * (kInvQuantScales / 64) * 2^per * 2^transformShift
//        = level * kInvQuantScales, rồi dịch phải (6 - per - transformShift) bit
//          (số bit âm nghĩa là phải dịch TRÁI, xảy ra khi QP lớn)
std::vector<int32_t> dequantize(const std::vector<int32_t>& level, int qp, int N) {
    assert(qp >= 0 && qp <= 51);
    const int shift = 6 - qp / 6 - transformShift(N);
    const int scale = kInvQuantScales[qp % 6];

    std::vector<int32_t> coeff(N * N);
    for (int i = 0; i < N * N; ++i) {
        int64_t value = static_cast<int64_t>(level[i]) * scale;
        if (shift > 0) {
            value = (value + (1LL << (shift - 1))) >> shift;   // dịch phải có làm tròn
        } else {
            value *= (1LL << -shift);                          // "dịch trái" bằng phép nhân (an toàn với số âm)
        }
        coeff[i] = clip16(value);
    }
    return coeff;
}
