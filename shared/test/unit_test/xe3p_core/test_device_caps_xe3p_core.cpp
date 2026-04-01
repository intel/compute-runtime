/*
 * Copyright (C) 2025-2026 Intel Corporation
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
