// tools/encoder.cpp
// Encoder intra đơn giản (hướng B), YUV 4:2:0: mỗi CTU gồm luma 16x16 và hai chroma 8x8.
// Dự đoán: tạm thời chỉ có DC; chroma dùng lại mode của luma (DM). Chroma dùng cùng QP với luma.
//
// Cách dùng (đứng ở thư mục H265):
//   ./out/encoder.exe [QP] [--input FILE] [--size RONGxCAO] [--frames K] [--out DIR] [--no-quant | --bypass]
//     QP          : 0..51, mặc định 32
//     --input FILE: file YUV 4:2:0 8-bit, mặc định Input/Input.yuv (tạo bằng tools/prepare_input.sh)
//     --size WxH  : kích thước frame của file đó, mặc định 1920x1080
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

// ===================== Chỗ ghép code của teammate (Intra Estimation / Prediction) =====================

// Chọn mode intra cho CTU luma. Hiện chỉ có DC.
// TODO: thay bằng chooseIntraMode(original, ref, cost) của teammate (thử Planar/DC/H/V, chọn SAD hoặc SSD nhỏ nhất).
int chooseLumaMode(const std::vector<int32_t>& original, const RefSamples& ref) {
    (void)original;
    (void)ref;
    return kModeDc;
}

// Dự đoán một block theo mode. isLuma = false thì không áp bộ lọc biên. Hiện chỉ có DC.
// TODO: thay bằng predictIntra(ref, mode, isLuma) của teammate.
std::vector<int32_t> predictBlock(const RefSamples& ref, int mode, bool isLuma) {
    if (mode != kModeDc) {
        std::cerr << "Error: mode " << mode << " chua duoc ho tro." << std::endl;
        std::exit(1);
    }
    return predictDC(ref, isLuma);
}

// =====================================================================================================

// Nút trừ -> Transform/Quant (tuỳ chế độ) -> nút cộng -> ghi recon, cho một block N x N đã có prediction.
// Trả về thứ phải gửi đi: level (normal), hệ số DCT (no-quant) hoặc residual (bypass).
std::vector<int32_t> codeResidual(Plane& recon, int topRow, int leftCol, int N,
                                  const std::vector<int32_t>& orgBlock, const std::vector<int32_t>& pred,
                                  int qp, TqMode tqMode) {
    std::vector<int32_t> residual = computeResidual(orgBlock, pred);

    std::vector<int32_t> sent;        // thứ encoder sẽ phải mã hoá vào bitstream
    std::vector<int32_t> residual2;   // residual mà decoder khôi phục được
    if (tqMode == TqMode::Bypass) {
        sent      = residual;
        residual2 = residual;
    } else if (tqMode == TqMode::NoQuant) {
        sent      = forwardDct(residual, N);
        residual2 = inverseDct(sent, N);
    } else {
        sent      = quantize(forwardDct(residual, N), qp, N);
        residual2 = inverseDct(dequantize(sent, qp, N), N);
    }

    writeBlock(recon, topRow, leftCol, N, addBlocks(pred, residual2));   // clip [0, 255] trong writeBlock
    return sent;
}

// Mã hoá một block chroma N x N với mode cho trước (lấy từ luma)
std::vector<int32_t> encodeChromaBlock(const Plane& original, Plane& recon, int topRow, int leftCol, int N,
                                       int mode, int qp, TqMode tqMode) {
    std::vector<int32_t> orgBlock = getBlock(original, topRow, leftCol, N);
    RefSamples ref                = getRefSamples(recon, topRow, leftCol, N);   // từ RECON của chính plane này
    return codeResidual(recon, topRow, leftCol, N, orgBlock, predictBlock(ref, mode, false), qp, tqMode);
}

// Kết quả mã hoá một CTU
struct CtuResult {
    int lumaMode = kModeDc;
    std::vector<int32_t> sentY, sentU, sentV;
};

