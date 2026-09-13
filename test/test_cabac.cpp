#include "cabac.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

namespace {

struct ContextBin {
    uint8_t value;
    uint8_t contextIndex;
};

void TestRoundTrip(std::mt19937& random, int binCount) {
    std::array<CabacContext, 4> encoderContexts = {
        CabacContext(static_cast<uint8_t>(random() % 64), random() & 1),
        CabacContext(static_cast<uint8_t>(random() % 64), random() & 1),
        CabacContext::FromInitValue(static_cast<int>(random() % 52), 154),
        CabacContext::FromInitValue(static_cast<int>(random() % 52), 139),
    };
    std::array<CabacContext, 4> decoderContexts = encoderContexts;

    std::vector<ContextBin> bins;
    bins.reserve(binCount);
    for (int i = 0; i < binCount; ++i) {
        bins.push_back({static_cast<uint8_t>(random() & 1),
                        static_cast<uint8_t>(random() % encoderContexts.size())});
    }

    CabacEncoder encoder;
    for (const ContextBin bin : bins) {
        encoder.EncodeBin(bin.value, encoderContexts[bin.contextIndex]);
    }
    const int bypassCount = static_cast<int>(random() % 33);
    const uint32_t bypassBins = random();
    encoder.EncodeBypassBins(bypassBins, bypassCount);
    const std::vector<uint8_t> bitstream = encoder.Finish();
    assert(!bitstream.empty());
    assert(encoder.Finish() == bitstream);  // Finish phải idempotent.

    CabacDecoder decoder(bitstream);
    for (const ContextBin bin : bins) {
        assert(decoder.DecodeBin(decoderContexts[bin.contextIndex]) == bin.value);
    }
    const uint32_t decodedBypass = decoder.DecodeBypassBins(bypassCount);
    if (bypassCount == 32) {
        assert(decodedBypass == bypassBins);
    } else if (bypassCount > 0) {
        assert(decodedBypass == (bypassBins & ((1U << bypassCount) - 1)));
    } else {
        assert(decodedBypass == 0);
    }
    assert(decoder.DecodeTerminate() == 1);

    for (std::size_t i = 0; i < encoderContexts.size(); ++i) {
        assert(encoderContexts[i].State() == decoderContexts[i].State());
        assert(encoderContexts[i].Mps() == decoderContexts[i].Mps());
    }
}

}  // namespace

int main() {
    std::mt19937 random(0x265cabacU);
    TestRoundTrip(random, 0);
    TestRoundTrip(random, 1);
    for (int trial = 0; trial < 100; ++trial) {
        TestRoundTrip(random, static_cast<int>(random() % 2001));
    }

    std::cout << "CABAC round-trip OK: 102 deterministic test sequences\n";
    return 0;
}
