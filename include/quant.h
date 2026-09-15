// include/quant.h
// Lượng tử hoá (quantization) và giải lượng tử (dequantization) theo QP cho block N x N sau DCT.
//
//   coeff --[quantize]--> level (số nguyên nhỏ, phần lớn bằng 0) --[dequantize]--> coeff' ≈ coeff
//
// QP ∈ [0, 51]. Qstep(QP) ≈ 2^((QP - 4) / 6): QP tăng 6 thì Qstep gấp đôi, QP = 4 thì Qstep = 1.
// Block là vector<int32_t> N*N phần tử xếp theo hàng, N ∈ {4, 8, 16, 32} (giống dct.h).
#pragma once

#include <cstdint>
#include <vector>

// Bước lượng tử dạng số thực, chỉ dùng để in và kiểm tra
double qStep(int qp);

// level ≈ coeff / (Qstep * 128/N), làm tròn kiểu dead-zone (cộng 1/3 thay vì 1/2).
// 128/N là phần phóng đại thừa của forwardDct.
std::vector<int32_t> quantize(const std::vector<int32_t>& coeff, int qp, int N);

// coeff' = level * Qstep * 128/N
std::vector<int32_t> dequantize(const std::vector<int32_t>& level, int qp, int N);
