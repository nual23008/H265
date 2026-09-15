// tools/debug_print.cpp
#include "debug_print.h"

#include <iomanip>    // std::setw
#include <iostream>   // std::cout

void printStats(const char* name, const Plane& plane) {
    uint8_t minVal = 255;
    uint8_t maxVal = 0;
    long long sum = 0;

    for (size_t i = 0; i < plane.data.size(); ++i) {
        if (plane.data[i] < minVal) minVal = plane.data[i];
        if (plane.data[i] > maxVal) maxVal = plane.data[i];
        sum += plane.data[i];
    }

    double mean = static_cast<double>(sum) / plane.data.size();
    std::cout << name << " plane: min = " << (int)minVal << ", max = " << (int)maxVal << ", mean = " << mean << std::endl;
}

void printBlock(const char* title, const std::vector<int32_t>& block, int N, int width) {
    std::cout << title << std::endl;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            std::cout << std::setw(width) << block[r * N + c];
        }
        std::cout << std::endl;
    }
}

void printRange(const char* label, const std::vector<int32_t>& values, int begin, int end) {
    std::cout << "  " << label << ":";
    for (int i = begin; i < end; ++i) {
        std::cout << std::setw(4) << values[i];
    }
    std::cout << std::endl;
}

void printRefSamples(const char* title, const RefSamples& ref) {
    int N = ref.N;
    std::cout << title << std::endl;
    std::cout << "  corner      :" << std::setw(4) << ref.corner << std::endl;
    printRange("top         ", ref.top,  0, N);
    printRange("top-right   ", ref.top,  N, 2 * N);
    printRange("left        ", ref.left, 0, N);
    printRange("bottom-left ", ref.left, N, 2 * N);
}
