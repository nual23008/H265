#include "common_types.h"
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

    Pixel* top = new Pixel[8];

    GetTopReference(frame->planeY, top, 4, 4, 0, 0, 4);

    for (int i = 0; i < 8; i++) {
        cout << static_cast<int>(top[i].data) << " ";
    }

    delete top;
    delete[] frame->planeY;
    delete frame;

    return 0;
}