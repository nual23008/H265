#include "quantization.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

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
    if (N != 4 && N != 8 && N != 16 && N != 32) {
        throw std::invalid_argument("Quant chi ho tro N = 4, 8, 16, 32");
    }
    int log2N = 0;
    while ((1 << log2N) < N) ++log2N;
    return 15 - kBitDepth - log2N;     // N = 16: 3,  N = 8: 4
}

int16_t clip16(int64_t value) {
    return static_cast<int16_t>(std::max<int64_t>(-32768, std::min<int64_t>(32767, value)));
}

void validateInput(std::size_t size, int qp, int N) {
    if (qp < 0 || qp > 51) {
        throw std::invalid_argument("QP phai nam trong khoang 0..51");
    }
    transformShift(N);
    if (size != static_cast<std::size_t>(N * N)) {
        throw std::invalid_argument("Kich thuoc block phai bang N * N");
    }
}

}  // namespace

// level = sign(c) * ((|c| * kQuantScales[QP%6] + offset) >> qbits)
//   1/Qstep = (64 / kInvQuantScales) / 2^per = (kQuantScales / 2^14) / 2^per
//   => chia cho Qstep * 2^transformShift = nhân kQuantScales rồi dịch phải qbits = 14 + per + transformShift
std::vector<int16_t> Quantize(const std::vector<int16_t>& coeff, int qp, int N) {
    validateInput(coeff.size(), qp, N);
    const int qbits      = 14 + qp / 6 + transformShift(N);
    const int64_t offset = int64_t{171} << (qbits - 9);    // 171/512 ≈ 1/3 (HM dùng cho intra). Làm tròn 1/2 sẽ là 256.
    const int scale      = kQuantScales[qp % 6];

    std::vector<int16_t> level(N * N);
    for (int i = 0; i < N * N; ++i) {
        // Làm việc với trị tuyệt đối để số âm và dương được làm tròn đối xứng
        const int64_t value = coeff[i];
        const int64_t magnitude = value < 0 ? -value : value;
        const int64_t quantized = (magnitude * scale + offset) >> qbits;
        level[i] = clip16(value < 0 ? -quantized : quantized);
    }
    return level;
}

// coeff' = level * (kInvQuantScales / 64) * 2^per * 2^transformShift
//        = level * kInvQuantScales, rồi dịch phải (6 - per - transformShift) bit
//          (số bit âm nghĩa là phải dịch TRÁI, xảy ra khi QP lớn)

std::vector<int16_t> Dequantize(const std::vector<int16_t>& level, int qp, int N) {
    validateInput(level.size(), qp, N);
    const int shift = 6 - qp / 6 - transformShift(N);
    const int scale = kInvQuantScales[qp % 6];

    std::vector<int16_t> coeff(N * N);
    for (int i = 0; i < N * N; ++i) {
        int64_t value = static_cast<int64_t>(level[i]) * scale;
        if (shift > 0) {
            value = (value + (int64_t{1} << (shift - 1))) >> shift;   // dịch phải có làm tròn
        } else {
            value *= (int64_t{1} << -shift);                   // "dịch trái" bằng phép nhân (an toàn với số âm)
        }
        coeff[i] = clip16(value);
    }
    return coeff;
}
