#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

#include "picture.h"     // Plane, Picture, readFrame, padPicture, getBlock
#include "block_ops.h"   // computeResidual, sumAbsolute, blockVariance
#include "intra_pred.h"  // RefSamples, getRefSamples, computeDcValue, predictDC
#include "debug_print.h" // printStats, printBlock, printRefSamples

// Chạy DC prediction cho một CTU và in đầy đủ các chặng
void demoDcPrediction(const char* name, const Plane& plane, int topRow, int leftCol, int N) {
    std::cout << name << " (row " << topRow << ", col " << leftCol << "):" << std::endl;

    RefSamples ref                  = getRefSamples(plane, topRow, leftCol, N);
    std::vector<int32_t> original   = getBlock(plane, topRow, leftCol, N);
    std::vector<int32_t> prediction = predictDC(ref, true);
    std::vector<int32_t> residual   = computeResidual(original, prediction);

    std::cout << "  dcVal = " << computeDcValue(ref) << std::endl;
    printBlock("Original:", original, N);
    printBlock("Prediction (DC):", prediction, N);
    printBlock("Residual = Original - Prediction:", residual, N);
    std::cout << "  SAD(residual) = " << sumAbsolute(residual) << std::endl;
}

int main(){
    std::cout << "Hello, YUV!" << std::endl;

    const int WIDTH = 1920;
    const int HEIGHT = 1080;

    const int ySize = WIDTH * HEIGHT;
    const int uvSize = (WIDTH / 2) * (HEIGHT / 2);
    const int frameSize = ySize + 2 * uvSize;

    std::ifstream file("Input/Input.yuv", std::ios::binary);   // mở file ở chế độ nhị phân
    if (!file) { 
        std::cerr << "Error: Could not open file 'Input/Input.yuv'." << std::endl;
        return 1;
    }                    

    file.seekg(0, std::ios::end);          // đưa "con trỏ đọc" xuống cuối file để xác định kích thước file
    long long fileSize = file.tellg();     // vị trí hiện tại = kích thước file (byte)
    int numFrames = fileSize / frameSize;
    if (fileSize % frameSize != 0) {
        std::cerr << "Error: File size is not a multiple of frame size. Check WIDTH/HEIGHT." << std::endl;
        return 1;
    }

    std::cout << "File size: " << fileSize << " bytes" << std::endl;
    std::cout << "Frame size: " << frameSize << " bytes" << std::endl;
    std::cout << "Number of frames: " << numFrames << std::endl;

    const int frameIndex = 0;   // số thứ tự frame trong file, đếm từ 0
    Picture picture;
    if (!readFrame(file, WIDTH, HEIGHT, frameIndex, picture)) {
        std::cerr << "Error: Could not read frame." << std::endl;
        return 1;
    }

    std::cout << "Y(0, 0) = " << picture.Y.getPixel(0, 0) << std::endl;
    std::cout << "U(0, 0) = " << picture.U.getPixel(0, 0) << std::endl;
    std::cout << "V(0, 0) = " << picture.V.getPixel(0, 0) << std::endl;

    // In gia tri pixel tai hang 500, cot 1000 cua plane Y
    int row = 500;
    int col = 1000;
    std::cout << "Pixel value at Y(row " << row << ", col " << col << ") = " << picture.Y.getPixel(row, col) << std::endl;

    // In thong tin chi tiet cua cac plane
    printStats("Y", picture.Y);
    printStats("U", picture.U);
    printStats("V", picture.V);

    // tách block N x N ra khỏi plane
    printBlock("Y 8x8 tai (row 0, col 0) - goc tren-trai:",       getBlock(picture.Y, 0, 0, 8), 8);
    printBlock("Y 8x8 tai (row 500, col 1000) - giua anh:",       getBlock(picture.Y, 500, 1000, 8), 8);
    printBlock("Y 8x8 tai (row 736, col 1216) - nhieu chi tiet:", getBlock(picture.Y, 736, 1216, 8), 8);
    printBlock("Y 8x8 tai (row 1076, col 0) - cham mep duoi:",    getBlock(picture.Y, 1076, 0, 8), 8);
    printBlock("U 4x4 tai (row 0, col 0):",                       getBlock(picture.U, 0, 0, 4), 4);

    //chia ảnh thành lưới CTU 
    const int CTU_SIZE = 16;

    // P1: đệm ảnh cho kích thước chia hết cho CTU. Từ đây trở đi encoder làm việc trên ảnh đã đệm.
    const Picture padded = padPicture(picture, CTU_SIZE);
    std::cout << "\n=== P1: anh da dem ===" << std::endl;
    std::cout << "Y: " << picture.Y.width << "x" << picture.Y.height << " -> " << padded.Y.width << "x" << padded.Y.height
              << ",  U/V: " << picture.U.width << "x" << picture.U.height << " -> " << padded.U.width << "x" << padded.U.height << std::endl;

    // Kiểm tra 1: vùng ảnh thật (1080 hàng đầu) phải giữ nguyên
    int numChangedPixels = 0;
    for (int r = 0; r < HEIGHT; ++r) {
        for (int c = 0; c < WIDTH; ++c) {
            if (padded.Y.getPixel(r, c) != picture.Y.getPixel(r, c)) ++numChangedPixels;
        }
    }
    // Kiểm tra 2: mỗi hàng đệm (1080..1087) phải giống hệt hàng cuối 1079
    int numWrongPadPixels = 0;
    for (int r = HEIGHT; r < padded.Y.height; ++r) {
        for (int c = 0; c < WIDTH; ++c) {
            if (padded.Y.getPixel(r, c) != picture.Y.getPixel(HEIGHT - 1, c)) ++numWrongPadPixels;
        }
    }
    std::cout << "Pixel vung anh that bi thay doi: " << numChangedPixels << std::endl;
    std::cout << "Pixel hang dem khac hang 1079   : " << numWrongPadPixels << std::endl;
    const int numCtuCols = (WIDTH  + CTU_SIZE - 1) / CTU_SIZE;   // chia làm tròn LÊN
    const int numCtuRows = (HEIGHT + CTU_SIZE - 1) / CTU_SIZE;
    const int numCtus    = numCtuCols * numCtuRows;
    std::cout << "\nCTU " << CTU_SIZE << "x" << CTU_SIZE << ": luoi " << numCtuCols << " cot x "
              << numCtuRows << " hang = " << numCtus << " CTU" << std::endl;

    int numPartialCtus = 0;        // số CTU thò ra ngoài ảnh
    double maxVar = -1.0;          // CTU nhiều chi tiết nhất
    int maxVarAddr = 0;
    double minVar = 1e18;          // CTU phẳng nhất
    int minVarAddr = 0;

    // Duyệt theo thứ tự raster: hết một hàng CTU (trái -> phải) mới xuống hàng dưới
    for (int ctuRow = 0; ctuRow < numCtuRows; ++ctuRow) {
        for (int ctuCol = 0; ctuCol < numCtuCols; ++ctuCol) {
            int ctuAddr = ctuRow * numCtuCols + ctuCol;   // số thứ tự CTU (spec: CtbAddrInRs)
            int topRow  = ctuRow * CTU_SIZE;              // toạ độ pixel góc trên-trái của CTU
            int leftCol = ctuCol * CTU_SIZE;

            if (topRow + CTU_SIZE > HEIGHT || leftCol + CTU_SIZE > WIDTH) {
                ++numPartialCtus;
            }

            std::vector<int32_t> ctu = getBlock(padded.Y, topRow, leftCol, CTU_SIZE);
            double var = blockVariance(ctu);
            if (var > maxVar) { maxVar = var; maxVarAddr = ctuAddr; }
            if (var < minVar) { minVar = var; minVarAddr = ctuAddr; }
        }
    }

    std::cout << "So CTU tho ra ngoai anh: " << numPartialCtus << std::endl;
    std::cout << "CTU phang nhat       : addr " << minVarAddr
              << " (row " << (minVarAddr / numCtuCols) * CTU_SIZE
              << ", col " << (minVarAddr % numCtuCols) * CTU_SIZE << "), variance = " << minVar << std::endl;
    std::cout << "CTU nhieu chi tiet nhat: addr " << maxVarAddr
              << " (row " << (maxVarAddr / numCtuCols) * CTU_SIZE
              << ", col " << (maxVarAddr % numCtuCols) * CTU_SIZE << "), variance = " << maxVar << std::endl;
    printBlock("CTU nhieu chi tiet nhat (Y 16x16):",
               getBlock(padded.Y, (maxVarAddr / numCtuCols) * CTU_SIZE,
                        (maxVarAddr % numCtuCols) * CTU_SIZE, CTU_SIZE), CTU_SIZE);

    // Bước 5: lấy mẫu tham chiếu cho block = CTU 16x16
    // LƯU Ý: tạm dùng ảnh GỐC để kiểm tra việc lấy mẫu. Khi có ảnh tái tạo phải truyền ảnh đó vào.
    std::cout << "\n=== Buoc 5: mau tham chieu ===" << std::endl;
    printRefSamples("[A] giua anh (row 720, col 1008):",                 getRefSamples(padded.Y, 720, 1008, CTU_SIZE));
    printRefSamples("[B] mep tren (row 0, col 1008):",                   getRefSamples(padded.Y, 0, 1008, CTU_SIZE));
    printRefSamples("[C] mep trai (row 720, col 0):",                    getRefSamples(padded.Y, 720, 0, CTU_SIZE));
    printRefSamples("[D] goc tren-trai (row 0, col 0):",                 getRefSamples(padded.Y, 0, 0, CTU_SIZE));
    printRefSamples("[E] goc duoi-phai, vuot bien (row 1072, col 1904):", getRefSamples(padded.Y, 1072, 1904, CTU_SIZE));

    // Bước 6: dự đoán DC + residual cho 2 CTU đối lập nhau
    // LƯU Ý: vẫn lấy mẫu tham chiếu từ ảnh GỐC (chưa có ảnh tái tạo).
    std::cout << "\n=== Buoc 6: du doan DC + residual ===" << std::endl;
    demoDcPrediction("[CTU phang nhat]", padded.Y,
                     (minVarAddr / numCtuCols) * CTU_SIZE, (minVarAddr % numCtuCols) * CTU_SIZE, CTU_SIZE);
    demoDcPrediction("[CTU nhieu chi tiet nhat]", padded.Y,
                     (maxVarAddr / numCtuCols) * CTU_SIZE, (maxVarAddr % numCtuCols) * CTU_SIZE, CTU_SIZE);

    return 0;
}