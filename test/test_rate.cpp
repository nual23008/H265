// test/test_rate.cpp
// Test module rate: Histogram và số bit theo entropy, với các phân bố tính tay được.
#include "rate.h"
#include "check.h"

int main() {
    Histogram constant;
    constant.add({5, 5, 5, 5});
    CHECK(constant.total == 4 && constant.counts.size() == 1);
    CHECK_NEAR(entropyBits(constant), 0.0, 1e-12);            // chỉ một giá trị -> không cần bit nào

    Histogram twoValues;
    twoValues.add({0, 0, 1, 1});
    CHECK_NEAR(entropyBits(twoValues), 4.0, 1e-12);           // p = 1/2 mỗi giá trị -> 1 bit mỗi phần tử

    Histogram fourValues;
    fourValues.add({-3, 0, 2, 7});
    CHECK_NEAR(entropyBits(fourValues), 8.0, 1e-12);          // p = 1/4 -> 2 bit mỗi phần tử

    Histogram skewed;
    skewed.add({0, 0, 1, -1});                                // p(0) = 1/2, p(1) = p(-1) = 1/4
    skewed.add(0);                                            // thêm từng giá trị: p(0) = 3/5
    CHECK(skewed.total == 5 && skewed.counts[0] == 3);
    // 3 * log2(5/3) + 2 * log2(5) = 6.8548...
    CHECK_NEAR(entropyBits(skewed), 6.854752972273, 1e-9);

    Histogram empty;
    CHECK_NEAR(entropyBits(empty), 0.0, 1e-12);
    return testResult("rate");
}
