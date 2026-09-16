#include "transform.h"

#include <cstdint>

// Ma tran integer DCT 8x8 cua HEVC.
const int HEVC_DCT_8[8][8] = {
    {64,  64,  64,  64,  64,  64,  64,  64},
    {89,  75,  50,  18, -18, -50, -75, -89},
    {83,  36, -36, -83, -83, -36,  36,  83},
    {75, -18, -89, -50,  50,  89,  18, -75},
    {64, -64, -64,  64,  64, -64, -64,  64},
    {50, -89,  18,  75, -75, -18,  89, -50},
    {36, -83,  83, -36, -36,  83, -83,  36},
    {18, -50,  75, -89,  89, -75,  50, -18}
};

int RoundShift(long long value, int shift) {
    long long add = 1LL << (shift - 1);
    return static_cast<int16_t>((value + add) >> shift);
}

std::vector<int16_t> Transform8x8(const std::vector<int16_t>& residual) {
    std::vector<int16_t> temp(64, 0);
    std::vector<int16_t> coefficients(64, 0);

    // Transform theo tung hang. HEVC 8-bit dung shift = 2.
    for (int row = 0; row < 8; row++) {
        for (int u = 0; u < 8; u++) {
            long long sum = 0;
            for (int col = 0; col < 8; col++) {
                sum = sum + HEVC_DCT_8[u][col] * residual[row * 8 + col];
            }
            temp[row * 8 + u] = RoundShift(sum, 2);
        }
    }

    // Transform theo tung cot. HEVC 8x8 dung shift = 9.
    for (int v = 0; v < 8; v++) {
        for (int u = 0; u < 8; u++) {
            long long sum = 0;
            for (int row = 0; row < 8; row++) {
                sum = sum + HEVC_DCT_8[v][row] * temp[row * 8 + u];
            }
            coefficients[v * 8 + u] = RoundShift(sum, 9);
        }
    }

    return coefficients;
}

std::vector<int16_t> InverseTransform8x8(const std::vector<int16_t>& coefficients) {
    std::vector<int16_t> temp(64, 0);
    std::vector<int16_t> residual(64, 0);

    // Inverse theo tung cot. HEVC dung shift = 7.
    for (int row = 0; row < 8; row++) {
        for (int u = 0; u < 8; u++) {
            long long sum = 0;
            for (int v = 0; v < 8; v++) {
                sum = sum + HEVC_DCT_8[v][row] * coefficients[v * 8 + u];
            }
            temp[row * 8 + u] = RoundShift(sum, 7);
        }
    }

    // Inverse theo tung hang. HEVC 8-bit dung shift = 12.
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            long long sum = 0;
            for (int u = 0; u < 8; u++) {
                sum = sum + HEVC_DCT_8[u][col] * temp[row * 8 + u];
            }
            residual[row * 8 + col] = RoundShift(sum, 12);
        }
    }

    return residual;
}
