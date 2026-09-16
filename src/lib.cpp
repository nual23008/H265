#include "lib.h"
#include "prediction.h"
#include "quantization.h"
#include "residual.h"
#include "transform.h"

#include <stdexcept>
#include <cmath>
#include <limits>
#include <stdexcept>



const int BLOCK_SIZE = 8;

std::vector<uint8_t> GetBlock8x8(const Plane& plane, int blockRow, int blockCol, int block_size) {
    std::vector<uint8_t> block(block_size * block_size);

    for (int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int blockIndex = row * block_size + col;
            block[blockIndex] = static_cast<uint8_t>(plane.getPixel(blockRow + row, blockCol + col));
        }
    }

    return block;
}

int ClipPixel(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

std::vector<uint8_t> ReconstructBlock(const std::vector<uint8_t>& predictionBlock, const std::vector<int16_t>& decodedResidual, int block_size) {
    std::vector<uint8_t> reconstructedBlock(block_size * block_size);

    for (int i = 0; i < 64; i++) {
        int value = predictionBlock[i] + decodedResidual[i];
        reconstructedBlock[i] = static_cast<uint8_t>(ClipPixel(value));
    }

    return reconstructedBlock;
}

void WriteBlock8x8(Plane& plane, int blockRow, int blockCol, const std::vector<uint8_t>& block, int block_size) {
    for (int row = 0; row < block_size; row++) {
        for (int col = 0; col < block_size; col++) {
            int blockIndex = row * block_size + col;
            int planeIndex = plane.getIndex(blockRow + row, blockCol + col);
            plane.data[planeIndex] = block[blockIndex];
        }
    }
}

Plane ReconstructPlane(const Plane& originalPlane, int quantStep, int block_size) {
    int width = originalPlane.width;
    int height = originalPlane.height;

    if (width <= 0 || height <= 0 || width % 8 != 0 || height % 8 != 0) {
        throw std::invalid_argument("Chieu rong va chieu cao phai chia het cho 8");
    }
    if (static_cast<int>(originalPlane.data.size()) != width * height) {
        throw std::invalid_argument("Kich thuoc frame khong dung");
    }
    if (quantStep <= 0) {
        throw std::invalid_argument("quantStep phai lon hon 0");
    }

    Plane reconstructedPlane;
    reconstructedPlane.width = width;
    reconstructedPlane.height = height;
    reconstructedPlane.data.assign(width * height, 0);

    for (int blockRow = 0; blockRow < height; blockRow = blockRow + 8) {
        for (int blockCol = 0; blockCol < width; blockCol = blockCol + 8) {
            std::vector<uint8_t> originalBlock = GetBlock8x8(originalPlane, blockRow, blockCol, block_size);

            int mode = EstimateIntraMode(originalBlock, reconstructedPlane, blockRow, blockCol, 8);

            std::vector<uint8_t> predictionBlock = IntraPrediction(reconstructedPlane, blockRow, blockCol, 8, mode);

            std::vector<int16_t> residual = Residual(originalBlock, predictionBlock);

            std::vector<int16_t> coefficients = Transform8x8(residual);

            std::vector<int16_t> levels = Quantize(coefficients, quantStep);

            std::vector<int16_t> decodedCoefficients = Dequantize(levels, quantStep);

            std::vector<int16_t> decodedResidual = InverseTransform8x8(decodedCoefficients);

            std::vector<uint8_t> reconstructedBlock = ReconstructBlock(predictionBlock, decodedResidual, block_size);

            WriteBlock8x8(reconstructedPlane, blockRow, blockCol, reconstructedBlock, block_size);
        }
    }

    return reconstructedPlane;
}

double PSNR(const Plane& originalPlane, const Plane& reconstructedPlane) {
    if (originalPlane.width != reconstructedPlane.width ||
        originalPlane.height != reconstructedPlane.height ||
        originalPlane.data.size() != reconstructedPlane.data.size() ||
        originalPlane.data.empty()) {
        throw std::invalid_argument("Hai frame phai co cung kich thuoc");
    }

    double sumSquaredError = 0.0;

    for (int i = 0; i < static_cast<int>(originalPlane.data.size()); i++) {
        double difference = static_cast<int>(originalPlane.data[i]) - static_cast<int>(reconstructedPlane.data[i]);
        sumSquaredError = sumSquaredError + difference * difference;
    }

    double mse = sumSquaredError / originalPlane.data.size();
    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    return 10.0 * std::log10(255.0 * 255.0 / mse);
}

bool ReadYUV420Frame(std::ifstream& input, int width, int height, Picture& picture) {
    int ySize = width * height;
    int chromaSize = ySize / 4;

    picture.Y.width = width;
    picture.Y.height = height;
    picture.Y.data.resize(ySize);

    picture.U.width = width / 2;
    picture.U.height = height / 2;
    picture.U.data.resize(chromaSize);

    picture.V.width = width / 2;
    picture.V.height = height / 2;
    picture.V.data.resize(chromaSize);

    input.read(reinterpret_cast<char*>(picture.Y.data.data()), ySize);
    if (input.gcount() == 0) {
        return false;
    }
    if (input.gcount() != ySize) {
        return false;
    }

    input.read(reinterpret_cast<char*>(picture.U.data.data()), chromaSize);
    if (input.gcount() != chromaSize) {
        return false;
    }

    input.read(reinterpret_cast<char*>(picture.V.data.data()), chromaSize);
    if (input.gcount() != chromaSize) {
        return false;
    }

    return true;
}

bool WriteYUV420Frame(std::ofstream& output, const Picture& picture) {
    output.write(
        reinterpret_cast<const char*>(picture.Y.data.data()),
        picture.Y.data.size());
    output.write(
        reinterpret_cast<const char*>(picture.U.data.data()),
        picture.U.data.size());
    output.write(
        reinterpret_cast<const char*>(picture.V.data.data()),
        picture.V.data.size());
    return output.good();
}
