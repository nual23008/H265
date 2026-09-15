// src/metrics.cpp
#include "metrics.h"

#include <cmath>    // std::log10
#include <limits>   // std::numeric_limits

double computeMse(const Plane& original, const Plane& recon, int width, int height) {
    long long sse = 0;   // tổng bình phương sai số; 1920*1080*255^2 ≈ 1.3e11 nên cần long long
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            long long diff = original.getPixel(row, col) - recon.getPixel(row, col);
            sse += diff * diff;
        }
    }
    return static_cast<double>(sse) / (static_cast<double>(width) * height);
}

double computePsnr(const Plane& original, const Plane& recon, int width, int height) {
    double mse = computeMse(original, recon, width, height);
    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return 10.0 * std::log10(255.0 * 255.0 / mse);
}
