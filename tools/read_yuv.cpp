// tools/read_yuv.cpp
// Chương trình thử: đọc 1 frame YUV thật rồi in dữ liệu qua từng bước.
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "picture.h"     // Plane, Picture, readFrame, padPicture, getBlock
#include "block_ops.h"   // computeResidual, sumAbsolute, blockVariance
#include "intra_pred.h"  // RefSamples, getRefSamples, computeDcValue, predictDC
#include "debug_print.h" // printStats, printBlock, printRefSamples

// Kết quả bước 4 mà bước 6 cần dùng lại
struct CtuGridInfo {
    int numCtuCols = 0;
    int minVarAddr = 0;   // số thứ tự CTU phẳng nhất
    int maxVarAddr = 0;   // số thứ tự CTU nhiều chi tiết nhất
};

// Bước 1-2: in vài pixel và thống kê 3 plane
void demoPixelsAndStats(const Picture& picture) {
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
}

// Bước 3: tách block N x N ra khỏi plane
void demoGetBlock(const Picture& picture) {
    printBlock("Y 8x8 tai (row 0, col 0) - goc tren-trai:",       getBlock(picture.Y, 0, 0, 8), 8);
    printBlock("Y 8x8 tai (row 500, col 1000) - giua anh:",       getBlock(picture.Y, 500, 1000, 8), 8);
    printBlock("Y 8x8 tai (row 736, col 1216) - nhieu chi tiet:", getBlock(picture.Y, 736, 1216, 8), 8);
    printBlock("Y 8x8 tai (row 1076, col 0) - cham mep duoi:",    getBlock(picture.Y, 1076, 0, 8), 8);
    printBlock("U 4x4 tai (row 0, col 0):",                       getBlock(picture.U, 0, 0, 4), 4);
}

// P1: kiểm tra ảnh đã đệm (padded) so với ảnh gốc (picture)
void demoPadding(const Picture& picture, const Picture& padded) {
    const int width  = picture.Y.width;
    const int height = picture.Y.height;

    std::cout << "\n=== P1: anh da dem ===" << std::endl;
    std::cout << "Y: " << picture.Y.width << "x" << picture.Y.height << " -> " << padded.Y.width << "x" << padded.Y.height
              << ",  U/V: " << picture.U.width << "x" << picture.U.height << " -> " << padded.U.width << "x" << padded.U.height << std::endl;

    // Kiểm tra 1: vùng ảnh thật (1080 hàng đầu) phải giữ nguyên
    int numChangedPixels = 0;
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (padded.Y.getPixel(r, c) != picture.Y.getPixel(r, c)) ++numChangedPixels;
        }
    }
    // Kiểm tra 2: mỗi hàng đệm (1080..1087) phải giống hệt hàng cuối 1079
    int numWrongPadPixels = 0;
    for (int r = height; r < padded.Y.height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (padded.Y.getPixel(r, c) != picture.Y.getPixel(height - 1, c)) ++numWrongPadPixels;
        }
    }
    std::cout << "Pixel vung anh that bi thay doi: " << numChangedPixels << std::endl;
    std::cout << "Pixel hang dem khac hang 1079   : " << numWrongPadPixels << std::endl;
}

// Bước 4: chia ảnh thành lưới CTU, tìm CTU phẳng nhất và nhiều chi tiết nhất.
// width, height: kích thước ảnh THẬT (chưa đệm), dùng để đếm CTU thò ra ngoài ảnh.
CtuGridInfo demoCtuGrid(const Plane& paddedY, int width, int height, int ctuSize) {
    const int numCtuCols = (width  + ctuSize - 1) / ctuSize;   // chia làm tròn LÊN
    const int numCtuRows = (height + ctuSize - 1) / ctuSize;
    const int numCtus    = numCtuCols * numCtuRows;
    std::cout << "\nCTU " << ctuSize << "x" << ctuSize << ": luoi " << numCtuCols << " cot x "
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
            int topRow  = ctuRow * ctuSize;               // toạ độ pixel góc trên-trái của CTU
            int leftCol = ctuCol * ctuSize;

            if (topRow + ctuSize > height || leftCol + ctuSize > width) {
                ++numPartialCtus;
            }

            std::vector<int32_t> ctu = getBlock(paddedY, topRow, leftCol, ctuSize);
            double var = blockVariance(ctu);
            if (var > maxVar) { maxVar = var; maxVarAddr = ctuAddr; }
            if (var < minVar) { minVar = var; minVarAddr = ctuAddr; }
        }
    }

    std::cout << "So CTU tho ra ngoai anh: " << numPartialCtus << std::endl;
    std::cout << "CTU phang nhat       : addr " << minVarAddr
              << " (row " << (minVarAddr / numCtuCols) * ctuSize
              << ", col " << (minVarAddr % numCtuCols) * ctuSize << "), variance = " << minVar << std::endl;
    std::cout << "CTU nhieu chi tiet nhat: addr " << maxVarAddr
              << " (row " << (maxVarAddr / numCtuCols) * ctuSize
              << ", col " << (maxVarAddr % numCtuCols) * ctuSize << "), variance = " << maxVar << std::endl;
    printBlock("CTU nhieu chi tiet nhat (Y 16x16):",
               getBlock(paddedY, (maxVarAddr / numCtuCols) * ctuSize,
                        (maxVarAddr % numCtuCols) * ctuSize, ctuSize), ctuSize);

    CtuGridInfo grid;
    grid.numCtuCols = numCtuCols;
    grid.minVarAddr = minVarAddr;
    grid.maxVarAddr = maxVarAddr;
    return grid;
}

