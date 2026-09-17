#pragma once

#include <cstdint>
#include <vector>

std::vector<int16_t> Quantize(const std::vector<int16_t>& coeff, int qp, int N);

std::vector<int16_t> Dequantize(const std::vector<int16_t>& level, int qp, int N);
