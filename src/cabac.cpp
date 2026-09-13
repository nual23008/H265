#include "cabac.h"

#include <algorithm>
#include <stdexcept>

namespace {

// HEVC Table 9-46: khoảng range dành cho LPS (least probable symbol).
constexpr uint8_t kLpsTable[64][4] = {
    {128, 176, 208, 240}, {128, 167, 197, 227}, {128, 158, 187, 216},
    {123, 150, 178, 205}, {116, 142, 169, 195}, {111, 135, 160, 185},
    {105, 128, 152, 175}, {100, 122, 144, 166}, { 95, 116, 137, 158},
    { 90, 110, 130, 150}, { 85, 104, 123, 142}, { 81,  99, 117, 135},
    { 77,  94, 111, 128}, { 73,  89, 105, 122}, { 69,  85, 100, 116},
    { 66,  80,  95, 110}, { 62,  76,  90, 104}, { 59,  72,  86,  99},
    { 56,  69,  81,  94}, { 53,  65,  77,  89}, { 51,  62,  73,  85},
    { 48,  59,  69,  80}, { 46,  56,  66,  76}, { 43,  53,  63,  72},
    { 41,  50,  59,  69}, { 39,  48,  56,  65}, { 37,  45,  54,  62},
    { 35,  43,  51,  59}, { 33,  41,  48,  56}, { 32,  39,  46,  53},
    { 30,  37,  43,  50}, { 29,  35,  41,  48}, { 27,  33,  39,  45},
    { 26,  31,  37,  43}, { 24,  30,  35,  41}, { 23,  28,  33,  39},
    { 22,  27,  32,  37}, { 21,  26,  30,  35}, { 20,  24,  29,  33},
    { 19,  23,  27,  31}, { 18,  22,  26,  30}, { 17,  21,  25,  28},
    { 16,  20,  23,  27}, { 15,  19,  22,  25}, { 14,  18,  21,  24},
    { 14,  17,  20,  23}, { 13,  16,  19,  22}, { 12,  15,  18,  21},
    { 12,  14,  17,  20}, { 11,  14,  16,  19}, { 11,  13,  15,  18},
    { 10,  12,  15,  17}, { 10,  12,  14,  16}, {  9,  11,  13,  15},
    {  9,  11,  12,  14}, {  8,  10,  12,  14}, {  8,   9,  11,  13},
    {  7,   9,  11,  12}, {  7,   9,  10,  12}, {  7,   8,  10,  11},
    {  6,   8,   9,  11}, {  6,   7,   9,  10}, {  6,   7,   8,   9},
    {  2,   2,   2,   2}
};

// HEVC Table 9-47: trạng thái kế tiếp sau khi gặp MPS hoặc LPS.
constexpr uint8_t kNextStateMps[64] = {
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63
};

constexpr uint8_t kNextStateLps[64] = {
     0,  0,  1,  2,  2,  4,  4,  5,  6,  7,  8,  9,  9, 11, 11, 12,
    13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
    24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
    33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63
};

int RenormalisationBits(uint32_t range) {
    int bits = 0;
    while (range < 256) {
        range <<= 1;
        ++bits;
    }
    return bits;
}

}  // namespace

CabacContext::CabacContext(uint8_t state, uint8_t mps) {
    Reset(state, mps);
}

CabacContext CabacContext::FromInitValue(int qp, uint8_t initValue) {
    qp = std::clamp(qp, 0, 51);
    const int slope = static_cast<int>(initValue >> 4) * 5 - 45;
    const int offset = static_cast<int>(initValue & 15) * 8 - 16;
    const int initialState = std::clamp(((slope * qp) >> 4) + offset, 1, 126);

    if (initialState >= 64) {
        return CabacContext(static_cast<uint8_t>(initialState - 64), 1);
    }
    return CabacContext(static_cast<uint8_t>(63 - initialState), 0);
}

void CabacContext::Reset(uint8_t state, uint8_t mps) {
    if (state > 63 || mps > 1) {
        throw std::invalid_argument("CABAC context requires state 0..63 and MPS 0 or 1");
    }
    state_ = state;
    mps_ = mps;
}

