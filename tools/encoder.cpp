// tools/encoder.cpp
// Encoder intra đơn giản (hướng B): CTU = TU = 16x16. Hiện chỉ mã hoá luma Y, dự đoán DC tạm thời.
//
// Cách dùng (đứng ở thư mục H265):
//   ./out/encoder.exe [QP] [--frames K] [--out DIR] [--no-quant | --bypass]
//     QP          : 0..51, mặc định 32
//     --frames K  : chỉ mã hoá K frame đầu (mặc định: mọi frame trong file)
//     --out DIR   : thư mục kết quả, mặc định Output. Ghi DIR/recon_qp<QP>.yuv và thêm dòng vào DIR/results.csv
//     --no-quant  : DCT -> IDCT, bỏ lượng tử hoá (chỉ còn sai số làm tròn của DCT)
//     --bypass    : bỏ cả DCT lẫn lượng tử hoá, PSNR phải bằng vô cực (kiểm tra vòng lặp)
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "picture.h"     // Plane, Picture, readFrame, writeFrame, padPicture, initPicture, getBlock, writeBlock
#include "block_ops.h"   // computeResidual, addBlocks
#include "intra_pred.h"  // RefSamples, getRefSamples, predictDC, kModeXxx
#include "dct.h"         // forwardDct, inverseDct
#include "quant.h"       // quantize, dequantize, qStep
#include "metrics.h"     // computeMse, computePsnr
#include "rate.h"        // Histogram, entropyBits

// Cách chạy khối Transform/Quantization. Hai chế độ sau dùng để kiểm tra, tách từng nguồn sai số.
enum class TqMode {
    Normal,    // DCT -> Q -> IQ -> IDCT
    NoQuant,   // DCT -> IDCT: bỏ lượng tử hoá, chỉ còn sai số làm tròn của DCT
    Bypass,    // residual' = residual: không còn sai số nào, recon phải giống hệt ảnh gốc
};

const char* tqModeName(TqMode mode) {
    switch (mode) {
        case TqMode::NoQuant: return "no-quant (DCT -> IDCT)";
        case TqMode::Bypass:  return "bypass (bo qua DCT va quant)";
        default:              return "normal (DCT -> Q -> IQ -> IDCT)";
    }
}

// Tên ngắn, dùng trong tên file recon và cột tq_mode của CSV
const char* tqModeTag(TqMode mode) {
    switch (mode) {
        case TqMode::NoQuant: return "no-quant";
        case TqMode::Bypass:  return "bypass";
        default:              return "normal";
    }
}

// Kết quả mã hoá một CTU
struct CtuResult {
    int mode = kModeDc;              // mode intra đã chọn
    std::vector<int32_t> sent;       // thứ phải gửi đi: level (normal), hệ số DCT (no-quant) hoặc residual (bypass)
};

// Mã hoá một CTU luma tại (topRow, leftCol) và ghi kết quả tái tạo vào recon.
CtuResult encodeLumaCtu(const Plane& original, Plane& recon, int topRow, int leftCol, int N, int qp, TqMode mode) {
    CtuResult result;
    std::vector<int32_t> orgBlock = getBlock(original, topRow, leftCol, N);

    // Intra prediction: mẫu tham chiếu lấy từ RECON (thứ decoder có), không phải ảnh gốc
    RefSamples ref            = getRefSamples(recon, topRow, leftCol, N);
    std::vector<int32_t> pred = predictDC(ref, true);   // TODO: thay bằng chooseIntraMode + predictIntra của teammate
    result.mode               = kModeDc;

    // Nút trừ
    std::vector<int32_t> residual = computeResidual(orgBlock, pred);

    // Transform -> Quantization -> Dequantization -> Inverse transform (tuỳ chế độ)
    std::vector<int32_t> residual2;   // residual mà decoder khôi phục được
    if (mode == TqMode::Bypass) {
        result.sent = residual;
        residual2   = residual;
    } else if (mode == TqMode::NoQuant) {
        result.sent = forwardDct(residual, N);
        residual2   = inverseDct(result.sent, N);
    } else {
        result.sent = quantize(forwardDct(residual, N), qp, N);
        residual2   = inverseDct(dequantize(result.sent, qp, N), N);
    }

    // Nút cộng -> ghi recon (có clip)
    writeBlock(recon, topRow, leftCol, N, addBlocks(pred, residual2));
    return result;
}

// Kết quả mã hoá luma của một frame
struct FrameResult {
    long long numNonZero = 0;                                        // số giá trị gửi đi khác 0
    double coeffBits     = 0.0;                                      // bit ước lượng cho level
    double modeBits      = 0.0;                                      // bit ước lượng cho mode
    std::vector<long long> modeCount = std::vector<long long>(kNumIntraModes, 0);
    double mseY          = 0.0;
    double psnrY         = 0.0;
    long long timeMs     = 0;
};

