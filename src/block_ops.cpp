// src/block_ops.cpp
#include "block_ops.h"

#include <cstdlib>   // std::abs

// Residual = Original - Prediction. Kiểu int32_t có dấu vì kết quả có thể âm.
std::vector<int32_t> computeResidual(const std::vector<int32_t>& original, const std::vector<int32_t>& prediction) {
    std::vector<int32_t> residual(original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        residual[i] = original[i] - prediction[i];
    }
    return residual;
}

// SAD (Sum of Absolute Differences): tổng trị tuyệt đối residual. Càng nhỏ -> dự đoán càng tốt.
long long sumAbsolute(const std::vector<int32_t>& block) {
    long long sum = 0;
    for (int32_t value : block) {
        sum += std::abs(value);
    }
    return sum;
}

// Phương sai (variance) của block: đo mức độ "chi tiết".
// Block phẳng -> variance gần 0; block có cạnh/texture -> variance lớn.
double blockVariance(const std::vector<int32_t>& block) {
    double sum = 0.0;
    double sumSquare = 0.0;
    for (int32_t value : block) {
        sum += value;
        sumSquare += static_cast<double>(value) * value;
    }
    double mean = sum / block.size();
    return sumSquare / block.size() - mean * mean;   // E[x^2] - (E[x])^2
}
