// test/test_quant.cpp
// Test module quant: Qstep, giá trị tính tay, ngưỡng dead-zone, đối xứng dấu, sai số khôi phục.
#include "quant.h"
#include "check.h"

#include <cstdlib>
#include <random>
#include <vector>

// Block N x N chỉ có hệ số đầu tiên khác 0
std::vector<int32_t> single(int N, int32_t value) {
    std::vector<int32_t> block(N * N, 0);
    block[0] = value;
    return block;
}

void testQstep() {
    CHECK_NEAR(qStep(4), 1.0, 1e-12);                        // QP 4 -> Qstep = 1
    CHECK_NEAR(qStep(22), 8.0, 1e-12);
    for (int qp = 0; qp + 6 <= 51; ++qp) {
        CHECK_NEAR(qStep(qp + 6), 2.0 * qStep(qp), 1e-9);    // tăng 6 QP -> gấp đôi
    }
}

void testKnownValues() {
    // N = 16, QP 4: bước hiệu dụng = Qstep * 128/N = 1 * 8 = 8
    //   level = (1000 * 16384 + (171 << 8)) >> 17 = 125;  dequant = (125 * 64 + 4) >> 3 = 1000
    CHECK(quantize(single(16, 1000), 4, 16)[0] == 125);
    CHECK(dequantize(single(16, 125), 4, 16)[0] == 1000);

    // N = 8, QP 32: bước hiệu dụng = 25.5 * 16 = 408
    //   level = (5000 * 20560 + (171 << 14)) >> 23 = 12;  dequant = 12 * 51 * 2^3 = 4896 = 12 * 408
    CHECK(quantize(single(8, 5000), 32, 8)[0] == 12);
    CHECK(dequantize(single(8, 12), 32, 8)[0] == 4896);
}

void testDeadZoneThreshold() {
    // N = 16, QP 22: bước hiệu dụng 64. Offset 171/512 -> level 1 cần |c| >= (1 - 171/512) * 64 = 42.6
    CHECK(quantize(single(16, 42), 22, 16)[0] == 0);
    CHECK(quantize(single(16, 43), 22, 16)[0] == 1);
    CHECK(quantize(single(16, -43), 22, 16)[0] == -1);
}

void testZeroAndSymmetry() {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> coeffValue(-20000, 20000);
    for (int N : {8, 16}) {
        const std::vector<int32_t> zero(N * N, 0);
        CHECK(quantize(zero, 30, N) == zero);
        CHECK(dequantize(zero, 30, N) == zero);

        std::vector<int32_t> coeff(N * N), negated(N * N);
        for (int i = 0; i < N * N; ++i) {
            coeff[i]   = coeffValue(rng);
            negated[i] = -coeff[i];
        }
        for (int qp : {0, 22, 37, 51}) {
            const std::vector<int32_t> level = quantize(coeff, qp, N);
            const std::vector<int32_t> levelOfNegated = quantize(negated, qp, N);
            bool symmetric = true;
            for (int i = 0; i < N * N; ++i) {
                if (levelOfNegated[i] != -level[i]) symmetric = false;
            }
            CHECK(symmetric);
        }
    }
}

void testRateAndDistortionTrend() {
    std::mt19937 rng(11);
    std::uniform_int_distribution<int> coeffValue(-3000, 3000);
    for (int N : {8, 16}) {
        std::vector<int32_t> coeff(N * N);
        for (int32_t& v : coeff) v = coeffValue(rng);

        int previousNonZero = N * N + 1;
        for (int qp = 0; qp <= 51; ++qp) {
            const std::vector<int32_t> level    = quantize(coeff, qp, N);
            const std::vector<int32_t> restored = dequantize(level, qp, N);
            const double step = qStep(qp) * 128.0 / N;

            int numNonZero = 0;
            bool errorInBound = true;
            for (int i = 0; i < N * N; ++i) {
                if (level[i] != 0) ++numNonZero;
                // Dead-zone: sai số khôi phục không vượt 2/3 bước (+1 do làm tròn)
                if (std::abs(coeff[i] - restored[i]) > 0.67 * step + 1.0) errorInBound = false;
            }
            CHECK(numNonZero <= previousNonZero);                 // QP tăng -> số level khác 0 không tăng
            CHECK(errorInBound);
            previousNonZero = numNonZero;
        }
    }
}

int main() {
    testQstep();
    testKnownValues();
    testDeadZoneThreshold();
    testZeroAndSymmetry();
    testRateAndDistortionTrend();
    return testResult("quant");
}
