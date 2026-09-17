#pragma once

#include <vector>
#include <cstdint>

std::vector<int16_t> Transform8x8(const std::vector<int16_t>& residual);

std::vector<int16_t> InverseTransform8x8(const std::vector<int16_t>& coefficients);