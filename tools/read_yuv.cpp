#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>

// Quy ước: row = hàng (tương ứng y trong spec), col = cột (tương ứng x trong spec)
struct Plane {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
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

    // In block 8x8 o goc tren trai (Top Left)
    std::cout << "Block 8x8 at top left corner:" << std::endl;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            std::cout << std::setw(3) << picture.Y.getPixel(row, col) << " ";
        }
        std::cout << std::endl;
    }

    // In gia tri pixel tai hang 500, cot 1000 cua plane Y
    int row = 500;
    int col = 1000;
    std::cout << "Pixel value at Y(row " << row << ", col " << col << ") = " << picture.Y.getPixel(row, col) << std::endl;

    // In thong tin chi tiet cua cac plane
    printStats("Y", picture.Y);
    printStats("U", picture.U);
    printStats("V", picture.V);

    return 0;
}