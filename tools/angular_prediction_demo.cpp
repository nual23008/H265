#include "prediction.h"

#include <array>
#include <iostream>

int main() {
    constexpr int n = 4;
    std::array<Pixel, 2 * n> top{}, left{};
    for (int i = 0; i < 2 * n; ++i) {
        top[i] = Pixel{static_cast<uint8_t>(10 * (i + 1)), true};
        left[i] = Pixel{static_cast<uint8_t>(100 + 10 * i), true};
    }
    const Pixel top_left{90, true};
    std::array<Pixel, n * n> pixels{};
    Block prediction{pixels.data()};

    for (int mode : {10, 26, 27, 34, 18}) {
        // Tat hieu chinh canh de de quan sat phep chieu theo huong.
        AngularPredictionBlock(&prediction, top.data(), left.data(),
                               top_left, n, mode, false);
        std::cout << "Mode " << mode << '\n';
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                std::cout << static_cast<int>(pixels[row * n + col].data) << ' ';
            }
            std::cout << '\n';
        }
    }
}
