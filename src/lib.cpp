#include "lib.h"
// TODO: implement
#include <iostream>
#include <cstdint>
#include <fstream>

using namespace std;

Pixel* CreatePixel(uint8_t data) {
    Pixel* pixel = new Pixel;
    pixel->data = data;
    pixel->available = true;

    return pixel;
}

void DeletePixel(Pixel* pixel) {
    delete pixel;
}

Frame* CreateFrame(int width, int height) {
    Frame* frame = new Frame;

    int Y_size      =    width * height;
    int UV_size     =   (width / 2) * (height / 2);

    frame->planeY = new Pixel[Y_size]{};
    frame->planeU = new Pixel[UV_size]{};
    frame->planeV = new Pixel[UV_size]{};

    return frame;
}

void DeleteFrame(Frame* frame) {
    delete[] frame->planeY;
    delete[] frame->planeU;
    delete[] frame->planeV;
    delete frame;
}

Block* CreateBlock(int block_size) {
    Block* block = new Block;

    block->data = new Pixel[block_size * block_size];
    for (int i = 0; i < block_size * block_size; i++) {
        block->data[i].available = true;
    }

    return block;
}

void DeleteBlock(Block* block) {
    delete[] block->data;
    delete block;
}

Block* GetBlock(const Pixel* plane, int block_x, int block_y, int frame_width, int frame_height, int block_size) {
    if (block_x + block_size > frame_width || block_y + block_size > frame_height) {return nullptr; }
    Block* block = CreateBlock(block_size);
    for (int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int plane_idx = (block_x + col) + (block_y + row) * frame_width;
            int block_idx = col + row * block_size;

            block->data[block_idx] = plane[plane_idx];
        }
    }

    return block;
}

void GetTopReference(const Pixel* plane, Pixel* top, int width, int height, int block_x, int block_y, int block_size) {
    for (int col = 0; col <= block_size; col++) {
        if (block_y == 0) {
            // Trường hợp ở góc trên bên trái
            top[col].data = (block_x == 0) ? 128 : plane[block_x - 1].data;
            top[col].available = false;
        }
        else {
            int x = block_x + col;
            // Trường hợp vượt ngoài biên 
            if (x >= width) {
                int idx = (width - 1) + (block_y - 1) * width;
                top[col].data = plane[idx].data;
                top[col].available = false;                                
            }
            // Trường hợp bình thường
            else {
                int idx = (block_x + col) + (block_y - 1) * width;
                top[col] = plane[idx];
            }
        }
    }
}

void GetLeftReference(const Pixel* plane, Pixel* left, int width, int height, int block_x, int block_y, int block_size) {
    for (int row = 0; row <= block_size; row++) {
        if (block_x == 0) {
            // Trường hợp ở góc trên bên trái
            left[row].data = (block_y == 0) ? 128 : plane[(block_y - 1) * width].data;
            left[row].available = false;
        }
        else {
            int y = block_y + row;
            // Trường hợp vượt ngoài biên
            if (y >= height) {
                int idx = (block_x - 1) + (height - 1) * width;
                left[row].data = plane[idx].data;
                left[row].available = false;
            }
            // Trường hợp bình thường
            else {
                int idx = (block_x - 1) + (block_y + row) * width;
                left[row] = plane[idx];
            }
        }
    }
}
