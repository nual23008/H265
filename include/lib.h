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

    uint8_t* Y;
    uint8_t* U;
    uint8_t* V;
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
bool ReadYUV(istream& input, Frame& frame);
void GetBlockY(Frame* frame, Block* block, int x_block, int y_block);
void GetBlockU(Frame* frame, Block* block, int x_block, int y_block);
void GetBlockV(Frame* frame, Block* block, int x_block, int y_block);
void GetReferencePixelY(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left);
void GetReferencePixelU(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left);
void GetReferencePixelV(Frame* frame, Block* block, int x_block, int y_block, uint8_t* top, uint8_t* left);
