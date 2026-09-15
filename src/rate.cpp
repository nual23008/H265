// src/rate.cpp
#include "rate.h"

#include <cmath>   // std::log2

void Histogram::add(int32_t value) {
    ++counts[value];   // giá trị chưa có trong map sẽ được tạo với count = 0 rồi tăng lên 1
    ++total;
}

void Histogram::add(const std::vector<int32_t>& values) {
    for (int32_t value : values) {
        add(value);
    }
}

double entropyBits(const Histogram& histogram) {
    double bits = 0.0;
    for (const auto& [value, count] : histogram.counts) {
        (void)value;                                               // chỉ cần count
        double probability = static_cast<double>(count) / histogram.total;
        bits += count * -std::log2(probability);                   // giá trị càng hiếm càng tốn nhiều bit
    }
    return bits;
}
