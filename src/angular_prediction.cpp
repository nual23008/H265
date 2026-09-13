#include "prediction.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace {
// Chia lam tron xuong, ke ca khi value am (C++ / mac dinh lam tron ve 0).
int FloorDivide(int value, int divisor) {
    const int quotient = value / divisor;
    return quotient - (value % divisor < 0 ? 1 : 0);
}
}

void AngularPredictionBlock(Block* prediction_block,
                            const Pixel* top, const Pixel* left,
                            Pixel top_left, int block_size, int mode,
                            bool filter_luma_boundary) {
    if (!prediction_block || !prediction_block->data || !top || !left) {
        throw std::invalid_argument("Angular prediction: null buffer");
    }
    if (block_size != 4 && block_size != 8 &&
        block_size != 16 && block_size != 32) {
        throw std::invalid_argument("Angular prediction: size must be 4, 8, 16 or 32");
    }
    if (mode < 2 || mode > 34) {
        throw std::invalid_argument("Angular prediction: mode must be 2..34");
    }

    // Don vi la 1/32 pixel dich chuyen tren moi hang/cot, KHONG phai do.
    constexpr int angles[] = {0, 2, 5, 9, 13, 17, 21, 26, 32};
    constexpr int inverse_angles[] = {0, 4096, 1638, 910, 630, 482, 390, 315, 256};
    const bool vertical = mode >= 18;
    const int direction = vertical ? mode - 26 : 10 - mode;
    const int angle_index = std::abs(direction);
    const int angle = (direction < 0 ? -1 : 1) * angles[angle_index];

    // ref[0] = goc; ref[1..2N] = canh chinh.
    // Dat con tro o giua bo dem de dung duoc chi so am khi huong nghieng am.
    std::array<int, 3 * 32 + 1> reference_storage{};
    int* reference = reference_storage.data() + block_size;
    std::array<int, 2 * 32 + 1> side{};
    reference[0] = side[0] = top_left.data;
    for (int i = 0; i < 2 * block_size; ++i) {
        reference[i + 1] = vertical ? top[i].data : left[i].data;
        side[i + 1] = vertical ? left[i].data : top[i].data;
    }

    // Huong am co the di qua goc: mo rong canh chinh bang mau tu canh kia.
    if (angle < 0) {
        int inverse_sum = 128;
        const int lower_bound = FloorDivide(block_size * angle, 32);
        for (int i = -1; i > lower_bound; --i) {
            inverse_sum += inverse_angles[angle_index];
            reference[i] = side[inverse_sum / 256];
        }
    }

    for (int row = 0; row < block_size; ++row) {
        for (int col = 0; col < block_size; ++col) {
            // Cung cong thuc cho hai nhom huong, chi doi vai tro row/col.
            const int distance = vertical ? row + 1 : col + 1;
            const int position = vertical ? col : row;
            const int displacement = distance * angle;
            const int offset = FloorDivide(displacement, 32);
            const int fraction = displacement - offset * 32; // 0..31
            const int index = position + offset + 1;

            int value = reference[index];
            if (fraction != 0) {
                value = ((32 - fraction) * reference[index] +
                         fraction * reference[index + 1] + 16) / 32;
            }

            // Hieu chinh canh cho mode ngang/doc cua luma nho hon 32.
            // Day la buoc khac voi loc mang tham chieu truoc khi predict.
            if (filter_luma_boundary && angle == 0 && block_size < 32 &&
                position == 0) {
                value += FloorDivide(side[distance] - side[0], 2);
                value = std::clamp(value, 0, 255);
            }

            prediction_block->data[row * block_size + col] =
                Pixel{static_cast<uint8_t>(value), true};
        }
    }
}
