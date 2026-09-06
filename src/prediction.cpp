#include "prediction.h"
#include "lib.h"

#include <iostream>
#include <cstdint>

using namespace std;

uint8_t calculateDC(uint8_t* top, uint8_t* left, int block_size) {
    int DC_value = 0;

    for (int i = 0; i < block_size; i++) {
        DC_value += top[i];
        DC_value += left[i];
    }

    DC_value = (DC_value + block_size) / (2 * block_size);

    return (uint8_t)DC_value;
}

void DCpredictionBlock(Block* prediction_block, uint8_t DC_value) {
    for (int block_h = 0; block_h < prediction_block->block_size; block_h++) {
        for (int block_w = 0; block_w < prediction_block->block_size; block_w++) {
            int prediction_block_idx = block_h * prediction_block->block_size + block_w;

            prediction_block->data[prediction_block_idx] = DC_value;
        }
    }
}

void DCpredictionFrame(Frame* frame, Frame* prediction_frame, int block_size) {
    Block* block = CreateBlock(block_size);
    uint8_t* top = new uint8_t[block_size];
    uint8_t* left = new uint8_t[block_size];
    int x_block;
    int y_block;

    for (y_block = 0; y_block < frame->frame_height; y_block = y_block + block_size) {
        for(x_block = 0; x_block < frame->frame_width; x_block = x_block + block_size) {
            GetReferencePixelY(frame, block, x_block, y_block, top, left);
            uint8_t DC_value = calculateDC(top, left, block_size);
            DCpredictionBlock(block, DC_value);

            for (int block_h = 0; block_h < block_size; block_h++) {
                for (int block_w = 0; block_w < block_size; block_w++) {
                    int block_idx = block_h * block_size + block_w;
                    int x_prediction = x_block + block_w;
                    int y_prediction = y_block + block_h;
                    int prediction_idx = y_prediction * prediction_frame->frame_width + x_prediction;

                    prediction_frame->Y[prediction_idx] = block->data[block_idx];
                }
            }
        }
    }

    for (y_block = 0; y_block < (frame->frame_height / 2); y_block = y_block + block_size) {
        for(x_block = 0; x_block < (frame->frame_width / 2); x_block = x_block + block_size) {
            GetReferencePixelU(frame, block, x_block, y_block, top, left);
            uint8_t DC_value = calculateDC(top, left, block_size);
            DCpredictionBlock(block, DC_value);

            for (int block_h = 0; block_h < block_size; block_h++) {
                for (int block_w = 0; block_w < block_size; block_w++) {
                    int block_idx = block_h * block_size + block_w;
                    int x_prediction = x_block + block_w;
                    int y_prediction = y_block + block_h;
                    int prediction_idx = y_prediction * (prediction_frame->frame_width / 2) + x_prediction;

                    prediction_frame->U[prediction_idx] = block->data[block_idx];
                }
            }
        }
    }

    for (y_block = 0; y_block < (frame->frame_height / 2); y_block = y_block + block_size) {
        for(x_block = 0; x_block < (frame->frame_width / 2); x_block = x_block + block_size) {
            GetReferencePixelV(frame, block, x_block, y_block, top, left);
            uint8_t DC_value = calculateDC(top, left, block_size);
            DCpredictionBlock(block, DC_value);

            for (int block_h = 0; block_h < block_size; block_h++) {
                for (int block_w = 0; block_w < block_size; block_w++) {
                    int block_idx = block_h * block_size + block_w;
                    int x_prediction = x_block + block_w;
                    int y_prediction = y_block + block_h;
                    int prediction_idx = y_prediction * (prediction_frame->frame_width / 2) + x_prediction;

                    prediction_frame->V[prediction_idx] = block->data[block_idx];
                }
            }
        }
    }

    delete[] top;
    delete[] left;
    DeleteBlock(block);
}