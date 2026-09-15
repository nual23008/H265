// src/picture.cpp
#include "picture.h"

#include <algorithm>   // std::min, std::clamp

void initPlane(Plane& plane, int width, int height) {
    plane.width = width;
    plane.height = height;
    plane.data.resize(width * height);
}

void initPicture(Picture& picture, int width, int height) {
    initPlane(picture.Y, width, height);
    initPlane(picture.U, width / 2, height / 2);
    initPlane(picture.V, width / 2, height / 2);
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

// Đệm plane cho chiều rộng/cao chia hết cho blockSize, bằng cách lặp lại cột/hàng ở mép.
// Ảnh đã đệm là ảnh encoder thực sự mã hoá. Decoder bỏ phần đệm nhờ conformance window trong SPS.
Plane padPlane(const Plane& src, int blockSize) {
    int paddedWidth  = (src.width  + blockSize - 1) / blockSize * blockSize;
    int paddedHeight = (src.height + blockSize - 1) / blockSize * blockSize;

    Plane dst;
    initPlane(dst, paddedWidth, paddedHeight);
    for (int row = 0; row < paddedHeight; ++row) {
        int srcRow = std::min(row, src.height - 1);      // hàng đệm -> lấy hàng cuối của ảnh gốc
        for (int col = 0; col < paddedWidth; ++col) {
            int srcCol = std::min(col, src.width - 1);   // cột đệm -> lấy cột cuối của ảnh gốc
            dst.data[dst.getIndex(row, col)] = src.data[src.getIndex(srcRow, srcCol)];
        }
    }
    return dst;
}

// Đệm cả 3 plane. Chroma (4:2:0) có CTB bằng một nửa CTU luma.
Picture padPicture(const Picture& src, int ctuSize) {
    Picture dst;
    dst.Y = padPlane(src.Y, ctuSize);
    dst.U = padPlane(src.U, ctuSize / 2);
    dst.V = padPlane(src.V, ctuSize / 2);
    return dst;
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

void writeBlock(Plane& plane, int topRow, int leftCol, int N, const std::vector<int32_t>& block) {
    for (int r = 0; r < N; ++r) {
        int planeRow = topRow + r;
        if (planeRow < 0 || planeRow >= plane.height) continue;     // ngoài plane -> bỏ qua
        for (int c = 0; c < N; ++c) {
            int planeCol = leftCol + c;
            if (planeCol < 0 || planeCol >= plane.width) continue;
            // clip: prediction + residual có thể < 0 hoặc > 255, uint8_t không chứa được
            int value = std::clamp(block[r * N + c], 0, 255);
            plane.data[plane.getIndex(planeRow, planeCol)] = static_cast<uint8_t>(value);
        }
    }
}
