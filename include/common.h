// include/common.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Kích thước block dùng cho demo: 8x8 
constexpr int kN        = 8;
constexpr int kLog2N    = 3;   // log2(8)
constexpr int kBitDepth = 8;

// Một block = mảng PHẲNG kN*kN
using Blk = std::vector<int32_t>;

// (x, y) -> chỉ số phẳng
inline int at(int x, int y, int n = kN) { return y * n + x; }

// Tạo block rỗng
inline Blk makeBlk(int n = kN) { return Blk(n * n, 0); }

// ---- Xuất dữ liệu ra file CSV (dùng cho debug)

void dumpCsv(const std::string& path, const Blk& b, int w = kN, int h = kN);
