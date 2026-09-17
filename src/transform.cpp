#include "transform.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace {
    constexpr int kBitDepth = 8;

    // kCos[m] ≈ 64·√2·cos(m·π/64), m = 0..32, được chuẩn HEVC làm tròn sẵn.
    // Riêng kCos[0] = 64 (không phải 90): hàng DC nhỏ hơn √2 lần để mọi hàng có cùng độ dài.
    // Cả ma trận 32x32 (1024 số) chỉ dùng các giá trị trong bảng này, kèm dấu +/-.

    const int kCos[33] = {
    64,                                 // m = 0 (hàng DC)
    90, 90, 90, 89, 88, 87, 85, 83,     // m = 1..8
    82, 80, 78, 75, 73, 70, 67, 64,     // m = 9..16
    61, 57, 54, 50, 46, 43, 38, 36,     // m = 17..24
    31, 25, 22, 18, 13,  9,  4,  0,     // m = 25..32
    };

    // ≈ 64·√2·cos(m·π/64) với m >= 0 bất kỳ: dùng chu kỳ và tính đối xứng của cos để đưa m về 0..32.
    int cosValue(int m) {
        m %= 128;                            // cos có chu kỳ 2π (= 128 đơn vị)
        if (m > 64) m = 128 - m;             // cos(2π - a) =  cos(a)   -> m còn 0..64
        if (m > 32) return -kCos[64 - m];    // cos(π  - a) = -cos(a)   -> m còn 0..32
        return kCos[m];
    }

    int log2Size(int N) {
    switch (N) {
        case 4:  return 2;
        case 8:  return 3;
        case 16: return 4;
        case 32: return 5;
    }
    throw std::invalid_argument("DCT chi ho tro N = 4, 8, 16, 32");
    }

    // Cả ma trận N x N, phần tử [k * N + n]. Tính một lần cho mỗi block thay vì gọi dctCoef trong vòng lặp.
    std::vector<int16_t> dctMatrix(int N) {
        std::vector<int16_t> T(N * N);
        for (int k = 0; k < N; ++k) {
            for (int n = 0; n < N; ++n) {
                T[k * N + n] = dctCoef(N, k, n);
            }
        }
        return T;
    }

    int64_t roundShift(int64_t value, int shift) {
        return (value + (int64_t{1} << (shift - 1))) >> shift;   // dịch phải có làm tròn
    }

    int16_t clip16(int64_t value) {
        return static_cast<int16_t>(std::max<int64_t>(-32768, std::min<int64_t>(32767, value)));
    }
}   //namespace

int dctCoef(int N, int k, int n) {
    // DCT-II: hàng k, cột n tỉ lệ với cos((2n+1)·k·π / (2N)).
    // Đổi sang đơn vị π/64: k32 = k * (32 / N), m = (2n+1) * k32.
    // => ma trận N x N chính là N hàng (cách đều) và N cột đầu của ma trận 32x32.
    // Với k32 < 32 thì m không bao giờ là bội của 64, nên kCos[0] = 64 chỉ rơi vào hàng DC.
    log2Size(N);   // kiểm tra N hợp lệ
    int k32 = k * (32 / N);
    return cosValue((2 * n + 1) * k32);
}

// Biến đổi thuận, 2 lượt 1-D, thứ tự giống HM:
//   lượt 1 (theo HÀNG): A[r][c] = sum_i X[r][i] * T[c][i]   >> shift1
//   lượt 2 (theo CỘT):  Y[r][c] = sum_i T[r][i] * A[i][c]   >> shift2
std::vector<int16_t> Transform8x8(const std::vector<int16_t>& residual, int N) {
    const int log2N  = log2Size(N);
    if (residual.size() != static_cast<std::size_t>(N * N)) {
        throw std::invalid_argument("Kich thuoc residual phai bang N * N");
    }
    const int shift1 = log2N + kBitDepth - 9;   // N=16: 3
    const int shift2 = log2N + 6;               // N=16: 10
    const std::vector<int16_t> T = dctMatrix(N);

    std::vector<int32_t> A(N * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int64_t sum = 0;
            for (int i = 0; i < N; ++i) {
                sum += static_cast<int64_t>(residual[r * N + i]) * T[c * N + i];
            }
            A[r * N + c] = static_cast<int32_t>(roundShift(sum, shift1));
        }
    }

    std::vector<int16_t> Y(N * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int64_t sum = 0;
            for (int i = 0; i < N; ++i) {
                sum += static_cast<int64_t>(T[r * N + i]) * A[i * N + c];
            }
            Y[r * N + c] = clip16(roundShift(sum, shift2));
        }
    }
    return Y;
}

// Biến đổi nghịch, thứ tự ngược lại:
//   lượt 1 (theo CỘT):  A[r][c]  = sum_i T[i][r] * Y[i][c]   >> 7,  chặn 16-bit
//   lượt 2 (theo HÀNG): X'[r][c] = sum_i A[r][i] * T[i][c]   >> 12, chặn 16-bit
std::vector<int16_t> InverseTransform8x8(const std::vector<int16_t>& coeff, int N) {
    log2Size(N);
    if (coeff.size() != static_cast<std::size_t>(N * N)) {
        throw std::invalid_argument("Kich thuoc coeff phai bang N * N");
    }
    const int shift1 = 7;
    const int shift2 = 20 - kBitDepth;          // 12
    const std::vector<int16_t> T = dctMatrix(N);

    std::vector<int16_t> A(N * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int64_t sum = 0;
            for (int i = 0; i < N; ++i) {
                sum += static_cast<int64_t>(T[i * N + r]) * coeff[i * N + c];
            }
            A[r * N + c] = clip16(roundShift(sum, shift1));
        }
    }

    std::vector<int16_t> X(N * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int64_t sum = 0;
            for (int i = 0; i < N; ++i) {
                sum += static_cast<int64_t>(A[r * N + i]) * T[i * N + c];
            }
            X[r * N + c] = clip16(roundShift(sum, shift2));
        }
    }
    return X;
}
