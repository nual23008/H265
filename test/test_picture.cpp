// test/test_picture.cpp
// Test module picture: cấp phát, đệm ảnh, lấy/ghi block, đọc/ghi frame YUV.
#include "picture.h"
#include "check.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Plane có pixel(row, col) = row * 10 + col, dễ đoán giá trị khi đọc test
Plane makeRampPlane(int width, int height) {
    Plane plane;
    initPlane(plane, width, height);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            plane.data[plane.getIndex(row, col)] = static_cast<uint8_t>(row * 10 + col);
        }
    }
    return plane;
}

void testInitPicture() {
    Picture picture;
    initPicture(picture, 8, 6);
    CHECK(picture.Y.width == 8 && picture.Y.height == 6 && picture.Y.data.size() == 48);
    CHECK(picture.U.width == 4 && picture.U.height == 3 && picture.U.data.size() == 12);
    CHECK(picture.V.width == 4 && picture.V.height == 3 && picture.V.data.size() == 12);
    bool allZero = true;
    for (uint8_t value : picture.Y.data) {
        if (value != 0) allZero = false;
    }
    CHECK(allZero);
}

void testPadPlane() {
    Plane src = makeRampPlane(5, 3);                         // 5 cột x 3 hàng
    Plane padded = padPlane(src, 4);
    CHECK(padded.width == 8 && padded.height == 4);          // làm tròn lên bội của 4
    CHECK(padded.getPixel(1, 2) == src.getPixel(1, 2));      // vùng ảnh thật giữ nguyên
    CHECK(padded.getPixel(3, 2) == src.getPixel(2, 2));      // hàng đệm = hàng cuối
    CHECK(padded.getPixel(1, 7) == src.getPixel(1, 4));      // cột đệm = cột cuối
    CHECK(padded.getPixel(3, 7) == src.getPixel(2, 4));      // góc đệm = góc ảnh

    Plane unchanged = padPlane(src, 1);                      // đã chia hết -> không đệm
    CHECK(unchanged.width == 5 && unchanged.height == 3 && unchanged.data == src.data);
}

void testPadPicture() {
    Picture picture;
    initPicture(picture, 20, 10);
    Picture padded = padPicture(picture, 16);
    CHECK(padded.Y.width == 32 && padded.Y.height == 16);
    CHECK(padded.U.width == 16 && padded.U.height == 8);      // chroma đệm theo 16 / 2 = 8
    CHECK(padded.V.width == 16 && padded.V.height == 8);
}

void testGetBlock() {
    Plane plane = makeRampPlane(6, 6);
    CHECK(getBlock(plane, 2, 3, 2) == (std::vector<int32_t>{23, 24, 33, 34}));
    // Hàng -1 kẹp về 0, cột 6 kẹp về 5
    CHECK(getBlock(plane, -1, 4, 3) == (std::vector<int32_t>{4, 5, 5, 4, 5, 5, 14, 15, 15}));
}

void testWriteBlock() {
    Plane plane = makeRampPlane(4, 4);
    writeBlock(plane, 1, 1, 2, {-7, 300, 128, 255});
    CHECK(plane.getPixel(1, 1) == 0);                        // -7  -> clip 0
    CHECK(plane.getPixel(1, 2) == 255);                      // 300 -> clip 255
    CHECK(plane.getPixel(2, 1) == 128 && plane.getPixel(2, 2) == 255);
    CHECK(plane.getPixel(0, 0) == 0 && plane.getPixel(3, 3) == 33);   // ngoài block không đổi

    // Block thò ra ngoài mép phải: phần ngoài phải bị bỏ qua, không được ghi tràn sang hàng dưới
    Plane edge = makeRampPlane(4, 4);
    writeBlock(edge, 1, 3, 2, {100, 101, 102, 103});
    CHECK(edge.getPixel(1, 3) == 100 && edge.getPixel(2, 3) == 102);
    CHECK(edge.getPixel(2, 0) == 20 && edge.getPixel(3, 0) == 30);    // (1,4) nếu tràn sẽ đè lên (2,0)
}

void testReadWriteFrame() {
    const int W = 4, H = 2;
    const std::string path = (std::filesystem::temp_directory_path() / "h265_test_picture.yuv").string();
    {
        std::ofstream out(path, std::ios::binary);
        for (int f = 0; f < 2; ++f) {
            Picture picture;
            initPicture(picture, W, 4);   // cao 4 = ảnh đã đệm; writeFrame chỉ ghi 2 hàng đầu
            for (size_t i = 0; i < picture.Y.data.size(); ++i) picture.Y.data[i] = static_cast<uint8_t>(f * 100 + i);
            for (size_t i = 0; i < picture.U.data.size(); ++i) picture.U.data[i] = static_cast<uint8_t>(50 + f * 100 + i);
            for (size_t i = 0; i < picture.V.data.size(); ++i) picture.V.data[i] = static_cast<uint8_t>(70 + f * 100 + i);
            CHECK(writeFrame(out, picture, W, H));
        }
    }
    CHECK(std::filesystem::file_size(path) == 2 * (8 + 2 + 2));   // mỗi frame: Y 4x2 + U 2x1 + V 2x1

    std::ifstream in(path, std::ios::binary);
    Picture back;
    CHECK(readFrame(in, W, H, 1, back));                          // đọc frame thứ 2
    CHECK(back.Y.width == 4 && back.Y.height == 2 && back.U.width == 2 && back.U.height == 1);
    CHECK(back.Y.getPixel(1, 3) == 107);
    CHECK(back.U.getPixel(0, 1) == 151);
    CHECK(back.V.getPixel(0, 0) == 170);
    CHECK(!readFrame(in, W, H, 2, back));                         // frame không tồn tại -> false
    in.close();
    std::filesystem::remove(path);
}

int main() {
    testInitPicture();
    testPadPlane();
    testPadPicture();
    testGetBlock();
    testWriteBlock();
    testReadWriteFrame();
    return testResult("picture");
}
