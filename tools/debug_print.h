// tools/debug_print.h
// Các hàm in ra màn hình, chỉ dùng để học và debug. Encoder không cần tới module này.
#pragma once

#include <cstdint>
#include <vector>

#include "picture.h"      // Plane
#include "intra_pred.h"   // RefSamples

// In min / max / mean của một plane
void printStats(const char* name, const Plane& plane);

// In block N x N (xếp theo hàng), mỗi số rộng 5 ký tự
void printBlock(const char* title, const std::vector<int32_t>& block, int N);

// In các phần tử values[begin .. end-1] trên một dòng, có nhãn phía trước
void printRange(const char* label, const std::vector<int32_t>& values, int begin, int end);

// In 4N+1 mẫu tham chiếu: corner, top, top-right, left, bottom-left
void printRefSamples(const char* title, const RefSamples& ref);
