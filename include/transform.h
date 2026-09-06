// include/transform.h
#pragma once
#include "common.h"

// Residual --> [Transform] --> Coefficient --> [Quantization] --> Level
// Level --> [Dequantization] --> Coefficient' --> [Inverse Transform] --> Residual'

// Biến đổi thuận (DCT số nguyên 8x8 của HEVC): residual -> hệ số.
// Vào : residual, giá trị nằm trong [-255, 255] với ảnh 8-bit
// Ra  : hệ số, đã bị phóng đại 2^transformShift so với DCT trực chuẩn
Blk forwardTransform(const Blk& residual);

// Biến đổi nghịch: hệ số -> residual khôi phục.
Blk inverseTransform(const Blk& coeff);

const int32_t* transformMatrix8();   // trỏ tới mảng phẳng 8*8