void CabacContext::UpdateMps() {
    state_ = kNextStateMps[state_];
}

void CabacContext::UpdateLps() {
    if (state_ == 0) {
        mps_ ^= 1;
    }
    state_ = kNextStateLps[state_];
}

CabacEncoder::CabacEncoder() {
    Reset();
}

void CabacEncoder::Reset() {
    low_ = 0;
    range_ = 510;
    bitsLeft_ = 23;
    bufferedByte_ = 0xff;
    numBufferedBytes_ = 0;
    bytes_.clear();
    bitsInLastByte_ = 0;
    finished_ = false;
}

void CabacEncoder::CheckCanEncode(uint8_t bin) const {
    if (finished_) {
        throw std::logic_error("CABAC encoder has already been finished");
    }
    if (bin > 1) {
        throw std::invalid_argument("CABAC bin must be 0 or 1");
    }
}

void CabacEncoder::EncodeBin(uint8_t bin, CabacContext& context) {
    CheckCanEncode(bin);
    const uint32_t lpsRange = kLpsTable[context.state_][(range_ >> 6) & 3];
    range_ -= lpsRange;

    if (bin != context.mps_) {
        const int shift = RenormalisationBits(lpsRange);
        low_ = (low_ + range_) << shift;
        range_ = lpsRange << shift;
        context.UpdateLps();
        bitsLeft_ -= shift;
        TestAndWriteOut();
        return;
    }

    context.UpdateMps();
    if (range_ < 256) {
        low_ <<= 1;
        range_ <<= 1;
        --bitsLeft_;
        TestAndWriteOut();
    }
}

void CabacEncoder::EncodeBypass(uint8_t bin) {
    CheckCanEncode(bin);
    low_ <<= 1;
    if (bin != 0) {
        low_ += range_;
    }
    --bitsLeft_;
    TestAndWriteOut();
}

void CabacEncoder::EncodeBypassBins(uint32_t bins, int numBins) {
    if (numBins < 0 || numBins > 32) {
        throw std::invalid_argument("CABAC bypass bin count must be 0..32");
    }
    for (int bit = numBins - 1; bit >= 0; --bit) {
        EncodeBypass(static_cast<uint8_t>((bins >> bit) & 1U));
    }
}

void CabacEncoder::EncodeTerminate(uint8_t bin) {
    CheckCanEncode(bin);
    range_ -= 2;
    if (bin != 0) {
        low_ = (low_ + range_) << 7;
        range_ = 2U << 7;
        bitsLeft_ -= 7;
    } else if (range_ >= 256) {
        return;
    } else {
        low_ <<= 1;
        range_ <<= 1;
        --bitsLeft_;
    }
    TestAndWriteOut();
}

std::vector<uint8_t> CabacEncoder::Finish() {
    if (finished_) {
        return bytes_;
    }

    EncodeTerminate(1);
    if ((low_ >> (32 - bitsLeft_)) != 0) {
        WriteByte(bufferedByte_ + 1);
        while (numBufferedBytes_ > 1) {
            WriteByte(0x00);
            --numBufferedBytes_;
        }
        low_ -= 1U << (32 - bitsLeft_);
    } else {
        if (numBufferedBytes_ > 0) {
            WriteByte(bufferedByte_);
        }
        while (numBufferedBytes_ > 1) {
            WriteByte(0xff);
            --numBufferedBytes_;
        }
    }

    WriteBits(low_ >> 8, 24 - bitsLeft_);
    WriteBits(1, 1);
    AlignWithZero();
    finished_ = true;
    return bytes_;
}

void CabacEncoder::TestAndWriteOut() {
    if (bitsLeft_ < 12) {
        WriteOut();
    }
}

