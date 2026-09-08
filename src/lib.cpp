#include "lib.h"
#include "common.h"
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
    delete[] pixel;
}

Frame* CreateFrame(int width, int height) {
    Frame* frame = new Frame;

    int Y_size = width * height;
    int UV_size = (width / 2) * (height / 2);

    frame->planeY = new Pixel[Y_size, 0];
    frame->planeU = new Pixel[UV_size, 0];
    frame->planeV = new Pixel[UV_size, 0];

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

void GetTopReference(const Pixel* plane, Pixel* top, int width, int height, int block_x, int block_y, int block_size) {
    //Xét trong một block
    for (int col = 0; col < block_size; col++) {
        // Trường hợp ở góc trên bên trái frame
        if (block_x == 0 && block_y == 0) {
            top[col].data = 128;
        }
        // Trường hợp ở mép trên
        else if (block_x > 0 && block_y == 0) {
            int frame_idx = block_y * width + (block_x - 1);
            top[col].data = plane[frame_idx].data;
        }
        // Trường hợp ra số pixel vượt ngoài frame
        else if ((block_x + col) >= width) {
            top[col].data = 0;
            top[col].available = false;
        }
        // Trường hợp bình thường ở giữa frame
        else {
            int frame_idx = ((block_x + col) + ((block_y - 1) * width));
            top[col].data = plane[frame_idx].data;
        }
    }
}

void GetLeftReference(const Pixel* plane, Pixel* left, int width, int height, int block_x, int block_y, int block_size) {
    //Xét trong một block
    for (int row = 0; row < block_size; row++) {
        // Trường hợp ở góc trên bên trái frame
        if (block_x == 0 && block_y == 0) {
            left[row].data = 128;
        }
        // Trường hợp ở mép trái 
        else if (block_x == 0 && block_y > 0) {
            int frame_idx = (block_y - 1) * width + block_x;
            left[row].data = plane[frame_idx].data;
        }
        // Trường hợp ra số pixel vượt ngoài frame
        else if ((block_y + row) >= height) {
            left[row].data = 0;
            left[row].available = false;
        }
        // Trường hợp bình thường ở giữa frame
        else {
            int frame_idx = ((block_x - 1) + ((block_y + row) * width));
            left[row].data = plane[frame_idx].data;
        }
    }
}
