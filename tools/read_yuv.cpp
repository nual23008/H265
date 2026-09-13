#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

#include "picture.h"     // Plane, Picture, readFrame, padPicture, getBlock
#include "block_ops.h"   // computeResidual, sumAbsolute, blockVariance

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

void printBlock(const char* title, const std::vector<int32_t>& block, int N) {
    std::cout << title << std::endl;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            std::cout << std::setw(5) << block[r * N + c];
        }
        std::cout << std::endl;
    }
}

// Mẫu tham chiếu (reference samples) cho intra prediction của một block N x N (spec 8.4.4.2).
// Spec ký hiệu p[x][y]: x = cột, y = hàng, gốc (0,0) là pixel trên-trái của block.
struct RefSamples {
    int N = 0;
    int32_t corner = 0;           // p[-1][-1]
    std::vector<int32_t> top;     // top[i]  = p[i][-1], i = 0..2N-1  (0..N-1: trên,  N..2N-1: trên-phải)
    std::vector<int32_t> left;    // left[i] = p[-1][i], i = 0..2N-1  (0..N-1: trái,  N..2N-1: dưới-trái)
};

// Pixel (row, col) có dùng được làm tham chiếu cho block đang mã hoá tại (curTopRow, curLeftCol) không?
// Điều kiện: nằm trong ảnh VÀ thuộc CTU đã được mã hoá trước (thứ tự raster).
// Giả định tạm thời: mỗi CTU là một block, chưa chia nhỏ thành CU.
bool isAvailable(const Plane& plane, int ctuSize, int curTopRow, int curLeftCol, int row, int col) {
    if (row < 0 || row >= plane.height || col < 0 || col >= plane.width) {
        return false;
    }
    int numCtuCols = (plane.width + ctuSize - 1) / ctuSize;
    int curCtuAddr = (curTopRow / ctuSize) * numCtuCols + curLeftCol / ctuSize;
    int ctuAddr    = (row / ctuSize) * numCtuCols + col / ctuSize;
    return ctuAddr < curCtuAddr;
}

// Lấy 4N+1 mẫu tham chiếu cho block N x N có góc trên-trái tại (topRow, leftCol).
// LƯU Ý: plane truyền vào phải là ảnh ĐÃ TÁI TẠO (thứ decoder có), không phải ảnh gốc.
RefSamples getRefSamples(const Plane& plane, int topRow, int leftCol, int N) {
    const int numSamples = 4 * N + 1;

    // Bước 1: xếp 4N+1 mẫu thành MỘT hàng theo đúng thứ tự quét của spec:
    //   i = 0 .. 2N-1   : cột trái, đi TỪ DƯỚI LÊN  (i = 0 là p[-1][2N-1])
    //   i = 2N          : góc p[-1][-1]
    //   i = 2N+1 .. 4N  : hàng trên, đi TỪ TRÁI SANG PHẢI (i = 2N+1 là p[0][-1])
    std::vector<int32_t> value(numSamples, 0);
    std::vector<bool> available(numSamples, false);
    int numAvailable = 0;

    for (int i = 0; i < numSamples; ++i) {
        int row;
        int col;
        if (i < 2 * N) {
            row = topRow + (2 * N - 1 - i);
            col = leftCol - 1;
        } else if (i == 2 * N) {
            row = topRow - 1;
            col = leftCol - 1;
        } else {
            row = topRow - 1;
            col = leftCol + (i - 2 * N - 1);
        }
        if (isAvailable(plane, N, topRow, leftCol, row, col)) {
            available[i] = true;
            value[i] = plane.getPixel(row, col);
            ++numAvailable;
        }
    }

    // Bước 2: thay thế mẫu không có sẵn (spec 8.4.4.2.2)
    if (numAvailable == 0) {
        // Không có mẫu nào: dùng giá trị giữa thang, 1 << (bitDepth - 1) = 128 với ảnh 8-bit
        std::fill(value.begin(), value.end(), 1 << (8 - 1));
    } else {
        // Mẫu đầu tiên thiếu: lấy mẫu có sẵn ĐẦU TIÊN tìm thấy trên đường quét
        if (!available[0]) {
            int first = 1;
            while (!available[first]) {
                ++first;
            }
            value[0] = value[first];
        }
        // Các mẫu thiếu còn lại: lấy giá trị của mẫu NGAY TRƯỚC nó trên đường quét
        for (int i = 1; i < numSamples; ++i) {
            if (!available[i]) {
                value[i] = value[i - 1];
            }
        }
    }

    // Bước 3: tách hàng quét về 3 phần corner / top / left cho dễ dùng
    RefSamples ref;
    ref.N = N;
    ref.corner = value[2 * N];
    ref.top.resize(2 * N);
    ref.left.resize(2 * N);
    for (int i = 0; i < 2 * N; ++i) {
        ref.left[i] = value[2 * N - 1 - i];   // đảo lại: left[0] là mẫu sát góc
        ref.top[i]  = value[2 * N + 1 + i];
    }
    return ref;
}

void printRange(const char* label, const std::vector<int32_t>& values, int begin, int end) {
    std::cout << "  " << label << ":";
    for (int i = begin; i < end; ++i) {
        std::cout << std::setw(4) << values[i];
    }
    std::cout << std::endl;
}

void printRefSamples(const char* title, const RefSamples& ref) {
    int N = ref.N;
    std::cout << title << std::endl;
    std::cout << "  corner      :" << std::setw(4) << ref.corner << std::endl;
    printRange("top         ", ref.top,  0, N);
    printRange("top-right   ", ref.top,  N, 2 * N);
    printRange("left        ", ref.left, 0, N);
    printRange("bottom-left ", ref.left, N, 2 * N);
}

// Giá trị DC = trung bình (làm tròn) của N mẫu trên và N mẫu trái (spec 8.4.4.2.5).
// Chỉ dùng top[0..N-1] và left[0..N-1]; KHÔNG dùng góc, trên-phải, dưới-trái.
int computeDcValue(const RefSamples& ref) {
    const int N = ref.N;
    int log2N = 0;
    while ((1 << log2N) < N) {       // log2N = log2(N): 16 -> 4
        ++log2N;
    }
    int sum = 0;
    for (int i = 0; i < N; ++i) {
        sum += ref.top[i] + ref.left[i];
    }
    // (sum + N) >> (log2N + 1)  ==  làm tròn của sum / (2N)
    return (sum + N) >> (log2N + 1);
}

// Dự đoán DC cho block N x N (spec 8.4.4.2.5).
// Luma với N < 32: hàng đầu và cột đầu được lọc để nối mượt với mẫu tham chiếu (DC edge filter).
std::vector<int32_t> predictDC(const RefSamples& ref, bool isLuma) {
    const int N = ref.N;
    const int dcVal = computeDcValue(ref);
    std::vector<int32_t> pred(N * N, dcVal);     // mặc định: mọi pixel = dcVal

    if (isLuma && N < 32) {
        // Pixel góc trên-trái: pha trộn với mẫu trái left[0] và mẫu trên top[0]
        pred[0] = (ref.left[0] + 2 * dcVal + ref.top[0] + 2) >> 2;
        // Hàng đầu (row 0): pha 1/4 mẫu trên + 3/4 dcVal
        for (int c = 1; c < N; ++c) {
            pred[0 * N + c] = (ref.top[c] + 3 * dcVal + 2) >> 2;
        }
        // Cột đầu (col 0): pha 1/4 mẫu trái + 3/4 dcVal
        for (int r = 1; r < N; ++r) {
            pred[r * N + 0] = (ref.left[r] + 3 * dcVal + 2) >> 2;
        }
    }
    return pred;
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