// Mã hoá một CTU tại (topRow, leftCol) theo toạ độ LUMA: luma ctuSize x ctuSize rồi U, V (ctuSize/2) x (ctuSize/2).
CtuResult encodeCtu(const Picture& padded, Picture& recon, int topRow, int leftCol, int ctuSize, int qp, TqMode tqMode) {
    CtuResult result;

    // ---- Luma: chọn mode, dự đoán, mã hoá residual
    std::vector<int32_t> orgY = getBlock(padded.Y, topRow, leftCol, ctuSize);
    RefSamples refY           = getRefSamples(recon.Y, topRow, leftCol, ctuSize);   // từ RECON
    result.lumaMode           = chooseLumaMode(orgY, refY);
    result.sentY = codeResidual(recon.Y, topRow, leftCol, ctuSize, orgY,
                                predictBlock(refY, result.lumaMode, true), qp, tqMode);

    // ---- Chroma 4:2:0: cùng vùng ảnh nên toạ độ và kích thước chia 2; dùng lại mode của luma (DM)
    const int chromaTop  = topRow / 2;
    const int chromaLeft = leftCol / 2;
    const int chromaSize = ctuSize / 2;
    result.sentU = encodeChromaBlock(padded.U, recon.U, chromaTop, chromaLeft, chromaSize, result.lumaMode, qp, tqMode);
    result.sentV = encodeChromaBlock(padded.V, recon.V, chromaTop, chromaLeft, chromaSize, result.lumaMode, qp, tqMode);
    return result;
}

long long countNonZero(const std::vector<int32_t>& values) {
    long long count = 0;
    for (int32_t value : values) {
        if (value != 0) ++count;
    }
    return count;
}

// Kết quả mã hoá một frame
struct FrameResult {
    long long nnzY = 0, nnzC = 0;                 // số giá trị gửi đi khác 0: luma, chroma (U + V)
    double bitsY = 0.0, bitsC = 0.0;              // bit ước lượng cho level: luma, chroma (U + V)
    double modeBits = 0.0;                        // bit ước lượng cho mode (chroma dùng DM nên không tốn thêm)
    std::vector<long long> modeCount = std::vector<long long>(kNumIntraModes, 0);
    double mseY = 0.0, mseU = 0.0, mseV = 0.0;
    double psnrY = 0.0, psnrU = 0.0, psnrV = 0.0, psnrYuv = 0.0;
    long long timeMs = 0;
};