// Bước 5: lấy mẫu tham chiếu cho block = CTU ở 5 vị trí đặc biệt
// LƯU Ý: tạm dùng ảnh GỐC để kiểm tra việc lấy mẫu. Khi có ảnh tái tạo phải truyền ảnh đó vào.
void demoRefSamples(const Plane& paddedY, int ctuSize) {
    std::cout << "\n=== Buoc 5: mau tham chieu ===" << std::endl;
    printRefSamples("[A] giua anh (row 720, col 1008):",                 getRefSamples(paddedY, 720, 1008, ctuSize));
    printRefSamples("[B] mep tren (row 0, col 1008):",                   getRefSamples(paddedY, 0, 1008, ctuSize));
    printRefSamples("[C] mep trai (row 720, col 0):",                    getRefSamples(paddedY, 720, 0, ctuSize));
    printRefSamples("[D] goc tren-trai (row 0, col 0):",                 getRefSamples(paddedY, 0, 0, ctuSize));
    printRefSamples("[E] goc duoi-phai, vuot bien (row 1072, col 1904):", getRefSamples(paddedY, 1072, 1904, ctuSize));
}

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

// Bước 6: dự đoán DC + residual cho 2 CTU đối lập nhau mà bước 4 đã tìm ra
// LƯU Ý: vẫn lấy mẫu tham chiếu từ ảnh GỐC (chưa có ảnh tái tạo).
void demoDcPredictionOnCtus(const Plane& paddedY, const CtuGridInfo& grid, int ctuSize) {
    std::cout << "\n=== Buoc 6: du doan DC + residual ===" << std::endl;
    demoDcPrediction("[CTU phang nhat]", paddedY,
                     (grid.minVarAddr / grid.numCtuCols) * ctuSize, (grid.minVarAddr % grid.numCtuCols) * ctuSize, ctuSize);
    demoDcPrediction("[CTU nhieu chi tiet nhat]", paddedY,
                     (grid.maxVarAddr / grid.numCtuCols) * ctuSize, (grid.maxVarAddr % grid.numCtuCols) * ctuSize, ctuSize);
}

int main(){
    std::cout << "Hello, YUV!" << std::endl;

    const int WIDTH      = 1920;
    const int HEIGHT     = 1080;
    const int CTU_SIZE   = 16;
    const int frameIndex = 0;   // số thứ tự frame trong file, đếm từ 0

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

    Picture picture;
    if (!readFrame(file, WIDTH, HEIGHT, frameIndex, picture)) {
        std::cerr << "Error: Could not read frame." << std::endl;
        return 1;
    }

    demoPixelsAndStats(picture);                                          // bước 1-2
    demoGetBlock(picture);                                                // bước 3

    // P1: đệm ảnh cho kích thước chia hết cho CTU. Từ đây trở đi encoder làm việc trên ảnh đã đệm.
    const Picture padded = padPicture(picture, CTU_SIZE);
    demoPadding(picture, padded);

    CtuGridInfo grid = demoCtuGrid(padded.Y, WIDTH, HEIGHT, CTU_SIZE);    // bước 4
    demoRefSamples(padded.Y, CTU_SIZE);                                   // bước 5
    demoDcPredictionOnCtus(padded.Y, grid, CTU_SIZE);                     // bước 6

    return 0;
}
