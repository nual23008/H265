#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <algorithm>

struct Plane {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data; // Danh sách các pixel trong plane
    int getIndex(int row, int col) const {
        return row * width + col;
    }
    int getPixel(int row, int col) const {
        return data[getIndex(row, col)];
    }
};

struct Picture {
    Plane Y;
    Plane U;
    Plane V;
};
void initPlane(Plane& plane, int width, int height) {
    plane.width = width;
    plane.height = height;
    plane.data.resize(width * height);
}

bool readFrame(std::ifstream&file, int width, int height, int frameIndex, Picture& picture) {
    int ySize = width * height;
    int uvSize = (width / 2) * (height / 2);
    int frameSize = ySize + 2 * uvSize;

    long long offset = (long long)frameIndex * frameSize;
    file.seekg(offset, std::ios::beg);

    initPlane(picture.Y, width, height);
    file.read(reinterpret_cast<char*>(picture.Y.data.data()), ySize);

    initPlane(picture.U, width / 2, height / 2);
    file.read(reinterpret_cast<char*>(picture.U.data.data()), uvSize);

    initPlane(picture.V, width / 2, height / 2);
    file.read(reinterpret_cast<char*>(picture.V.data.data()), uvSize);

    return file.good();
}

void printStats(const char* name, const Plane& plane) {
    uint8_t minVal = 255;
    uint8_t maxVal = 0;
    long long sum = 0;

    for (size_t i = 0; i < plane.data.size(); ++i) {
        if (plane.data[i] < minVal) minVal = plane.data[i];
        if (plane.data[i] > maxVal) maxVal = plane.data[i];
        sum += plane.data[i];
    }

    double mean = static_cast<double>(sum) / plane.data.size();
    std::cout << name << " plane: min = " << (int)minVal << ", max = " << (int)maxVal << ", mean = " << mean << std::endl;
}

std::vector<int32_t> getBlock(const Plane& plane, int topRow, int leftCol, int N) { 
    std::vector<int32_t> block(N * N);
    for (int r = 0; r < N; ++r) {
        // clamp: hàng vượt ra ngoài ảnh thì lấy hàng ở mép gần nhất (logic xử lý block vượt ra ngoài ảnh, ví dụ 1080 không chia hết cho 16)
        // xử lý tạm thời bằng cách Kẹp toạ độ: Toạ độ vượt biên thì lấy pixel ở biên gần nhất (giống padding trong Python anh hay làm)
        // Anh đọc thì thằng H265 nó xử lý logic kiểu khác, khó hơn (CU ở biên bị bắt buộc chia nhỏ, kèm conformance window trong SPS)
        int planeRow = std::clamp(topRow + r, 0, plane.height - 1);
        for (int c = 0; c < N; ++c) {
            int planeCol = std::clamp(leftCol + c, 0, plane.width - 1);
            block[r * N + c] = plane.getPixel(planeRow, planeCol);
        }
    }
    return block;
}

void printBlock(const char* title, const std::vector<int32_t>& block, int N) {
    std::cout << title << std::endl;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            std::cout << std::setw(4) << block[r * N + c];
        }
        std::cout << std::endl;
    }
}

// Phương sai (variance) của block: đo mức độ "chi tiết".
// Block phẳng -> variance gần 0; block có cạnh/texture -> variance lớn.
double blockVariance(const std::vector<int32_t>& block) {
    double sum = 0.0;
    double sumSquare = 0.0;
    for (int32_t value : block) {
        sum += value;
        sumSquare += static_cast<double>(value) * value;
    }
    double mean = sum / block.size();
    return sumSquare / block.size() - mean * mean;   // E[x^2] - (E[x])^2
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

    // Bước 3: tách block N x N ra khỏi plane
    printBlock("Y 8x8 tai (row 0, col 0) - goc tren-trai:",       getBlock(picture.Y, 0, 0, 8), 8);
    printBlock("Y 8x8 tai (row 500, col 1000) - giua anh:",       getBlock(picture.Y, 500, 1000, 8), 8);
    printBlock("Y 8x8 tai (row 736, col 1216) - nhieu chi tiet:", getBlock(picture.Y, 736, 1216, 8), 8);
    printBlock("Y 8x8 tai (row 1076, col 0) - cham mep duoi:",    getBlock(picture.Y, 1076, 0, 8), 8);
    printBlock("U 4x4 tai (row 0, col 0):",                       getBlock(picture.U, 0, 0, 4), 4);

    // Bước 4: chia ảnh thành lưới CTU ("Split into CTUs" trong Fig. 1)
    const int CTU_SIZE = 16;
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

            std::vector<int32_t> ctu = getBlock(picture.Y, topRow, leftCol, CTU_SIZE);
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
               getBlock(picture.Y, (maxVarAddr / numCtuCols) * CTU_SIZE,
                        (maxVarAddr % numCtuCols) * CTU_SIZE, CTU_SIZE), CTU_SIZE);

    return 0;
}