void CabacEncoder::WriteOut() {
    const uint32_t leadByte = low_ >> (24 - bitsLeft_);
    bitsLeft_ += 8;
    low_ &= 0xffffffffU >> bitsLeft_;

    if (leadByte == 0xff) {
        ++numBufferedBytes_;
        return;
    }

    if (numBufferedBytes_ > 0) {
        const uint32_t carry = leadByte >> 8;
        WriteByte(bufferedByte_ + carry);
        const uint32_t fillByte = (0xff + carry) & 0xff;
        while (numBufferedBytes_ > 1) {
            WriteByte(fillByte);
            --numBufferedBytes_;
        }
    } else {
        numBufferedBytes_ = 1;
    }
    bufferedByte_ = leadByte & 0xff;
}

void CabacEncoder::WriteBits(uint32_t value, int numBits) {
    for (int bit = numBits - 1; bit >= 0; --bit) {
        if (bitsInLastByte_ == 0) {
            bytes_.push_back(0);
        }
        bytes_.back() |= static_cast<uint8_t>(((value >> bit) & 1U)
                                              << (7 - bitsInLastByte_));
        bitsInLastByte_ = (bitsInLastByte_ + 1) & 7;
    }
}

void CabacEncoder::WriteByte(uint32_t value) {
    WriteBits(value & 0xff, 8);
}

void CabacEncoder::AlignWithZero() {
    while (bitsInLastByte_ != 0) {
        WriteBits(0, 1);
    }
}

CabacDecoder::CabacDecoder(const std::vector<uint8_t>& bytes)
    : bytes_(bytes), bytePosition_(0), range_(510), value_(0), bitsNeeded_(-8) {
    if (bytes.empty()) {
        throw std::invalid_argument("CABAC bitstream is empty");
    }
    value_ = ReadByte() << 8;
    value_ |= ReadByte();
}

uint8_t CabacDecoder::DecodeBin(CabacContext& context) {
    const uint32_t lpsRange = kLpsTable[context.state_][(range_ >> 6) & 3];
    range_ -= lpsRange;
    const uint32_t scaledRange = range_ << 7;

    if (value_ < scaledRange) {
        const uint8_t bin = context.mps_;
        context.UpdateMps();
        if (scaledRange < (256U << 7)) {
            range_ = scaledRange >> 6;
            value_ <<= 1;
            if (++bitsNeeded_ == 0) {
                bitsNeeded_ = -8;
                value_ += ReadByte();
            }
        }
        return bin;
    }

    const uint8_t bin = context.mps_ ^ 1;
    const int shift = RenormalisationBits(lpsRange);
    value_ = (value_ - scaledRange) << shift;
    range_ = lpsRange << shift;
    context.UpdateLps();
    bitsNeeded_ += shift;
    if (bitsNeeded_ >= 0) {
        value_ += ReadByte() << bitsNeeded_;
        bitsNeeded_ -= 8;
    }
    return bin;
}

uint8_t CabacDecoder::DecodeBypass() {
    value_ <<= 1;
    if (++bitsNeeded_ >= 0) {
        bitsNeeded_ = -8;
        value_ += ReadByte();
    }
    const uint32_t scaledRange = range_ << 7;
    if (value_ >= scaledRange) {
        value_ -= scaledRange;
        return 1;
    }
    return 0;
}

uint32_t CabacDecoder::DecodeBypassBins(int numBins) {
    if (numBins < 0 || numBins > 32) {
        throw std::invalid_argument("CABAC bypass bin count must be 0..32");
    }
    uint32_t bins = 0;
    for (int i = 0; i < numBins; ++i) {
        bins = (bins << 1) | DecodeBypass();
    }
    return bins;
}

uint8_t CabacDecoder::DecodeTerminate() {
    range_ -= 2;
    const uint32_t scaledRange = range_ << 7;
    if (value_ >= scaledRange) {
        return 1;
    }
    if (scaledRange < (256U << 7)) {
        range_ = scaledRange >> 6;
        value_ <<= 1;
        if (++bitsNeeded_ == 0) {
            bitsNeeded_ = -8;
            value_ += ReadByte();
        }
    }
    return 0;
}

uint32_t CabacDecoder::ReadByte() {
    // Sau stop/alignment bit, CABAC cho phép coi phần đệm còn lại là bit 0.
    if (bytePosition_ >= bytes_.size()) {
        return 0;
    }
    return bytes_[bytePosition_++];
}
