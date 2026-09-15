#pragma once

#include <lib.h>
#include <cstdint>

using namespace std;

uint8_t calculateDC(Pixel* top, Pixel* left, int block_size);
void DCpredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size);

// Planar và IntraPictureEstimate cần top/left có block_size + 1 mẫu đã điền.
void PlanarPredictionBlock(Block* prediciton_block, Pixel* top, Pixel* left, int block_size);
void VerticalPredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size);
void HorizontalPredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size);
uint64_t EstimateBlock(Block* original_block, Block* prediciton_block, int block_size);
Block* IntraPictureEstimate(Block* original_block, int* mode, Pixel* top, Pixel* left, int block_size);

// Angular prediction cho pixel 8-bit, block vuong 4/8/16/32, mode 2..34.
// top[0] va left[0] la mau sat block; moi mang phai co du 2*block_size mau.
// top_left la mau goc chung, khong nam trong hai mang tren.
// Caller chuan bi mau da tai tao: thay mau thieu va loc tham chieu neu can.
// Ham khong doc Pixel::available. Dau ra phai co du block_size^2 pixel.
// filter_luma_boundary: bat cho luma thong thuong, tat cho chroma/demo hinh hoc.
void AngularPredictionBlock(Block* prediction_block, const Pixel* top, const Pixel* left, Pixel top_left, int block_size, int mode, bool filter_luma_boundary = true);
