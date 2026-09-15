// include/rate.h
// Ước lượng số bit (rate) khi chưa có bộ mã hoá entropy thật (CABAC).
#pragma once

#include <cstdint>
#include <map>
#include <vector>

// Đếm số lần xuất hiện của từng giá trị (vd: mọi level của một frame)
struct Histogram {
    std::map<int32_t, long long> counts;   // giá trị -> số lần xuất hiện
    long long total = 0;                   // tổng số giá trị đã đếm

    void add(int32_t value);
    void add(const std::vector<int32_t>& values);
};

// Số bit nếu mã hoá từng giá trị ĐỘC LẬP bằng bộ mã entropy lý tưởng:
//   bits = sum_v count(v) * (-log2 p(v)),   p(v) = count(v) / total
// Không dùng ngữ cảnh (vị trí hệ số, giá trị lân cận) như CABAC, nên chỉ để so sánh TƯƠNG ĐỐI
// giữa các QP / các cách dự đoán, không so được với bitstream HM thật.
double entropyBits(const Histogram& histogram);
