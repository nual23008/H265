// test/test_metrics.cpp
// Test module metrics: MSE, PSNR, chỉ tính trên vùng width x height.
#include "metrics.h"
#include "check.h"

#include <cmath>

int main() {
    Plane original, recon;
    initPlane(original, 4, 3);
    initPlane(recon, 4, 3);
    for (size_t i = 0; i < original.data.size(); ++i) {
        original.data[i] = static_cast<uint8_t>(100 + i);
        recon.data[i]    = original.data[i];
    }

    // Giống hệt -> MSE 0, PSNR vô cực
    CHECK(computeMse(original, recon, 4, 3) == 0.0);
    CHECK(std::isinf(computePsnr(original, recon, 4, 3)));

    // Mọi pixel lệch 1 -> MSE 1, PSNR = 10 * log10(255^2) = 48.1308 dB
    for (uint8_t& value : recon.data) value = static_cast<uint8_t>(value + 1);
    CHECK_NEAR(computeMse(original, recon, 4, 3), 1.0, 1e-12);
    CHECK_NEAR(computePsnr(original, recon, 4, 3), 48.130803608679, 1e-9);

    // Chỉ hàng cuối (hàng 2) lệch 10: vùng 4 x 2 bỏ qua hàng đó (giống việc bỏ phần đệm)
    for (size_t i = 0; i < original.data.size(); ++i) recon.data[i] = original.data[i];
    for (int col = 0; col < 4; ++col) recon.data[recon.getIndex(2, col)] = static_cast<uint8_t>(original.getPixel(2, col) + 10);
    CHECK(computeMse(original, recon, 4, 2) == 0.0);
    CHECK_NEAR(computeMse(original, recon, 4, 3), 4 * 100.0 / 12, 1e-12);
    return testResult("metrics");
}
