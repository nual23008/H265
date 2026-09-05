#include "lib.h"
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

    frame->Y = new uint8_t[Y_size];
    frame->U = new uint8_t[UV_size];
    frame->V = new uint8_t[UV_size];

    return frame;
}

void DeleteFrame(Frame* frame) {
    delete[] frame->Y;
    delete[] frame->U;
    delete[] frame->V;
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

bool ReadYUV (istream& input, Frame& frame) {
    int Y_size = frame.frame_height * frame.frame_width;
    int CbCr_size  = (frame.frame_height * frame.frame_width) / 4;
    char value;

    for (int i = 0; i < Y_size; i++) {
        input.get(value);
        if (!input)
        {
            return false;
        }

        frame.Y[i] = (uint8_t)value;
    }

    for (int i = 0; i < CbCr_size; i++) {
        input.get(value);
        if (!input)
        {
            return false;
        }

        frame.U[i] = (uint8_t)value;
    }

    for (int i = 0; i < CbCr_size; i++) {
        input.get(value);
        if (!input)
        {
            return false;
        }

        frame.V[i] = (uint8_t)value;
    }

    return true;
}

void GetBlockY(Frame* frame, Block* block, int x_block, int y_block) {
    for (int block_h = 0; block_h < block->block_size; block_h++) {
        for (int block_w = 0; block_w < block->block_size; block_w++) {
            int block_x_idx = x_block + block_w;
            int block_y_idx = y_block + block_h;
            int block_idx = block_h * block->block_size + block_w;
            int frame_idx = block_y_idx * frame->frame_width + block_x_idx;

            if (block_x_idx >= frame->frame_width || 
                block_y_idx >= frame->frame_height) {
                block->data[block_idx] = 0;
            }
            else {
                block->data[block_idx] = frame->Y[frame_idx];   
            }
        }
    }
}

void GetBlockU(Frame* frame, Block* block, int x_block, int y_block) {
    for (int block_h = 0; block_h < block->block_size; block_h++) {
        for (int block_w = 0; block_w < block->block_size; block_w++) {
            int block_x_idx = x_block + block_w;
            int block_y_idx = y_block + block_h;
            int block_idx = block_h * block->block_size + block_w;
            int frame_idx = block_y_idx * (frame->frame_width / 2) + block_x_idx;

            if (block_x_idx >= (frame->frame_width / 2) || 
                block_y_idx >= (frame->frame_height / 2)) {
                block->data[block_idx] = 0;
            }
            else {
                block->data[block_idx] = frame->U[frame_idx];
            }
        }
    }
}

void GetBlockV(Frame* frame, Block* block, int x_block, int y_block) {
    for (int block_h = 0; block_h < block->block_size; block_h++) {
        for (int block_w = 0; block_w < block->block_size; block_w++) {
            int block_x_idx = x_block + block_w;
            int block_y_idx = y_block + block_h;
            int block_idx = block_h * block->block_size + block_w;
            int frame_idx = block_y_idx * (frame->frame_width / 2) + block_x_idx;

            if (block_x_idx >= (frame->frame_width / 2) || 
                block_y_idx >= (frame->frame_height / 2)) {
                block->data[block_idx] = 0;
            }
            else {
                block->data[block_idx] = frame->V[frame_idx];
            }
        }
    }
}

void GetReferencePixelY(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left) {
    for (int block_h = 0; block_h < block->block_size; block_h++) {
        int y_block_idx = y_block - 1;
        int x_block_idx = x_block + block_h;
        int frame_idx = y_block_idx * frame->frame_width + x_block_idx;

        if (y_block_idx < 0) {
            top[block_h] = 128;
        }
        else if (x_block_idx >= frame->frame_width) {
            top[block_h] = 128;
        }
        else {
            top[block_h] = frame->Y[frame_idx];
        }
    }

    for (int block_w = 0; block_w < block->block_size; block_w++) {
        int y_block_idx = y_block + block_w;
        int x_block_idx = x_block - 1;
        int frame_idx = y_block_idx * frame->frame_width + x_block_idx;

        if (x_block_idx < 0) {
            left[block_w] = 128;
        }
        else if (y_block_idx >= frame->frame_height) {
            left[block_w] = 128;
        }
        else {
            left[block_w] = frame->Y[frame_idx];
        }
    }
}

void GetReferencePixelU(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left) {
    for (int block_h = 0; block_h < block->block_size; block_h++) {
        int y_block_idx = y_block - 1;
        int x_block_idx = x_block + block_h;
        int frame_idx = y_block_idx * (frame->frame_width / 2) + x_block_idx;

        if (y_block_idx < 0) {
            top[block_h] = 128;
        }
        else if (x_block_idx >= (frame->frame_width / 2)) {
            top[block_h] = 128;
        }
        else {
            top[block_h] = frame->U[frame_idx];
        }
    }

    for (int block_w = 0; block_w < block->block_size; block_w++) {
        int y_block_idx = y_block + block_w;
        int x_block_idx = x_block - 1;
        int frame_idx = y_block_idx * (frame->frame_width / 2) + x_block_idx;

        if (x_block_idx < 0) {
            left[block_w] = 128;
        }
        else if (y_block_idx >= (frame->frame_height / 2)) {
            left[block_w] = 128;
        }
        else {
            left[block_w] = frame->U[frame_idx];
        }
    }
}

void GetReferencePixelV(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left) {
    for (int block_h = 0; block_h < block->block_size; block_h++) {
        int y_block_idx = y_block - 1;
        int x_block_idx = x_block + block_h;
        int frame_idx = y_block_idx * (frame->frame_width / 2) + x_block_idx;

        if (y_block_idx < 0) {
            top[block_h] = 128;
        }
        else if (x_block_idx >= (frame->frame_width / 2)) {
            top[block_h] = 128;
        }
        else {
            top[block_h] = frame->V[frame_idx];
        }
    }

    for (int block_w = 0; block_w < block->block_size; block_w++) {
        int y_block_idx = y_block + block_w;
        int x_block_idx = x_block - 1;
        int frame_idx = y_block_idx * (frame->frame_width / 2) + x_block_idx;

        if (x_block_idx < 0) {
            left[block_w] = 128;
        }
        else if (y_block_idx >= (frame->frame_height / 2)) {
            left[block_w] = 128;
        }
        else {
            left[block_w] = frame->V[frame_idx];
        }
    }
}