// include/picture.h
// Quản lý ảnh: Plane (một thành phần màu), Picture (Y, U, V),
// đọc frame từ file YUV, đệm ảnh, lấy block N x N.
#pragma once

#include <cstdint>
#include <fstream>
#include <vector>

// Quy ước: row = hàng (tương ứng y trong spec), col = cột (tương ứng x trong spec)
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

// Gán kích thước và cấp phát bộ nhớ cho plane
void initPlane(Plane& plane, int width, int height);

// Đọc frame thứ frameIndex (đếm từ 0) của file YUV 4:2:0 8-bit. Trả về false nếu đọc lỗi.
bool readFrame(std::ifstream& file, int width, int height, int frameIndex, Picture& picture);

// Đệm plane cho chiều rộng/cao chia hết cho blockSize (lặp lại hàng/cột ở mép)
Plane padPlane(const Plane& src, int blockSize);

// Đệm cả 3 plane: luma theo ctuSize, chroma theo ctuSize / 2
Picture padPicture(const Picture& src, int ctuSize);

// Lấy block N x N có góc trên-trái tại (topRow, leftCol); toạ độ vượt biên được kẹp về mép
std::vector<int32_t> getBlock(const Plane& plane, int topRow, int leftCol, int N);
