#pragma once

#include <lib.h>
#include <cstdint>

using namespace std;

uint8_t calculateDC(uint8_t* top, uint8_t* left, int block_size);
void DCpredictionBlock(Block* prediction_block, uint8_t DC_value);
