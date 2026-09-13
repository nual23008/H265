#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Mô hình xác suất CABAC của HEVC. state=0 nghĩa là chưa chắc chắn,
// state=63 nghĩa là rất chắc chắn; mps là ký hiệu có xác suất cao hơn.
class CabacContext {
public:
    CabacContext(uint8_t state = 0, uint8_t mps = 0);

    // Khởi tạo context từ initValue và slice QP theo HEVC.
    static CabacContext FromInitValue(int qp, uint8_t initValue);
    void Reset(uint8_t state = 0, uint8_t mps = 0);

    uint8_t State() const { return state_; }
    uint8_t Mps() const { return mps_; }

private:
    friend class CabacEncoder;
    friend class CabacDecoder;

    void UpdateMps();
    void UpdateLps();

    uint8_t state_;
    uint8_t mps_;
};

// Bộ mã hóa số học nhị phân CABAC 9-bit của HEVC. Syntax element phải được
// binarize trước, sau đó truyền từng bin vào lớp này.
class CabacEncoder {
public:
    CabacEncoder();

    void Reset();
    void EncodeBin(uint8_t bin, CabacContext& context);
    void EncodeBypass(uint8_t bin);
    void EncodeBypassBins(uint32_t bins, int numBins);
    void EncodeTerminate(uint8_t bin);

    // Mã hóa termination bin bằng 1, flush coder và căn chỉnh tới byte.
    std::vector<uint8_t> Finish();

private:
    void TestAndWriteOut();
    void WriteOut();
    void WriteBits(uint32_t value, int numBits);
    void WriteByte(uint32_t value);
    void AlignWithZero();
    void CheckCanEncode(uint8_t bin) const;

    uint32_t low_;
    uint32_t range_;
    int bitsLeft_;
    uint32_t bufferedByte_;
    int numBufferedBytes_;
    std::vector<uint8_t> bytes_;
    int bitsInLastByte_;
    bool finished_;
};

class CabacDecoder {
public:
    explicit CabacDecoder(const std::vector<uint8_t>& bytes);

    uint8_t DecodeBin(CabacContext& context);
    uint8_t DecodeBypass();
    uint32_t DecodeBypassBins(int numBins);
    uint8_t DecodeTerminate();

private:
    uint32_t ReadByte();

    const std::vector<uint8_t>& bytes_;
    std::size_t bytePosition_;
    uint32_t range_;
    uint32_t value_;
    int bitsNeeded_;
};
