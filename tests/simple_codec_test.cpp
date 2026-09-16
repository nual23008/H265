#include "prediction.h"
#include "transform.h"
#include "lib.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>

void TestReferencesAndPrediction()
{
    const int width = 16;
    Plane reconstructed;
    reconstructed.width = width;
    reconstructed.height = 16;
    reconstructed.data.assign(16 * 16, 0);

    for (int i = 0; i < 8; i++) {
        reconstructed.data[7 * width + 8 + i] = static_cast<uint8_t>(10 + i);
        reconstructed.data[(8 + i) * width + 7] = static_cast<uint8_t>(30 + i);
    }

    std::vector<uint8_t> top =
        GetTopReference(reconstructed, 8, 8, 8);
    std::vector<uint8_t> left =
        GetLeftReference(reconstructed, 8, 8, 8);

    assert(top.size() == 16);
    assert(left.size() == 16);

    for (int i = 0; i < 8; i++) {
        assert(top[i] == 10 + i);
        assert(left[i] == 30 + i);
    }

    std::vector<uint8_t> vertical = IntraPrediction(reconstructed, 8, 8, 8, MODE_VERTICAL);
    std::vector<uint8_t> horizontal = IntraPrediction(reconstructed, 8, 8, 8, MODE_HORIZONTAL);

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (col > 0) {
                assert(vertical[row * 8 + col] == top[col]);
            }
            if (row > 0) {
                assert(horizontal[row * 8 + col] == left[row]);
            }
        }
    }

    int mode = EstimateIntraMode(vertical, reconstructed, 8, 8, 8);
    assert(mode == MODE_VERTICAL);

    mode = EstimateIntraMode(horizontal, reconstructed, 8, 8, 8);
    assert(mode == MODE_HORIZONTAL);

    top = GetTopReference(reconstructed, 0, 0, 8);
    left = GetLeftReference(reconstructed, 0, 0, 8);
    for (int i = 0; i < 16; i++) {
        assert(top[i] == 128);
        assert(left[i] == 128);
    }
}

void TestReconstructionAndPSNR()
{
    const int width = 16;
    const int height = 16;
    Plane original;
    original.width = width;
    original.height = height;
    original.data.resize(width * height);

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            original.data[row * width + col] =
                static_cast<uint8_t>((20 + row * 31 + col * 17 + row * col * 3) % 256);
        }
    }

    Plane reconstructed = ReconstructPlane(original, 16, 8);

    double psnr = PSNR(original, reconstructed);
    assert(reconstructed.data.size() == original.data.size());
    assert(std::isfinite(psnr));
    assert(psnr > 20.0);
}

void TestIntegerTransformHEVC()
{
    std::vector<int16_t> residual(64, 1);
    std::vector<int16_t> coefficients = Transform8x8(residual);

    assert(coefficients[0] == 128);
    for (int i = 1; i < 64; i++) {
        assert(coefficients[i] == 0);
    }

    std::vector<int16_t> decodedResidual = InverseTransform8x8(coefficients);
    for (int i = 0; i < 64; i++) {
        assert(decodedResidual[i] == 1);
    }
}

int main()
{
    TestReferencesAndPrediction();
    TestIntegerTransformHEVC();
    TestReconstructionAndPSNR();
    std::cout << "Tat ca test da pass.\n";
    return 0;
}
