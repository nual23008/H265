#pragma once
// TODO: khai báo hàm intra/inter prediction
#include <cstdint>
#include <vector>
#include <iostream>

using namespace std;

#define FRAME_HEIGHT    1080
#define FRAME_WIDTH     1920

struct Pixel {
    uint8_t data;
    bool available;
};

struct Frame {
    Pixel* planeY;
    Pixel* planeU;
    Pixel* planeV;
};


struct Block
{
    Pixel* data;
};

// Frame* CreateFrame(int height, int width);
// void DeleteFrame(Frame* frame);
// Block* CreateBlock(int block_size);
// void DeleteBlock(Block* block);
// // bool ReadYUV(istream& input, Frame& frame);
// void GetReferencePixel(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left);