// Mã hoá một frame: duyệt CTU theo raster, ghi vào recon, đo rate và distortion.
// picture: ảnh gốc chưa đệm (để tính PSNR); padded: ảnh đã đệm (để mã hoá).
FrameResult encodeFrame(const Picture& picture, const Picture& padded, Picture& recon, int ctuSize, int qp, TqMode tqMode) {
    FrameResult result;
    Histogram histY, histU, histV;   // mỗi thành phần một bảng thống kê riêng vì phân bố level khác nhau
    Histogram modeHistogram;

    const int numCtuCols = padded.Y.width  / ctuSize;
    const int numCtuRows = padded.Y.height / ctuSize;
    const auto startTime = std::chrono::steady_clock::now();

    // Thứ tự raster: bắt buộc, vì CTU sau lấy tham chiếu từ recon của CTU trước
    for (int ctuRow = 0; ctuRow < numCtuRows; ++ctuRow) {
        for (int ctuCol = 0; ctuCol < numCtuCols; ++ctuCol) {
            CtuResult ctu = encodeCtu(padded, recon, ctuRow * ctuSize, ctuCol * ctuSize, ctuSize, qp, tqMode);

            histY.add(ctu.sentY);
            histU.add(ctu.sentU);
            histV.add(ctu.sentV);
            modeHistogram.add(ctu.lumaMode);
            ++result.modeCount[ctu.lumaMode];
            result.nnzY += countNonZero(ctu.sentY);
            result.nnzC += countNonZero(ctu.sentU) + countNonZero(ctu.sentV);
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    result.timeMs   = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    result.bitsY    = entropyBits(histY);
    result.bitsC    = entropyBits(histU) + entropyBits(histV);
    result.modeBits = entropyBits(modeHistogram);

    // So sánh với ảnh GỐC chưa đệm, chỉ trên vùng ảnh thật của từng plane
    result.mseY  = computeMse(picture.Y, recon.Y, picture.Y.width, picture.Y.height);
    result.mseU  = computeMse(picture.U, recon.U, picture.U.width, picture.U.height);
    result.mseV  = computeMse(picture.V, recon.V, picture.V.width, picture.V.height);
    result.psnrY = computePsnr(picture.Y, recon.Y, picture.Y.width, picture.Y.height);
    result.psnrU = computePsnr(picture.U, recon.U, picture.U.width, picture.U.height);
    result.psnrV = computePsnr(picture.V, recon.V, picture.V.width, picture.V.height);
    // PSNR tổng hợp kiểu JVET: luma quan trọng gấp 6 lần mỗi chroma
    result.psnrYuv = (6.0 * result.psnrY + result.psnrU + result.psnrV) / 8.0;
    return result;
}

int main(int argc, char* argv[]) {
    const int CTU_SIZE = 16;

    // ---- Tham số, đổi được bằng cờ dòng lệnh
    int width  = 1920;
    int height = 1080;
    std::string inputPath = "Input/Input.yuv";
    int qp = 32;
    int maxFrames = 0;               // 0 = mọi frame
    std::string outDir = "Output";
    TqMode tqMode = TqMode::Normal;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--bypass") {
            tqMode = TqMode::Bypass;
        } else if (arg == "--no-quant") {
            tqMode = TqMode::NoQuant;
        } else if (arg == "--frames" && i + 1 < argc) {
            maxFrames = std::atoi(argv[++i]);          // ++i: lấy luôn tham số kế tiếp làm giá trị
        } else if (arg == "--out" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (arg == "--size" && i + 1 < argc) {
            const std::string value = argv[++i];
            const size_t separator = value.find('x');                       // dạng "1920x1080"
            width  = (separator == std::string::npos) ? 0 : std::atoi(value.substr(0, separator).c_str());
            height = (separator == std::string::npos) ? 0 : std::atoi(value.substr(separator + 1).c_str());
            if (width <= 0 || height <= 0 || width % 2 != 0 || height % 2 != 0) {
                std::cerr << "Error: --size phai co dang RONGxCAO voi hai so chan (4:2:0). Vi du: --size 1920x800." << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Dung: ./out/encoder.exe [QP] [--input FILE] [--size RONGxCAO] [--frames K] [--out DIR]"
                         " [--no-quant | --bypass]" << std::endl;
            return 0;
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
    std::ifstream inputFile(inputPath, std::ios::binary);
    if (!inputFile) {
        std::cerr << "Error: Could not open file '" << inputPath << "'." << std::endl;
        return 1;
    }
    const long long frameSize = static_cast<long long>(width) * height * 3 / 2;
    inputFile.seekg(0, std::ios::end);
    const long long fileSize = inputFile.tellg();
    if (fileSize % frameSize != 0) {
        std::cerr << "Error: Kich thuoc file (" << fileSize << " byte) khong chia het cho kich thuoc frame ("
                  << frameSize << " byte). Kiem tra lai --size." << std::endl;
        return 1;
    }
    const int numFramesInFile = static_cast<int>(fileSize / frameSize);
    const int numFrames = (maxFrames > 0) ? std::min(maxFrames, numFramesInFile) : numFramesInFile;

    // ---- Mở file ra: recon.yuv (ghi mới) và results.csv (ghi nối thêm)
    std::filesystem::create_directories(outDir);
    std::string reconPath = outDir + "/recon_qp" + std::to_string(qp);
    if (tqMode != TqMode::Normal) reconPath += std::string("_") + tqModeTag(tqMode);
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
        csvFile << "frame,qp,tq_mode,predictor,nnz_y,nnz_c,bits_y,bits_c,mode_bits,bpp,"
                   "mse_y,mse_u,mse_v,psnr_y,psnr_u,psnr_v,psnr_yuv,planar,dc,hor,ver,time_ms\n";
    }

    std::cout << "Input: " << inputPath << " (" << width << "x" << height << ", " << numFramesInFile << " frame)"
              << ",  ma hoa " << numFrames << " frame" << std::endl;
    std::cout << "QP " << qp << " (Qstep = " << qStep(qp) << "),  che do T/Q: " << tqModeName(tqMode) << std::endl;
    std::cout << " frame       bits     bpp   PSNR_Y   PSNR_U   PSNR_V PSNR_YUV  time_ms" << std::endl;

    // ---- Mã hoá từng frame (intra độc lập, không frame nào tham chiếu frame khác)
    double sumBpp = 0.0, sumPsnrY = 0.0, sumPsnrU = 0.0, sumPsnrV = 0.0, sumPsnrYuv = 0.0;
    long long sumTimeMs = 0;
    std::vector<long long> totalModeCount(kNumIntraModes, 0);

    for (int frameIndex = 0; frameIndex < numFrames; ++frameIndex) {
        Picture picture;
        if (!readFrame(inputFile, width, height, frameIndex, picture)) {
            std::cerr << "Error: Could not read frame " << frameIndex << "." << std::endl;
            return 1;
        }
        const Picture padded = padPicture(picture, CTU_SIZE);
        Picture recon;
        initPicture(recon, padded.Y.width, padded.Y.height);

        const FrameResult r = encodeFrame(picture, padded, recon, CTU_SIZE, qp, tqMode);
        if (!writeFrame(reconFile, recon, width, height)) {
            std::cerr << "Error: Ghi recon that bai." << std::endl;
            return 1;
        }

        const double totalBits = r.bitsY + r.bitsC + r.modeBits;
        const double bpp = totalBits / (static_cast<double>(width) * height);   // bit trên mỗi pixel luma
        sumBpp     += bpp;
        sumPsnrY   += r.psnrY;
        sumPsnrU   += r.psnrU;
        sumPsnrV   += r.psnrV;
        sumPsnrYuv += r.psnrYuv;
        sumTimeMs  += r.timeMs;
        for (int m = 0; m < kNumIntraModes; ++m) totalModeCount[m] += r.modeCount[m];

        std::cout << std::fixed
                  << std::setw(6) << frameIndex
                  << std::setw(11) << std::setprecision(0) << totalBits
                  << std::setw(8) << std::setprecision(4) << bpp
                  << std::setw(9) << r.psnrY << std::setw(9) << r.psnrU << std::setw(9) << r.psnrV
                  << std::setw(9) << r.psnrYuv << std::setw(9) << r.timeMs << std::endl;

        csvFile << std::fixed << std::setprecision(6)
                << frameIndex << ',' << qp << ',' << tqModeTag(tqMode) << ",dc-only,"
                << r.nnzY << ',' << r.nnzC << ',' << r.bitsY << ',' << r.bitsC << ',' << r.modeBits << ',' << bpp << ','
                << r.mseY << ',' << r.mseU << ',' << r.mseV << ','
                << r.psnrY << ',' << r.psnrU << ',' << r.psnrV << ',' << r.psnrYuv << ','
                << r.modeCount[kModePlanar] << ',' << r.modeCount[kModeDc] << ','
                << r.modeCount[kModeHor] << ',' << r.modeCount[kModeVer] << ','
                << r.timeMs << '\n';
    }

    // ---- Tổng kết
    const double avgBpp = sumBpp / numFrames;
    std::cout << std::setprecision(4)
              << "Trung binh: bpp = " << avgBpp
              << ",  PSNR Y/U/V = " << sumPsnrY / numFrames << " / " << sumPsnrU / numFrames << " / " << sumPsnrV / numFrames
              << " dB,  PSNR_YUV = " << sumPsnrYuv / numFrames << " dB"
              << ",  thoi gian = " << sumTimeMs / numFrames << " ms/frame" << std::endl;
    if (avgBpp > 0) {
        // Ảnh gốc 4:2:0 8-bit: 8 bit luma + 2 * (8 / 4) bit chroma = 12 bit cho mỗi pixel luma
        std::cout << "Ti le nen (12 bpp goc / bpp): " << std::setprecision(2) << 12.0 / avgBpp << " : 1" << std::endl;
    }
    std::cout << "Mode: planar = " << totalModeCount[kModePlanar] << ", dc = " << totalModeCount[kModeDc]
              << ", hor = " << totalModeCount[kModeHor] << ", ver = " << totalModeCount[kModeVer] << std::endl;
    std::cout << "Da ghi: " << reconPath << "  va  " << csvPath << std::endl;
    std::cout << "Xem recon: ffplay -f rawvideo -pixel_format yuv420p -video_size " << width << "x" << height
              << " " << reconPath << std::endl;
    return 0;
}
