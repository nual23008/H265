// test/test_block_ops.cpp
// Test module block_ops: residual, phép cộng block, SAD, phương sai.
#include "block_ops.h"
#include "check.h"

#include <vector>

int main() {
    const std::vector<int32_t> original   = {10, 20, 0, 255};
    const std::vector<int32_t> prediction = {3, 25, 0, 128};

    const std::vector<int32_t> residual = computeResidual(original, prediction);
    CHECK(residual == (std::vector<int32_t>{7, -5, 0, 127}));        // có thể âm
    CHECK(addBlocks(prediction, residual) == original);              // cộng lại phải ra ảnh gốc
    CHECK(sumAbsolute(residual) == 7 + 5 + 0 + 127);

    CHECK_NEAR(blockVariance({1, 2, 3, 4}), 1.25, 1e-12);            // E[x^2] - E[x]^2 = 7.5 - 6.25
    CHECK_NEAR(blockVariance({9, 9, 9, 9}), 0.0, 1e-12);             // block phẳng
    return testResult("block_ops");
}
