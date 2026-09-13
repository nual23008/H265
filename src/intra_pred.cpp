// src/intra_pred.cpp
#include "intra_pred.h"

#include <algorithm>   // std::fill

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
