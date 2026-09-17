#include "residual.h"

std::vector<int16_t> Residual(const std::vector<uint8_t>& originalBlock, const std::vector<uint8_t>& predictionBlock, int block_size) {
    std::vector<int16_t> residual(block_size * block_size);

    for (int i = 0; i < block_size * block_size; i++) {
        residual[i] = static_cast<int16_t>(originalBlock[i]) - static_cast<int16_t>(predictionBlock[i]);
    }

    return residual;
}
