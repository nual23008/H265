#pragma once

#include <lib.h>
#include <cstdint>

using namespace std;

uint8_t calculateDC(Pixel* top, Pixel* left, int block_size);
void DCpredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size);