// Mã hoá luma của một frame: duyệt CTU theo raster, ghi vào recon, đo rate và distortion.
// picture: ảnh gốc chưa đệm (để tính PSNR); padded: ảnh đã đệm (để mã hoá).
FrameResult encodeFrame(const Picture& picture, const Picture& padded, Picture& recon, int ctuSize, int qp, TqMode mode) {
    FrameResult result;
    Histogram levelHistogram;   // mọi giá trị "gửi đi" của frame
    Histogram modeHistogram;    // mode của từng CTU

    const int numCtuCols = padded.Y.width  / ctuSize;
    const int numCtuRows = padded.Y.height / ctuSize;
    const auto startTime = std::chrono::steady_clock::now();

    // Thứ tự raster: bắt buộc, vì CTU sau lấy tham chiếu từ recon của CTU trước
    for (int ctuRow = 0; ctuRow < numCtuRows; ++ctuRow) {
        for (int ctuCol = 0; ctuCol < numCtuCols; ++ctuCol) {
            CtuResult ctu = encodeLumaCtu(padded.Y, recon.Y, ctuRow * ctuSize, ctuCol * ctuSize, ctuSize, qp, mode);

            levelHistogram.add(ctu.sent);
            modeHistogram.add(ctu.mode);
            ++result.modeCount[ctu.mode];
            for (int32_t value : ctu.sent) {
                if (value != 0) ++result.numNonZero;
            }
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    result.timeMs    = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    result.coeffBits = entropyBits(levelHistogram);
    result.modeBits  = entropyBits(modeHistogram);

    // So sánh với ảnh GỐC chưa đệm, chỉ trên vùng ảnh thật
    result.mseY  = computeMse(picture.Y, recon.Y, picture.Y.width, picture.Y.height);
    result.psnrY = computePsnr(picture.Y, recon.Y, picture.Y.width, picture.Y.height);
    return result;
}

int main(int argc, char* argv[]) {
    const int WIDTH    = 1920;
    const int HEIGHT   = 1080;
    const int CTU_SIZE = 16;
    const std::string INPUT_PATH = "Input/Input.yuv";

    // ---- Đọc tham số dòng lệnh
    int qp = 32;
    int maxFrames = 0;               // 0 = mọi frame
    std::string outDir = "Output";
    TqMode mode = TqMode::Normal;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bypass") {
            mode = TqMode::Bypass;
        } else if (arg == "--no-quant") {
            mode = TqMode::NoQuant;
        } else if (arg == "--frames" && i + 1 < argc) {
            maxFrames = std::atoi(argv[++i]);          // ++i: lấy luôn tham số kế tiếp làm giá trị
        } else if (arg == "--out" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (arg.rfind("--", 0) == 0) {          // bắt đầu bằng "--" nhưng không phải cờ đã biết
            std::cerr << "Error: co khong hop le hoac thieu gia tri '" << arg << "'." << std::endl;
            return 1;
        } else {
            qp = std::atoi(argv[i]);
        }
    }
    if (qp < 0 || qp > 51) {
        std::cerr << "Error: QP phai nam trong [0, 51]." << std::endl;
        return 1;
    }

    // ---- Mở file vào, đếm số frame
    std::ifstream inputFile(INPUT_PATH, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Error: Could not open file '" << INPUT_PATH << "'." << std::endl;
        return 1;
    }
    const long long frameSize = static_cast<long long>(WIDTH) * HEIGHT * 3 / 2;
    inputFile.seekg(0, std::ios::end);
    const long long fileSize = inputFile.tellg();
    if (fileSize % frameSize != 0) {
        std::cerr << "Error: File size is not a multiple of frame size. Check WIDTH/HEIGHT." << std::endl;
        return 1;
    }
    const int numFramesInFile = static_cast<int>(fileSize / frameSize);
    const int numFrames = (maxFrames > 0) ? std::min(maxFrames, numFramesInFile) : numFramesInFile;

    // ---- Mở file ra: recon.yuv (ghi mới) và results.csv (ghi nối thêm)
    std::filesystem::create_directories(outDir);
    std::string reconPath = outDir + "/recon_qp" + std::to_string(qp);
    if (mode != TqMode::Normal) reconPath += std::string("_") + tqModeTag(mode);
    reconPath += ".yuv";
    const std::string csvPath = outDir + "/results.csv";
    const bool csvIsNew = !std::filesystem::exists(csvPath);

    std::ofstream reconFile(reconPath, std::ios::binary);
    std::ofstream csvFile(csvPath, std::ios::app);
    if (!reconFile || !csvFile) {
        std::cerr << "Error: Khong mo duoc file ket qua trong '" << outDir << "'." << std::endl;
        return 1;
    }
    if (csvIsNew) {
        csvFile << "frame,qp,tq_mode,predictor,nnz,coeff_bits,mode_bits,bpp,mse_y,psnr_y,planar,dc,hor,ver,time_ms\n";
    }

    std::cout << "Input: " << INPUT_PATH << " (" << WIDTH << "x" << HEIGHT << ", " << numFramesInFile << " frame)"
              << ",  ma hoa " << numFrames << " frame" << std::endl;
    std::cout << "QP " << qp << " (Qstep = " << qStep(qp) << "),  che do T/Q: " << tqModeName(mode) << std::endl;
    std::cout << " frame        nnz       bits     bpp    PSNR_Y  time_ms" << std::endl;

    // ---- Mã hoá từng frame (intra độc lập, không frame nào tham chiếu frame khác)
    double sumBpp = 0.0, sumPsnr = 0.0;
    long long sumTimeMs = 0;
    std::vector<long long> totalModeCount(kNumIntraModes, 0);

    for (int frameIndex = 0; frameIndex < numFrames; ++frameIndex) {
        Picture picture;
        if (!readFrame(inputFile, WIDTH, HEIGHT, frameIndex, picture)) {
            std::cerr << "Error: Could not read frame " << frameIndex << "." << std::endl;
            return 1;
        }
        const Picture padded = padPicture(picture, CTU_SIZE);
        Picture recon;
        initPicture(recon, padded.Y.width, padded.Y.height);

        const FrameResult r = encodeFrame(picture, padded, recon, CTU_SIZE, qp, mode);

        // Chưa mã hoá chroma: ghi U, V = 128 (xám) để recon.yuv hiển thị riêng chất lượng luma
        std::fill(recon.U.data.begin(), recon.U.data.end(), 128);
        std::fill(recon.V.data.begin(), recon.V.data.end(), 128);
        if (!writeFrame(reconFile, recon, WIDTH, HEIGHT)) {
            std::cerr << "Error: Ghi recon that bai." << std::endl;
            return 1;
        }

        const double totalBits = r.coeffBits + r.modeBits;
        const double bpp = totalBits / (static_cast<double>(WIDTH) * HEIGHT);   // bit trên mỗi pixel luma
        sumBpp    += bpp;
        sumPsnr   += r.psnrY;
        sumTimeMs += r.timeMs;
        for (int m = 0; m < kNumIntraModes; ++m) totalModeCount[m] += r.modeCount[m];

        std::cout << std::fixed << std::setprecision(4)
                  << std::setw(6) << frameIndex << std::setw(11) << r.numNonZero
                  << std::setw(11) << std::setprecision(0) << totalBits
                  << std::setw(8) << std::setprecision(4) << bpp
                  << std::setw(10) << r.psnrY << std::setw(9) << r.timeMs << std::endl;

        csvFile << std::fixed << std::setprecision(6)
                << frameIndex << ',' << qp << ',' << tqModeTag(mode) << ",dc-only,"
                << r.numNonZero << ',' << r.coeffBits << ',' << r.modeBits << ',' << bpp << ','
                << r.mseY << ',' << r.psnrY << ','
                << r.modeCount[kModePlanar] << ',' << r.modeCount[kModeDc] << ','
                << r.modeCount[kModeHor] << ',' << r.modeCount[kModeVer] << ','
                << r.timeMs << '\n';
    }

    // ---- Tổng kết
    const double avgBpp = sumBpp / numFrames;
    std::cout << std::setprecision(4)
              << "Trung binh: bpp = " << avgBpp << ",  PSNR_Y = " << sumPsnr / numFrames << " dB"
              << ",  thoi gian = " << sumTimeMs / numFrames << " ms/frame" << std::endl;
    if (avgBpp > 0) {
        std::cout << "Ti le nen luma (8 bpp goc / bpp): " << std::setprecision(2) << 8.0 / avgBpp << " : 1" << std::endl;
    }
    std::cout << "Mode: planar = " << totalModeCount[kModePlanar] << ", dc = " << totalModeCount[kModeDc]
              << ", hor = " << totalModeCount[kModeHor] << ", ver = " << totalModeCount[kModeVer] << std::endl;
    std::cout << "Da ghi: " << reconPath << "  va  " << csvPath << std::endl;
    std::cout << "Xem recon: ffplay -f rawvideo -pixel_format yuv420p -video_size " << WIDTH << "x" << HEIGHT
              << " " << reconPath << std::endl;
    return 0;
}
