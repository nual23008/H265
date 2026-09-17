#pragma once

#include <cstdint>
#include <vector>

std::vector<int16_t> Residual(const std::vector<uint8_t>& originalBlock, const std::vector<uint8_t>& predictionBlock, int block_size);
