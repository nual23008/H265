#include "lib.h"
#include "common.h"
// TODO: implement
#include <iostream>
#include <cstdint>
#include <fstream>

using namespace std;

Frame* CreateFrame(int height, int width) {
    Frame* frame = new Frame;
    frame->frame_height = height;
    frame->frame_width = width;

    const int Y_size = frame->frame_height * frame->frame_width;
    const int UV_size = (frame->frame_height * frame->frame_width) / 4;

    frame->data = new uint8_t[Y_size + 2 * UV_size];
    return frame;
}

void DeleteFrame(Frame* frame) {
    delete[] frame->data;
    delete frame;
}

Block* CreateBlock(int block_size) {
    Block* block = new Block;
    
    block->block_size = block_size;

    const int number_of_block = block->block_size * block->block_size;

    block->data = new uint8_t[number_of_block];

    return block;
}

void DeleteBlock(Block* block) {
    delete[] block->data;
    delete block;
}

// bool ReadYUV (istream& input, Frame& frame) {
//     int Y_size = frame.frame_height * frame.frame_width;
//     int CbCr_size  = (frame.frame_height * frame.frame_width) / 4;
//     char value;

//     for (int i = 0; i < Y_size; i++) {
//         input.get(value);
//         if (!input)
//         {
//             return false;
//         }

//         frame.Y[i] = (uint8_t)value;
//     }

//     for (int i = 0; i < CbCr_size; i++) {
//         input.get(value);
//         if (!input)
//         {
//             return false;
//         }

//         frame.U[i] = (uint8_t)value;
//     }

//     for (int i = 0; i < CbCr_size; i++) {
//         input.get(value);
//         if (!input)
//         {
//             return false;
//         }

//         frame.V[i] = (uint8_t)value;
//     }

//     return true;
// }

void GetReferencePixel(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left) {

}

