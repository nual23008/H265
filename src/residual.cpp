#include "residual.h"

std::vector<int16_t> Residual(const std::vector<uint8_t>& originalBlock, const std::vector<uint8_t>& predictionBlock) {
    std::vector<int16_t> residual(64);

    for (int i = 0; i < 64; i++) {
        residual[i] = static_cast<int16_t>(originalBlock[i]) - static_cast<int16_t>(predictionBlock[i]);
    }

    return residual;
}
