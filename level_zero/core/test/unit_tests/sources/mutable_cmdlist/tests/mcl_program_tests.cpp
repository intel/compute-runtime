/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_decoder.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_encoder.h"

#include <array>
#include <memory>
#include <vector>

namespace L0 {
namespace ult {

using namespace L0::MCL;
using namespace L0::MCL::Program;

struct MockMclEncoder : public Encoder::MclEncoder {
    using Encoder::MclEncoder::addSymbol;
    using Encoder::MclEncoder::program;
};

class MclProgramKernelDataTest : public ::testing::Test {
  public:
    std::vector<uint8_t> encodeKernelData(uint8_t numLocalIdChannels) {
        auto kernelData = std::make_unique<KernelData>();
        kernelData->kernelName = kernelName;
        kernelData->kernelIsa = {kernelIsa.data(), kernelIsa.size()};
        kernelData->simdSize = simdSize;
        kernelData->numLocalIdChannels = numLocalIdChannels;

        std::vector<std::unique_ptr<KernelData>> kernelDataVec;
        kernelDataVec.push_back(std::move(kernelData));

        Encoder::MclEncoder encoder;
        encoder.parseKernelData(kernelDataVec);
        return encoder.encodeProgram();
    }

    const std::string kernelName = "kernel";
    const std::array<uint8_t, 16> kernelIsa = {};
    const uint8_t simdSize = 32;
};

TEST(MclProgramKernelSymbolTest, givenNumLocalIdChannelsWhenKernelSymbolIsPackedAndUnpackedThenAllFieldsArePreserved) {
    for (uint8_t numLocalIdChannels = 0; numLocalIdChannels <= 3; numLocalIdChannels++) {
        Symbols::KernelSymbol packed(7u, 0x80u, 32u, 1u, 0xau, numLocalIdChannels);
        Symbols::KernelSymbol unpacked(packed.data);

        EXPECT_EQ(7u, unpacked.kernelDataId);
        EXPECT_EQ(0x80u, unpacked.skipPerThreadDataLoad);
        EXPECT_EQ(32u, unpacked.simdSize);
        EXPECT_EQ(1u, unpacked.passInlineData);
        EXPECT_EQ(0xau, unpacked.indirectOffset);
        EXPECT_EQ(1u, unpacked.hasNumLocalIdChannels);
        EXPECT_EQ(numLocalIdChannels, unpacked.numLocalIdChannels);
    }
}

TEST_F(MclProgramKernelDataTest, givenKernelDataWhenEncodedAndDecodedThenNumLocalIdChannelsIsPreserved) {
    for (uint8_t numLocalIdChannels = 0; numLocalIdChannels <= 3; numLocalIdChannels++) {
        auto binary = encodeKernelData(numLocalIdChannels);

        Decoder::MclDecoder decoder;
        ASSERT_TRUE(decoder.decodeElf({binary.data(), binary.size()}));
        decoder.parseSymbols();

        auto &decodedKernelData = decoder.getKernelData();
        ASSERT_EQ(1u, decodedKernelData.size());
        EXPECT_EQ(kernelName, decodedKernelData[0].kernelName);
        EXPECT_EQ(simdSize, decodedKernelData[0].simdSize);
        EXPECT_EQ(numLocalIdChannels, decodedKernelData[0].numLocalIdChannels);
    }
}

TEST_F(MclProgramKernelDataTest, givenKernelSymbolWithoutNumLocalIdChannelsWhenDecodedThenDefaultNumLocalIdChannelsIsUsed) {
    MockMclEncoder encoder;
    encoder.program.ih.insert(encoder.program.ih.end(), kernelIsa.begin(), kernelIsa.end());
    encoder.addSymbol(Symbols::SymbolNames::kernelPrefix.str() + kernelName, Sections::SectionType::shtIh, Symbols::SymbolType::heap, 0u, kernelIsa.size());

    Symbols::KernelSymbol legacyKernelSymbol(0u, 0u, simdSize, 0u, 0u, 1u);
    legacyKernelSymbol.hasNumLocalIdChannels = 0;
    legacyKernelSymbol.numLocalIdChannels = 0;
    encoder.addSymbol(Symbols::SymbolNames::kernelDataPrefix.str() + kernelName, Sections::SectionType::shtUndef, Symbols::SymbolType::kernel, legacyKernelSymbol.data, 0u);
    auto binary = encoder.encodeProgram();

    Decoder::MclDecoder decoder;
    ASSERT_TRUE(decoder.decodeElf({binary.data(), binary.size()}));
    decoder.parseSymbols();

    auto &decodedKernelData = decoder.getKernelData();
    ASSERT_EQ(1u, decodedKernelData.size());
    EXPECT_EQ(simdSize, decodedKernelData[0].simdSize);
    EXPECT_EQ(KernelData{}.numLocalIdChannels, decodedKernelData[0].numLocalIdChannels);
}

} // namespace ult
} // namespace L0
