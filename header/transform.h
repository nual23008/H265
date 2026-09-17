// include/dct.h
// DCT số nguyên của HEVC cho block N x N, N ∈ {4, 8, 16, 32}. Encoder dùng N = 16 (luma) và N = 8 (chroma).
//
//   residual --[forwardDct]--> coeff --[quant]--> level --[dequant]--> coeff' --[inverseDct]--> residual'
//
// Block là vector<int16_t> N*N phần tử xếp theo hàng (phần tử [r * N + c]).
#pragma once

#include <cstdint>
#include <vector>

// Phần tử hàng k, cột n của ma trận DCT N x N. Hàng k là hàm cơ sở tần số k:
// hàng 0 toàn 64 (DC), hàng càng lớn càng đổi dấu nhiều lần (tần số cao).
int dctCoef(int N, int k, int n);

// Biến đổi thuận: coeff = T * X * T^T, kèm 2 lần dịch phải có làm tròn.
// Block phẳng giá trị v -> chỉ coeff[0] khác 0 và bằng 128 * v (với mọi N).
std::vector<int16_t> Transform8x8(const std::vector<int16_t>& residual, int N);

// Biến đổi nghịch: residual' = T^T * coeff * T, kèm 2 lần dịch phải và chặn về 16-bit.
// inverseDct(forwardDct(x)) gần bằng x (sai lệch nhỏ do làm tròn).
std::vector<int16_t> InverseTransform8x8(const std::vector<int16_t>& coeff, int N);
