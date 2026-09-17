#pragma once

#include "lib.h"

#include <vector>

const int MODE_PLANAR = 0;
const int MODE_DC = 1;
const int MODE_HORIZONTAL = 10;
const int MODE_VERTICAL = 26;

std::vector<uint8_t> GetTopReference(const Plane& reconstructedFrame, int blockRow, int blockCol, int blockSize);

std::vector<uint8_t> GetLeftReference(const Plane& reconstructedFrame, int blockRow, int blockCol, int blockSize);

uint8_t GetTopLeftReference(const Plane& reconstructedFrame, int blockRow, int blockCol, const std::vector<uint8_t>& top, const std::vector<uint8_t>& left);

int ClipPrediction(int value);

int FloorDivide(int value, int divisor);

std::vector<uint8_t> PlanarPrediction(const std::vector<uint8_t>& top, const std::vector<uint8_t>& left, int blockSize);

std::vector<uint8_t> DCPrediction(const std::vector<uint8_t>& top, const std::vector<uint8_t>& left, int blockSize);

std::vector<uint8_t> AngularPrediction(const std::vector<uint8_t>& top, const std::vector<uint8_t>& left, uint8_t topLeft, int blockSize, int mode);

std::vector<uint8_t> IntraPrediction(const Plane& reconstructedFrame, int blockRow, int blockCol, int blockSize,int mode);

int CalculateSAD(const std::vector<uint8_t>& originalBlock, const std::vector<uint8_t>& predictionBlock, int block_size);

int EstimateIntraMode(const std::vector<uint8_t>& originalBlock, const Plane& reconstructedPlane, int blockRow, int blockCol, int blockSize);
