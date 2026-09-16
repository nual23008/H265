#pragma once

#include <vector>
#include <cstdint>
#include <fstream>

struct Plane {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data; // Danh sach cac pixel trong plane

    int getIndex(int row, int col) const {
        return row * width + col;
    }

    int getPixel(int row, int col) const {
        return data[getIndex(row, col)];
    }
};

struct Picture {
    Plane Y;
    Plane U;
    Plane V;
};

std::vector<uint8_t> GetBlock8x8(const Plane& plane, int blockRow, int blockCol, int block_size);

int ClipPixel(int value);

std::vector<uint8_t> ReconstructBlock(const std::vector<uint8_t>& predictionBlock, const std::vector<int>& decodedResidual, int block_size);

void WriteBlock8x8(Plane& plane, int blockRow, int blockCol, const std::vector<uint8_t>& block, int block_size);

Plane ReconstructPlane(const Plane& originalPlane, int quantStep, int block_size);

double PSNR(const Plane& originalPlane, const Plane& reconstructedPlane);

bool ReadYUV420Frame(std::ifstream& input, int width, int height, Picture& picture);

bool WriteYUV420Frame(std::ofstream& output, const Picture& picture);
