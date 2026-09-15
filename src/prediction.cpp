#include "prediction.h"
#include "lib.h"

#include <iostream>
#include <cstdint>
#include <algorithm>

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

void PlanarPredictionBlock(Block* prediciton_block, Pixel* top, Pixel* left, int block_size) {
    for (int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int idx = row * block_size + col;
            int vertical    =   (block_size - 1 - row) * top[col].data + (row + 1) * left[block_size].data;
            int horizontal   =   (block_size - 1 - col) * left[row].data + (col + 1) * top[block_size].data;
            int value       =   (vertical + horizontal + block_size) / (2 * block_size);

            prediciton_block->data[idx].data = static_cast<uint8_t>(value);
        }
    }
}

void VerticalPredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size) {
    for (int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int idx = row * block_size + col;
            prediction_block->data[idx].data = top[col].data;
        }
    }
}

void HorizontalPredictionBlock(Block* prediction_block, Pixel* top, Pixel* left, int block_size) {
    for(int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int idx = row * block_size + col;
            prediction_block->data[idx].data = left[row].data;
        }
    }
}

uint64_t EstimateBlock(Block* original_block, Block* prediciton_block, int block_size) {
    const size_t number_of_pixel = static_cast<size_t>(block_size) * block_size;
    uint64_t sum = 0;

    for (size_t idx = 0; idx < number_of_pixel; idx++) {
        int diff = static_cast<int>(original_block->data[idx].data) - static_cast<int>(prediciton_block->data[idx].data);

        sum += static_cast<uint64_t>(diff * diff);
    }

    return sum;
}

Block* IntraPictureEstimate(Block* original_block, int* mode, Pixel* top, Pixel* left, int block_size) {
    Block* DC_prediction_block          = CreateBlock(block_size);
    Block* Planar_prediction_block      = CreateBlock(block_size);
    Block* Vertical_prediction_block    = CreateBlock(block_size);
    Block* Horizontal_prediciton_block  = CreateBlock(block_size);

    DCpredictionBlock(DC_prediction_block, top, left, block_size);
    PlanarPredictionBlock(Planar_prediction_block, top, left, block_size);
    VerticalPredictionBlock(Vertical_prediction_block, top, left, block_size);
    HorizontalPredictionBlock(Horizontal_prediciton_block, top, left, block_size);

    const uint64_t DC_estimate            =   EstimateBlock(original_block, DC_prediction_block, block_size);
    const uint64_t Planar_estimate        =   EstimateBlock(original_block, Planar_prediction_block, block_size);
    const uint64_t Vertical_estimate      =   EstimateBlock(original_block, Vertical_prediction_block, block_size);
    const uint64_t Horizontal_estimate    =   EstimateBlock(original_block, Horizontal_prediciton_block, block_size);

    uint64_t min_estimate = min({DC_estimate, Planar_estimate, Vertical_estimate, Horizontal_estimate});

    Block* best_block = nullptr;

    if (min_estimate == DC_estimate) {
        best_block = DC_prediction_block;
        *mode = MODE_DC;
        DeleteBlock(*(&Planar_prediction_block));
        DeleteBlock(*(&Vertical_prediction_block));
        DeleteBlock(*(&Horizontal_prediciton_block));
    }
    else if (min_estimate == Planar_estimate) {
        best_block = Planar_prediction_block;
        *mode = MODE_PLANAR;
        DeleteBlock(*(&DC_prediction_block));
        DeleteBlock(*(&Vertical_prediction_block));
        DeleteBlock(*(&Horizontal_prediciton_block));    
    }
    else if (min_estimate == Vertical_estimate) {
        best_block = Vertical_prediction_block;
        *mode = MODE_VERTICAL;
        DeleteBlock(*(&DC_prediction_block));
        DeleteBlock(*(&Planar_prediction_block));
        DeleteBlock(*(&Horizontal_prediciton_block));
    }
    else {
        best_block = Horizontal_prediciton_block;
        *mode = MODE_HORIZONTAL;
        DeleteBlock(*(&DC_prediction_block));
        DeleteBlock(*(&Planar_prediction_block));
        DeleteBlock(*(&Vertical_prediction_block));
    }

    return best_block;
}