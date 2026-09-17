#pragma once

#include <vector>
#include <cstdint>

std::vector<int16_t> Quantize(const std::vector<int16_t>& coefficients, int quantStep);

std::vector<int16_t> Dequantize(const std::vector<int16_t>& levels, int quantStep);
