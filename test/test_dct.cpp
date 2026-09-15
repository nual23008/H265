// test/test_dct.cpp
// Test module dct: giá trị ma trận theo bảng HM, tính đối xứng, block phẳng, sai số thuận -> nghịch.
#include "dct.h"
#include "check.h"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <vector>

const int kSizes[] = {4, 8, 16, 32};

void testMatrixValues() {
    // Hàng 1 (tần số thấp nhất khác DC) lấy từ bảng HM
    const int row1N4[4]   = {83, 36, -36, -83};
    const int row1N8[8]   = {89, 75, 50, 18, -18, -50, -75, -89};
    const int row1N16[8]  = {90, 87, 80, 70, 57, 43, 25, 9};                                  // nửa đầu
    const int row1N32[16] = {90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13, 4};   // nửa đầu
    for (int n = 0; n < 4; ++n)  CHECK(dctCoef(4, 1, n) == row1N4[n]);
    for (int n = 0; n < 8; ++n)  CHECK(dctCoef(8, 1, n) == row1N8[n]);
    for (int n = 0; n < 8; ++n)  CHECK(dctCoef(16, 1, n) == row1N16[n]);
    for (int n = 0; n < 16; ++n) CHECK(dctCoef(32, 1, n) == row1N32[n]);
    CHECK(dctCoef(4, 2, 0) == 64 && dctCoef(4, 2, 1) == -64);
    CHECK(dctCoef(8, 3, 1) == -18);

    for (int N : kSizes) {
        for (int k = 0; k < N; ++k) {
            for (int n = 0; n < N; ++n) {
                if (k == 0) CHECK(dctCoef(N, k, n) == 64);                           // hàng DC toàn 64
                // Hàng chẵn đối xứng, hàng lẻ phản đối xứng qua giữa
                const int sign = (k % 2 == 0) ? 1 : -1;
                CHECK(dctCoef(N, k, N - 1 - n) == sign * dctCoef(N, k, n));
            }
        }
    }
}

void testFlatAndZeroBlock() {
    for (int N : kSizes) {
        const std::vector<int32_t> flat(N * N, 100);
        const std::vector<int32_t> coeff = forwardDct(flat, N);
        CHECK(coeff[0] == 12800);                                  // 128 * 100 với mọi N
        CHECK(std::count(coeff.begin() + 1, coeff.end(), 0) == N * N - 1);   // mọi AC = 0
        CHECK(inverseDct(coeff, N) == flat);                       // block phẳng khôi phục chính xác

        const std::vector<int32_t> zero(N * N, 0);
        CHECK(forwardDct(zero, N) == zero);
        CHECK(inverseDct(zero, N) == zero);
    }
}

void testRoundTripError() {
    // Ma trận HEVC chỉ gần trực giao nên thuận -> nghịch lệch vài đơn vị, nhưng phải nhỏ
    std::mt19937 rng(2026);
    std::uniform_int_distribution<int> residualValue(-255, 255);
    for (int N : kSizes) {
        int maxError = 0;
        long long sumError = 0;
        const int numBlocks = 200;
        for (int t = 0; t < numBlocks; ++t) {
            std::vector<int32_t> residual(N * N);
            for (int32_t& v : residual) v = residualValue(rng);
            const std::vector<int32_t> restored = inverseDct(forwardDct(residual, N), N);
            for (int i = 0; i < N * N; ++i) {
                const int e = std::abs(restored[i] - residual[i]);
                maxError = std::max(maxError, e);
                sumError += e;
            }
        }
        CHECK(maxError <= 8);
        CHECK(static_cast<double>(sumError) / (numBlocks * N * N) <= 1.0);
    }
}

int main() {
    testMatrixValues();
    testFlatAndZeroBlock();
    testRoundTripError();
    return testResult("dct");
}
