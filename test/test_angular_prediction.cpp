#include "prediction.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestSize(int n) {
    std::vector<Pixel> top(2 * n, Pixel{77, true});
    std::vector<Pixel> left(2 * n, Pixel{77, true});
    std::vector<Pixel> pixels(n * n);
    Block block{pixels.data()};
    for (int mode = 2; mode <= 34; ++mode) {
        AngularPredictionBlock(&block, top.data(), left.data(), {77, true}, n, mode);
        for (const Pixel& p : pixels) {
            Check(p.data == 77 && p.available, "Constant reference must stay constant");
        }
    }

    for (int i = 0; i < 2 * n; ++i) {
        top[i].data = static_cast<uint8_t>(20 + i);
        left[i].data = static_cast<uint8_t>(120 + i);
    }
    for (int mode : {2, 10, 18, 26, 34}) {
        AngularPredictionBlock(&block, top.data(), left.data(), {90, true}, n, mode, false);
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                int expected = 0;
                if (mode == 2) expected = left[y + x + 1].data;
                if (mode == 10) expected = left[y].data;
                if (mode == 26) expected = top[x].data;
                if (mode == 34) expected = top[x + y + 1].data;
                if (mode == 18) {
                    expected = x == y ? 90 :
                        (x > y ? top[x - y - 1].data : left[y - x - 1].data);
                }
                Check(pixels[y * n + x].data == expected, "Axis/diagonal projection mismatch");
            }
        }
    }

    // Hoan doi hai canh va mode doi xung phai cho ma tran chuyen vi.
    std::vector<Pixel> transposed_pixels(n * n);
    Block transposed{transposed_pixels.data()};
    for (int mode = 2; mode <= 34; ++mode) {
        AngularPredictionBlock(&block, top.data(), left.data(), {90, true}, n, mode, false);
        AngularPredictionBlock(&transposed, left.data(), top.data(), {90, true}, n, 36 - mode, false);
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                Check(pixels[y * n + x].data == transposed_pixels[x * n + y].data,
                      "Angular transpose symmetry mismatch");
            }
        }
    }
}

void TestInterpolationAndBoundary() {
    std::vector<Pixel> top(8, Pixel{32, true}), left(8, Pixel{0, true}), pixels(16);
    top[1].data = 64;
    Block block{pixels.data()};
    AngularPredictionBlock(&block, top.data(), left.data(), {0, true}, 4, 27, false);
    Check(pixels[0].data == 34, "Positive fractional interpolation mismatch");
    AngularPredictionBlock(&block, top.data(), left.data(), {0, true}, 4, 25, false);
    Check(pixels[0].data == 30, "Negative fractional interpolation mismatch");
    AngularPredictionBlock(&block, top.data(), left.data(), {255, true}, 4, 26);
    Check(pixels[0].data == 0, "Lower clipping failed");
    left[0].data = 255;
    top[0].data = 255;
    AngularPredictionBlock(&block, top.data(), left.data(), {0, true}, 4, 26);
    Check(pixels[0].data == 255, "Upper clipping failed");
    top[0].data = 100;
    left[0].data = 99;
    AngularPredictionBlock(&block, top.data(), left.data(), {100, true}, 4, 26);
    Check(pixels[0].data == 99, "Negative boundary difference must round down");
    for (int mode : {0, 1, 35}) {
        bool rejected = false;
        try { AngularPredictionBlock(&block, top.data(), left.data(), {0, true}, 4, mode); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Invalid angular mode accepted");
    }
}
}

int main() {
    for (int n : {4, 8, 16, 32}) TestSize(n);
    TestInterpolationAndBoundary();
    std::cout << "Angular prediction tests passed\n";
}
