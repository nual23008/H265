#include "prediction.h"

#include <cstdlib>

std::vector<uint8_t> GetTopReference(const Plane& reconstructedFrame, int blockRow, int blockCol, int blockSize) {
    std::vector<uint8_t> top(2 * blockSize, 128);

    // Khong co hang tren: dung mau ben trai neu co.
    if (blockRow == 0) {
        if (blockCol > 0) {
            uint8_t value = reconstructedFrame.data[reconstructedFrame.getIndex(blockRow, blockCol - 1)];
            for (int i = 0; i < 2 * blockSize; i++) {
                top[i] = value;
            }
        }
        return top;
    }

    // Lay top va top-right. Neu vuot bien phai, lap lai mau cuoi.
    for (int i = 0; i < 2 * blockSize; i++) {
        int col = blockCol + i;
        if (col < reconstructedFrame.width) {
            top[i] = static_cast<uint8_t>(reconstructedFrame.getPixel(blockRow - 1, col));
        } else {
            top[i] = top[i - 1];
        }
    }

    return top;
}

std::vector<uint8_t> GetLeftReference(const Plane& reconstructedFrame, int blockRow, int blockCol, int blockSize) {
    std::vector<uint8_t> left(2 * blockSize, 128);

    // Khong co cot trai: dung mau phia tren neu co.
    if (blockCol == 0) {
        if (blockRow > 0) {
            uint8_t value = reconstructedFrame.data[reconstructedFrame.getIndex(blockRow - 1, blockCol)];
            for (int i = 0; i < 2 * blockSize; i++) {
                left[i] = value;
            }
        }
        return left;
    }

    // N mau left dau tien da duoc reconstructed.
    for (int i = 0; i < blockSize; i++) {
        int row = blockRow + i;
        if (row < reconstructedFrame.height) {
            left[i] = static_cast<uint8_t>(reconstructedFrame.getPixel(row, blockCol - 1));
        } else {
            left[i] = left[i - 1];
        }
    }

    // Below-left chua duoc xu ly theo raster scan, nen lap mau left cuoi.
    for (int i = blockSize; i < 2 * blockSize; i++) {
        left[i] = left[blockSize - 1];
    }

    return left;
}

uint8_t GetTopLeftReference(const Plane& reconstructedFrame, int blockRow, int blockCol, const std::vector<uint8_t>& top, const std::vector<uint8_t>& left) {
    if (blockRow > 0 && blockCol > 0) {
        return static_cast<uint8_t>(reconstructedFrame.getPixel(blockRow - 1, blockCol - 1));
    }
    if (blockRow > 0) {
        return top[0];
    }
    if (blockCol > 0) {
        return left[0];
    }
    return 128;
}

