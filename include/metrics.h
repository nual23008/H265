// include/metrics.h
// Đo chất lượng ảnh tái tạo so với ảnh gốc.
#pragma once

#include "picture.h"   // Plane

// MSE (Mean Squared Error) trên vùng width x height ở góc trên-trái của hai plane.
// Truyền kích thước ảnh THẬT (vd 1920 x 1080) để bỏ qua phần đệm của recon.
double computeMse(const Plane& original, const Plane& recon, int width, int height);

// PSNR = 10 * log10(255^2 / MSE), đơn vị dB. Càng cao càng giống ảnh gốc.
// MSE = 0 (hai ảnh giống hệt) -> trả về +vô cực.
double computePsnr(const Plane& original, const Plane& recon, int width, int height);
