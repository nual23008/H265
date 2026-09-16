#include "quantization.h"

#include <cstdint>

std::vector<int16_t> Quantize(const std::vector<int16_t>& coefficients, int quantStep) {
    std::vector<int16_t> levels(64);

    for (int i = 0; i < 64; i++) {
        if (coefficients[i] >= 0) {
            levels[i] = (coefficients[i] + quantStep / 2) / quantStep;
        } else {
            levels[i] = -(-coefficients[i] + quantStep / 2) / quantStep;
        }
    }

    return levels;
}

std::vector<int16_t> Dequantize(const std::vector<int16_t>& levels, int quantStep) {
    std::vector<int16_t> coefficients(64);

    for (int i = 0; i < 64; i++) {
        coefficients[i] = levels[i] * quantStep;
    }

    return coefficients;
}
