// tools/encoder.cpp
// Encoder intra đơn giản (hướng B): CTU = TU = 16x16. Hiện chỉ mã hoá luma Y, dự đoán DC tạm thời.
//
// Cách dùng (đứng ở thư mục H265):
//   ./out/encoder.exe [QP] [frameIndex]      mặc định QP = 32, frameIndex = 0
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "picture.h"     // Plane, Picture, readFrame, padPicture, initPicture, getBlock, writeBlock
#include "block_ops.h"   // computeResidual, addBlocks
#include "intra_pred.h"  // RefSamples, getRefSamples, predictDC
#include "dct.h"         // forwardDct, inverseDct
#include "quant.h"       // quantize, dequantize, qStep
#include "metrics.h"     // computeMse, computePsnr

// Mã hoá một CTU luma tại (topRow, leftCol) và ghi kết quả tái tạo vào recon.
// Trả về số level khác 0 (đại diện thô cho số bit).
int encodeLumaCtu(const Plane& original, Plane& recon, int topRow, int leftCol, int N, int qp) {
    std::vector<int32_t> orgBlock = getBlock(original, topRow, leftCol, N);

    // Intra prediction: mẫu tham chiếu lấy từ RECON (thứ decoder có), không phải ảnh gốc
    RefSamples ref            = getRefSamples(recon, topRow, leftCol, N);
    std::vector<int32_t> pred = predictDC(ref, true);   // TODO: thay bằng chooseIntraMode + predictIntra của teammate

    // Nút trừ -> Transform -> Quantization
    std::vector<int32_t> residual = computeResidual(orgBlock, pred);
    std::vector<int32_t> level    = quantize(forwardDct(residual, N), qp, N);

    // Dequantization -> Inverse transform -> nút cộng -> ghi recon (có clip)
    std::vector<int32_t> residual2  = inverseDct(dequantize(level, qp, N), N);
    std::vector<int32_t> reconBlock = addBlocks(pred, residual2);
    writeBlock(recon, topRow, leftCol, N, reconBlock);

    int numNonZero = 0;
    for (int32_t value : level) {
        if (value != 0) ++numNonZero;
    }
    return numNonZero;
}

int main(int argc, char* argv[]) {
    const int WIDTH    = 1920;
    const int HEIGHT   = 1080;
    const int CTU_SIZE = 16;
    const int qp         = (argc > 1) ? std::atoi(argv[1]) : 32;
    const int frameIndex = (argc > 2) ? std::atoi(argv[2]) : 0;

    if (qp < 0 || qp > 51) {
        std::cerr << "Error: QP phai nam trong [0, 51]." << std::endl;
        return 1;
    }

    std::ifstream file("Input/Input.yuv", std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open file 'Input/Input.yuv'." << std::endl;
        return 1;
    }
    Picture picture;
    if (!readFrame(file, WIDTH, HEIGHT, frameIndex, picture)) {
        std::cerr << "Error: Could not read frame " << frameIndex << "." << std::endl;
        return 1;
    }

    // Encoder làm việc trên ảnh đã đệm (1920 x 1088); recon cùng kích thước, ban đầu toàn 0
    const Picture padded = padPicture(picture, CTU_SIZE);
    Picture recon;
    initPicture(recon, padded.Y.width, padded.Y.height);

    const int numCtuCols = padded.Y.width  / CTU_SIZE;
    const int numCtuRows = padded.Y.height / CTU_SIZE;
    long long totalNonZero = 0;

    const auto startTime = std::chrono::steady_clock::now();

    // Thứ tự raster: bắt buộc, vì CTU sau lấy tham chiếu từ recon của CTU trước
    for (int ctuRow = 0; ctuRow < numCtuRows; ++ctuRow) {
        for (int ctuCol = 0; ctuCol < numCtuCols; ++ctuCol) {
            totalNonZero += encodeLumaCtu(padded.Y, recon.Y, ctuRow * CTU_SIZE, ctuCol * CTU_SIZE, CTU_SIZE, qp);
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    const long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // So sánh với ảnh GỐC chưa đệm, chỉ trên vùng ảnh thật 1920 x 1080
    const double mseY  = computeMse(picture.Y, recon.Y, WIDTH, HEIGHT);
    const double psnrY = computePsnr(picture.Y, recon.Y, WIDTH, HEIGHT);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Frame " << frameIndex << ", QP " << qp << " (Qstep = " << qStep(qp) << ")" << std::endl;
    std::cout << "CTU: " << numCtuCols << " x " << numCtuRows << " = " << numCtuCols * numCtuRows << std::endl;
    std::cout << "Level khac 0 (Y): " << totalNonZero << std::endl;
    std::cout << "MSE_Y  = " << mseY << std::endl;
    std::cout << "PSNR_Y = " << psnrY << " dB" << std::endl;
    std::cout << "Thoi gian ma hoa: " << elapsedMs << " ms" << std::endl;
    return 0;
}
