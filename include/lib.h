#pragma once
// TODO: khai báo hàm intra/inter prediction
#include <cstdint>
#include <vector>
#include <iostream>

using namespace std;

struct Frame
{
    int frame_width;
    int frame_height;

    uint8_t* data;
};


struct Block
{
    int block_size;

    uint8_t* data;
};

Frame* CreateFrame(int height, int width);
void DeleteFrame(Frame* frame);
Block* CreateBlock(int block_size);
void DeleteBlock(Block* block);
// bool ReadYUV(istream& input, Frame& frame);
void GetReferencePixel(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left);
