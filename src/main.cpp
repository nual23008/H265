#include "common_types.h"
#include "prediction.h"
#include "bitstream.h"
#include "cabac.h"
#include "lib.h"
#include "quantization.h"
#include "residual.h"
#include "transform.h"

#include <cstdint>
#include <fstream>
#include <iostream>

int main() {
    //=================================================================================
    // Tạo frame
    Frame* frame = new Frame{};

    frame->planeY = new Pixel[16]{};
    frame->planeU = nullptr;
    frame->planeV = nullptr;

    for (int i = 0; i < 16; ++i) {
        frame->planeY[i].data = static_cast<uint8_t>(i);
        frame->planeY[i].available = true;
    }

    // for (int i = 0; i < 16; ++i) {
    //     cout
    //         << static_cast<int>(frame->planeY[i].data)
    //         << ' ';

    //     if ((i + 1) % 4 == 0) {
    //         cout << '\n';
    //     }
    // }
    //=================================================================================




    for (int i = 0; i < 4; i+=2) {
        for (int j = 0; j < 4; j+=2) {
            // Lấy original block
            Block* original_block = CreateBlock(2);
            original_block = GetBlock(frame->planeY, j, i, 2);
            Pixel* top = new Pixel[2];
            Pixel* left = new Pixel[2];

            // lấy top, left
            GetTopReference(frame->planeY, top, 4, 4, j, i, 2);
            GetLeftReference(frame->planeY, left, 4, 4, j, i, 2);

                        // cout << "TOP: ";

                        // for (int k = 0; k < 2; k++) {
                        //     cout << static_cast<int>(top[k].data) << " ";
                        // }
                        // cout << "\n";

                        // cout << "LEFT" << "\n";

                        // for (int k = 0; k < 2; k++) {
                        //     cout << static_cast<int>(left[k].data) << "\n";
                        // }


                        // cout << "\n";

            Block* prediction_block = new Block;
            prediction_block = CreateBlock(2);

            DCpredictionBlock(prediction_block, top, left, 2);

                        // for (int k = 0; k < 2; k++) {
                        //     for (int g = 0; g < 2; g++) {
                        //         cout << static_cast<int>(prediction_block->data[i].data) << " ";
                        //     }
                        //     cout << "\n";
                        // }
            
            Block* residual = CreateBlock(2);
            residual = Residual(original_block, prediction_block, 2);

            for (int k = 0; k < 2; k++) {
                for (int g = 0; g < 2; g++) {
                    cout << static_cast<int>(residual->data[i].data) << " ";
                }

                cout << "\n";
            }



            delete[] top;
            delete[] frame->planeY;
            delete frame;
        }
    }
    return 0;
}

