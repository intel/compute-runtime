/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmVirtualMemoryTest, givenDisableScratchPagesWhenCreateDrmVirtualMemoryThenProperFlagIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableScratchPages.set(1);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags->disableScratch);
}

TEST(DrmVirtualMemoryTest, givenDebuggingEnabledWithoutDisableScratchPagesFlagSetWhenCreateDrmVirtualMemoryThenDisableScratchPagesFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(productHelper.release());
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    EXPECT_TRUE(drm.disableScratch);
    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags->disableScratch);
}

TEST(DrmVirtualMemoryTest, givenDebuggingEnabledWithDisableScratchPagesFlagSetWhenCreateDrmVirtualMemoryThenDisableScratchPagesFlagIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isDisableScratchPagesRequiredForDebuggerResult = false;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    executionEnvironment->rootDeviceEnvironments[0]->productHelper.reset(productHelper.release());
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    EXPECT_FALSE(drm.disableScratch);
    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);
    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
    EXPECT_FALSE(drm.getMockIoctlHelper()->receivedVmCreateFlags->disableScratch);
}

TEST(DrmVirtualMemoryTest, givenDisableScratchPagesDebugKeyOffAndDebuggingEnabledWhenCreateDrmVirtualMemoryThenEnvVariableIsPriority) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableScratchPages.set(0);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
    EXPECT_FALSE(drm.getMockIoctlHelper()->receivedVmCreateFlags->disableScratch);
}

TEST(DrmVirtualMemoryTest, givenDisableScratchPagesAndDebuggingEnabledWhenCreateDrmVirtualMemoryThenEnvVariableIsPriority) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableScratchPages.set(1);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags->disableScratch);
}

TEST(DrmVirtualMemoryTest, givenLocalMemoryDisabledWhenCreateDrmVirtualMemoryThenVmRegionExtensionIsNotPassed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(1u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockWithCaptureHelper drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.memoryInfo = std::make_unique<MockMemoryInfo>(drm);

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_EQ(1u, drm.getMockIoctlHelper()->createVmControlExtRegionCalled);
    EXPECT_FALSE(drm.getMockIoctlHelper()->receivedVmControlRegion);
}

TEST(DrmVirtualMemoryTest, givenLocalMemoryEnabledWhenCreateDrmVirtualMemoryThenVmRegionExtensionIsPassed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(1u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    HardwareInfo testHwInfo = *defaultHwInfo;

    testHwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
    testHwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    testHwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = 1;
    testHwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 0b1;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&testHwInfo);
    DrmMockWithCaptureHelper drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.memoryInfo = std::make_unique<MockMemoryInfo>(drm);
    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    EXPECT_EQ(1u, drm.getMockIoctlHelper()->createVmControlExtRegionCalled);
    EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmControlRegion);
}

TEST(DrmVirtualMemoryTest, givenPageFaultNotSupportedWhenCallingCreateDrmVirtualMemoryThenDontEnablePageFaultsOnVirtualMemory) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};
    EXPECT_FALSE(drm.pageFaultSupported);

    uint32_t vmId = 0;
    drm.createDrmVirtualMemory(vmId);

    ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
    EXPECT_FALSE(drm.getMockIoctlHelper()->receivedVmCreateFlags->enablePageFault);

    drm.destroyDrmVirtualMemory(vmId);
}

TEST(DrmVirtualMemoryTest, givenPageFaultSupportedWhenVmBindIsAvailableThenEnablePageFaultsOnVirtualMemory) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockWithCaptureHelper drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.pageFaultSupported = true;

    for (auto vmBindAvailable : {false, true}) {
        drm.bindAvailable = vmBindAvailable;

        uint32_t vmId = 0;
        drm.ioctlCount.gemVmCreate = 0;
        drm.createDrmVirtualMemory(vmId);
        EXPECT_EQ(1, drm.ioctlCount.gemVmCreate.load());

        if (drm.isVmBindAvailable()) {
            EXPECT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags->enablePageFault);
        } else {
            ASSERT_TRUE(drm.getMockIoctlHelper()->receivedVmCreateFlags);
            EXPECT_FALSE(drm.getMockIoctlHelper()->receivedVmCreateFlags->enablePageFault);
        }

        drm.ioctlCount.gemVmDestroy = 0;
        drm.destroyDrmVirtualMemory(vmId);
        EXPECT_EQ(1, drm.ioctlCount.gemVmDestroy.load());
    }
}
