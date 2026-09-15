#pragma once
// TODO: khai báo hàm intra/inter prediction
#include <cstdint>
#include <vector>
#include <iostream>

using namespace std;

#define FRAME_HEIGHT    1080
#define FRAME_WIDTH     1920

enum IntraMode {
    MODE_DC = 0,
    MODE_PLANAR = 1,
    MODE_VERTICAL = 2,
    MODE_HORIZONTAL = 3
};

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

Pixel* CreatePixel(uint8_t data);
void DeletePixel(Pixel* pixel);
Frame* CreateFrame(int width, int height);
void DeleteFrame(Frame* frame);
Block* CreateBlock(int block_size);
void DeleteBlock(Block* block);
Block* GetBlock(const Pixel* plane, int block_x, int block_y, int frame_width, int frame_height, int block_size);
// top/left phải có ít nhất block_size + 1 phần tử; hai hàm điền cả mẫu cuối.
// Mẫu vượt mép phải/dưới lặp lại giá trị trước đó, available = false.
void GetTopReference(const Pixel* plane, Pixel* top, int width, int height, int block_x, int block_y, int block_size);
void GetLeftReference(const Pixel* plane, Pixel* left, int width, int height, int block_x, int block_y, int block_size);
