// include/block_ops.h
// Phép toán trên block: vector<int32_t> gồm N*N phần tử xếp theo hàng (phần tử [r * N + c]).
#pragma once

#include <cstdint>
#include <vector>

// Residual = original - prediction (từng phần tử). Kết quả có thể âm.
std::vector<int32_t> computeResidual(const std::vector<int32_t>& original, const std::vector<int32_t>& prediction);

// Cộng từng phần tử: reconstruction = prediction + residual'. Chưa clip (writeBlock sẽ clip).
std::vector<int32_t> addBlocks(const std::vector<int32_t>& a, const std::vector<int32_t>& b);

// SAD: tổng trị tuyệt đối các phần tử. Càng nhỏ -> dự đoán càng tốt.
long long sumAbsolute(const std::vector<int32_t>& block);

// Phương sai: đo mức độ "chi tiết" của block (block phẳng -> gần 0).
double blockVariance(const std::vector<int32_t>& block);
