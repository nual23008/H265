#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>

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
    }                    // mở thất bại thì file mang giá trị false

    file.seekg(0, std::ios::end);          // đưa "con trỏ đọc" xuống cuối file để xác định kích thước file
    long long fileSize = file.tellg();     // vị trí hiện tại = kích thước file (byte)
    int numFrames = fileSize / frameSize;
    if (fileSize % frameSize != 0) {
        std::cerr << "Warning: File size is not a multiple of frame size. Some data may be ignored." << std::endl;
        return 1;
    }

    std::cout << "File size: " << fileSize << " bytes" << std::endl;
    std::cout << "Frame size: " << frameSize << " bytes" << std::endl;
    std::cout << "Number of frames: " << numFrames << std::endl;

    const int f = 6; // Chỉ đọc frame đầu tiên
    std::vector<uint8_t> Y(ySize);
    std::vector<uint8_t> U(uvSize);
    std::vector<uint8_t> V(uvSize);

    long long offset = f * frameSize;
    file.seekg(offset, std::ios::beg); // đưa "con trỏ đọc" đến vị trí bắt đầu của frame thứ f
    file.read(reinterpret_cast<char*>(Y.data()), ySize);
    file.read(reinterpret_cast<char*>(U.data()), uvSize);
    file.read(reinterpret_cast<char*>(V.data()), uvSize);
    if (!file) {
        std::cerr << "Error: Could not read frame data." << std::endl;
        return 1;
    }
    std::cout << "Y[0] = " << (int)Y[0] << std::endl;
    std::cout << "U[0] = " << (int)U[0] << std::endl;
    std::cout << "V[0] = " << (int)V[0] << std::endl;

    // In block 8x8 o goc tren trai (Top Left)
    std::cout << "Block 8x8 at top left corner:" << std::endl;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            std::cout << std::setw(3) << (int)Y[row * WIDTH + col] << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}