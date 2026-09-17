#include "lib.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

int main(int argc, char* argv[]) {
    std::string inputName = "Input.yuv";
    std::string outputName = "Reconstructed.yuv";
    int width = 1920;
    int height = 1080;
    int quantStep = 16;

    if (argc == 6) {
        inputName = argv[1];
        outputName = argv[2];
        width = std::atoi(argv[3]);
        height = std::atoi(argv[4]);
        quantStep = std::atoi(argv[5]);
    } else if (argc != 1) {
        std::cout << "Cach dung:\n";
        std::cout << "  simple_codec_app input.yuv output.yuv width height quantStep\n";
        return 1;
    }

    if (width <= 0 || height <= 0 || width % 8 != 0 || height % 8 != 0) {
        std::cout << "Width va height phai chia het cho 8\n";
        return 1;
    }
    if (quantStep <= 0) {
        std::cout << "quantStep phai lon hon 0\n";
        return 1;
    }

    std::ifstream input(inputName, std::ios::binary);
    std::ofstream output(outputName, std::ios::binary);

    if (!input) {
        std::cout << "Khong mo duoc file " << inputName << "\n";
        return 1;
    }
    if (!output) {
        std::cout << "Khong tao duoc file " << outputName << "\n";
        return 1;
    }

    Picture originalPicture;
    int frameNumber = 0;
    double totalMSE = 0.0;

    std::cout << std::fixed << std::setprecision(4);

    while (ReadYUV420Frame(input, width, height, originalPicture)) {
        Picture reconstructedPicture = originalPicture;
        reconstructedPicture.Y = ReconstructPlane(originalPicture.Y, quantStep, 8);
        reconstructedPicture.U = ReconstructPlane(originalPicture.U, quantStep, 4);
        reconstructedPicture.V = ReconstructPlane(originalPicture.V, quantStep, 4);

        if (!WriteYUV420Frame(output, reconstructedPicture)) {
            std::cout << "Loi khi ghi output\n";
            return 1;
        }

        double mse = MSE(originalPicture, reconstructedPicture);
        double psnr = (mse == 0.0)
                          ? std::numeric_limits<double>::infinity()
                          : 10.0 * std::log10(255.0 * 255.0 / mse);

        totalMSE = totalMSE + mse;
        frameNumber++;

        std::cout << "Frame " << frameNumber
                  << ": PSNR = " << psnr << " dB\n";

        
        
    }

    if (frameNumber == 0) {
        std::cout << "File khong chua frame YUV420 hop le\n";
        return 1;
    }

    double averageMSE = totalMSE / frameNumber;
    double averagePSNR = (averageMSE == 0.0)
                             ? std::numeric_limits<double>::infinity()
                             : 10.0 * std::log10(255.0 * 255.0 / averageMSE);

    std::cout << "PSNR trung binh = "
              << averagePSNR << " dB\n";
    std::cout << "Da ghi " << frameNumber
              << " frame vao " << outputName << "\n";
    return 0;
}