int ClipPrediction(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

int FloorDivide(int value, int divisor) {
    if (value >= 0) {
        return value / divisor;
    }
    return -((-value + divisor - 1) / divisor);
}

std::vector<uint8_t> PlanarPrediction(const std::vector<uint8_t>& top, const std::vector<uint8_t>& left, int blockSize) {
    std::vector<uint8_t> prediction(blockSize * blockSize);

    for (int row = 0; row < blockSize; row++) {
        for (int col = 0; col < blockSize; col++) {
            int horizontal   = (blockSize - 1 - col) * left[row] + (col + 1) * top[blockSize];
            int vertical     = (blockSize - 1 - row) * top[col] + (row + 1) * left[blockSize];

            prediction[row * blockSize + col] = static_cast<uint8_t>((horizontal + vertical + blockSize) / (2 * blockSize));
        }
    }

    return prediction;
}

std::vector<uint8_t> DCPrediction(const std::vector<uint8_t>& top, const std::vector<uint8_t>& left, int blockSize) {
    int sum = 0;
    for (int i = 0; i < blockSize; i++) {
        sum = sum + top[i] + left[i];
    }

    int dc = (sum + blockSize) / (2 * blockSize);
    std::vector<uint8_t> prediction(blockSize * blockSize, dc);

    // Boundary filtering cua DC mode cho block nho hon 32.
    if (blockSize < 32) {
        prediction[0] = static_cast<uint8_t>((top[0] + left[0] + 2 * dc + 2) / 4);

        for (int i = 1; i < blockSize; i++) {
            prediction[i] = static_cast<uint8_t>((top[i] + 3 * dc + 2) / 4);
            prediction[i * blockSize] = static_cast<uint8_t>((left[i] + 3 * dc + 2) / 4);
        }
    }

    return prediction;
}

std::vector<uint8_t> AngularPrediction(const std::vector<uint8_t>& top, const std::vector<uint8_t>& left, uint8_t topLeft, int blockSize, int mode) {
    const int angles[9] = {0, 2, 5, 9, 13, 17, 21, 26, 32};
    const int inverseAngles[9] = {0, 4096, 1638, 910, 630, 482, 390, 315, 256};

    bool vertical = mode >= 18;
    int angleIndex = vertical ? mode - 26 : 10 - mode;
    int magnitude = std::abs(angleIndex);
    int angle = angles[magnitude];
    if (angleIndex < 0) {
        angle = -angle;
    }

    std::vector<int> mainStorage(3 * blockSize + 1, 0);
    int* mainReference = mainStorage.data() + blockSize;
    std::vector<int> sideReference(2 * blockSize + 1, 0);

    mainReference[0] = topLeft;
    sideReference[0] = topLeft;

    for (int i = 0; i < 2 * blockSize; i++) {
        mainReference[i + 1] = vertical ? top[i] : left[i];
        sideReference[i + 1] = vertical ? left[i] : top[i];
    }

    // Tao cac reference co chi so am cho cac mode goc am.
    if (angle < 0) {
        int lowest = FloorDivide(blockSize * angle, 32);
        for (int index = -1; index > lowest; index--) {
            int sideIndex =
                (128 + (-index) * inverseAngles[magnitude]) / 256;
            mainReference[index] = sideReference[sideIndex];
        }
    }

    std::vector<uint8_t> prediction(blockSize * blockSize);

    for (int depth = 0; depth < blockSize; depth++) {
        int displacement = (depth + 1) * angle;
        int whole = FloorDivide(displacement, 32);
        int fraction = displacement - whole * 32;

        for (int along = 0; along < blockSize; along++) {
            int index = along + whole + 1;
            int value = mainReference[index];

            if (fraction != 0) {
                value = ((32 - fraction) * mainReference[index] + fraction * mainReference[index + 1] + 16) / 32;
            }

            // Boundary filtering cho horizontal va vertical chinh xac.
            if (angle == 0 && along == 0 && blockSize < 32) {
                value = value + FloorDivide(sideReference[depth + 1] - sideReference[0], 2);
            }

            int row = vertical ? depth : along;
            int col = vertical ? along : depth;
            prediction[row * blockSize + col] = static_cast<uint8_t>(ClipPrediction(value));
        }
    }

    return prediction;
}

std::vector<uint8_t> IntraPrediction(const Plane& reconstructedFrame, int blockRow, int blockCol, int blockSize,int mode) {
    std::vector<uint8_t> top = GetTopReference(
        reconstructedFrame, blockRow, blockCol, blockSize);
    std::vector<uint8_t> left = GetLeftReference(
        reconstructedFrame, blockRow, blockCol, blockSize);

    if (mode == MODE_PLANAR) {
        return PlanarPrediction(top, left, blockSize);
    }
    if (mode == MODE_DC) {
        return DCPrediction(top, left, blockSize);
    }

    uint8_t topLeft = GetTopLeftReference(
        reconstructedFrame, blockRow, blockCol, top, left);
    return AngularPrediction(top, left, topLeft, blockSize, mode);
}

int CalculateSAD(const std::vector<uint8_t>& originalBlock, const std::vector<uint8_t>& predictionBlock, int block_size) {
    int sad = 0;

    for (int i = 0; i < block_size * block_size; i++) {
        sad = sad + std::abs(static_cast<int>(originalBlock[i]) - static_cast<int>(predictionBlock[i]));
    }

    return sad;
}

int EstimateIntraMode(const std::vector<uint8_t>& originalBlock, const Plane& reconstructedPlane, int blockRow, int blockCol, int blockSize) {
    int bestMode = MODE_PLANAR;
    int bestSad = 1000000;

    // HEVC co 35 mode: Planar, DC va 33 mode Angular.
    for (int mode = 0; mode <= 34; mode++) {
        std::vector<uint8_t> prediction = IntraPrediction(reconstructedPlane, blockRow, blockCol, blockSize, mode);
        int sad = CalculateSAD(originalBlock, prediction, blockSize);

        if (sad < bestSad) {
            bestSad = sad;
            bestMode = mode;
        }
    }

    return bestMode;
}
