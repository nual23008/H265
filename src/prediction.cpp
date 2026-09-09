#include "prediction.h"
#include "lib.h"
#include "common.h"

#include <iostream>
#include <cstdint>

using namespace std;

uint8_t calculateDC(Pixel* top, Pixel* left, int block_size) {
    int DC_value = 0;
    
    for (int i = 0; i < block_size; i++) {
        DC_value += top[i].data;
        DC_value += left[i].data;
    }

    DC_value = (DC_value + block_size) / (2 * block_size);

    return (uint8_t)DC_value;
}

void DCpredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size) {
    int DC_value = calculateDC(top, left, block_size);
    
    for (int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int idx = row * block_size + col;

            prediction_block->data[idx].data = DC_value;
        }
    }
}

