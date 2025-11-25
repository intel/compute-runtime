/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using CriDeviceTest = Test<DeviceFixture>;

CRITEST_F(CriDeviceTest, givenCriProductWhenCheckingCapabilitiesThenReturnCorrectValues) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);

    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);

    EXPECT_FALSE(CRI::hwInfo.capabilityTable.supportsImages);

    EXPECT_EQ(1024u, pDevice->getDeviceInfo().maxWorkGroupSize);

    EXPECT_EQ(384u, pDevice->getHardwareInfo().capabilityTable.maxProgrammableSlmSize);

    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.kernelTimestampValidBits);
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.timestampValidBits);
}

CRITEST_F(CriDeviceTest, givenCriProductWhenInitializedThenContextGroupIsEnabled) {
    EXPECT_TRUE(pDevice->getGfxCoreHelper().areSecondaryContextsSupported());
}

CRITEST_F(CriDeviceTest, givenSlmSizeAndHeaplessWhenEncodingThenReturnCorrectValues) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2::SHARED_LOCAL_MEMORY_SIZE;

    struct ComputeSlmTestInput {
        uint32_t expected;
        uint32_t slmSize;
    };

    const auto &hwInfo = pDevice->getHardwareInfo();
    auto releaseHelper = pDevice->getReleaseHelper();
    bool isHeapless = true;

    ComputeSlmTestInput computeSlmValuesXe3pAndLaterTestsInput[] = {
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K), 0 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K), 0 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K), 1 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K), 1 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K), 2 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_3K), 2 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_3K), 3 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K), 3 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K), 4 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_5K), 4 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_5K), 5 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_6K), 5 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_6K), 6 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_7K), 6 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_7K), 7 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K), 7 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K), 8 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_9K), 8 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_9K), 9 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_10K), 9 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_10K), 10 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_11K), 10 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_11K), 11 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_12K), 11 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_12K), 12 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_13K), 12 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_13K), 13 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_14K), 13 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_14K), 14 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_15K), 14 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_15K), 15 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K), 15 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K), 16 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K), 16 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_24K), 24 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K), 24 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_32K), 32 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K), 32 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_48K), 48 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K), 48 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_64K), 64 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K), 64 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_96K), 96 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K), 96 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_128K), 128 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K), 128 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_192K), 192 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K), 192 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_256K), 256 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_320K), 256 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_320K), 320 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K), 320 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_384K), 384 * MemoryConstants::kiloByte}};

    for (const auto &testInput : computeSlmValuesXe3pAndLaterTestsInput) {
        EXPECT_EQ(testInput.expected, EncodeDispatchKernel<FamilyType>::computeSlmValues(hwInfo, testInput.slmSize, releaseHelper, isHeapless));
    }

    EXPECT_THROW(EncodeDispatchKernel<FamilyType>::computeSlmValues(hwInfo, 384 * MemoryConstants::kiloByte + 1, releaseHelper, isHeapless), std::exception);
}
