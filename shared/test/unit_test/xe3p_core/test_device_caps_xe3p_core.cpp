/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;

using Xe3pCoreDeviceCaps = Test<DeviceFixture>;

XE3P_CORETEST_F(Xe3pCoreDeviceCaps, givenXe3pCoreWhenCheckingMediaBlockSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE3P_CORETEST_F(Xe3pCoreDeviceCaps, givenXe3pCoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

XE3P_CORETEST_F(Xe3pCoreDeviceCaps, givenXe3pCoreWhenCheckingDefaultPreemptionModeThenDefaultPreemptionModeIsMidThread) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

XE3P_CORETEST_F(Xe3pCoreDeviceCaps, givenDeviceWhenAskingForSubGroupSizesThenReturnCorrectValues) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const auto deviceSubgroups = gfxCoreHelper.getDeviceSubGroupSizes();

    EXPECT_EQ(2u, deviceSubgroups.size());
    EXPECT_EQ(16u, deviceSubgroups[0]);
    EXPECT_EQ(32u, deviceSubgroups[1]);
}

XE3P_CORETEST_F(Xe3pCoreDeviceCaps, givenSlmSizeWhenEncodingThenReturnCorrectValues) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;

    struct ComputeSlmTestInput {
        uint32_t expected;
        uint32_t slmSize;
    };

    const auto &hwInfo = pDevice->getHardwareInfo();
    auto releaseHelper = pDevice->getReleaseHelper();
    bool isHeapless = false;

    ComputeSlmTestInput computeSlmValuesXe3pAndLaterTestsInput[] = {
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K), 0 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K), 0 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_1K), 1 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K), 1 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_2K), 2 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K), 2 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_4K), 4 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K), 4 * MemoryConstants::kiloByte + 1},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_8K), 8 * MemoryConstants::kiloByte},
        {static_cast<uint32_t>(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_16K), 8 * MemoryConstants::kiloByte + 1},
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

using Xe3pDeviceTests = ::testing::Test;
XE3P_CORETEST_F(Xe3pDeviceTests, given3CopyEnginesWhenSecondaryContextsCretaedThanRegularEngineCountIsLimitedToHalfOfContextGroupSize) {
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ContextGroupSize.set(8);

    hwInfo.capabilityTable.blitterOperationsSupported = true;
    // BCS1, BCS2 and BCS3 available
    hwInfo.featureTable.ftrBcsInfo = 0b1110;

    auto executionEnvironment = NEO::MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->incRefInternal();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0));

    auto memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());

    EXPECT_EQ(4u, device->secondaryEngines[aub_stream::EngineType::ENGINE_BCS1].engines.size());
    EXPECT_EQ(4u, device->secondaryEngines[aub_stream::EngineType::ENGINE_BCS2].engines.size());
    EXPECT_EQ(8u, device->secondaryEngines[aub_stream::EngineType::ENGINE_BCS3].engines.size());

    device.reset(nullptr);

    EXPECT_EQ(0u, memoryManager->secondaryEngines[0].size());
    EXPECT_EQ(0u, memoryManager->allRegisteredEngines[0].size());

    EXPECT_GT(memoryManager->maxOsContextCount, memoryManager->latestContextId);

    executionEnvironment->decRefInternal();
}
