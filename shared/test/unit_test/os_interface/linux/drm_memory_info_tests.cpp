/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_impl.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryInfo, givenMemoryRegionQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryInfo();

    EXPECT_EQ(2u, drm->ioctlCallsCount);
    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(2u, memoryInfo->getDrmRegionInfos().size());
}

TEST(MemoryInfo, givenMemoryRegionQueryNotSupportedWhenQueryingMemoryInfoThenMemoryInfoIsNotCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->i915QuerySuccessCount = 0;
    drm->queryMemoryInfo();

    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryRegionQueryWhenQueryingFailsThenMemoryInfoIsNotCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryRegionInfoSuccessCount = 0;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->i915QuerySuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->queryMemoryRegionInfoSuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryEnabledWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithoutDeviceRegionWhenGettingDeviceRegionSizeThenReturnCorrectSize) {
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(0 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryDisabledWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(0);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);
}

TEST(MemoryInfo, whenDebugVariablePrintMemoryRegionSizeIsSetAndGetMemoryRegionSizeIsCalledThenMessagePrintedToStdOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintMemoryRegionSizes.set(true);

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);

    testing::internal::CaptureStdout();
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(16 * GB, regionSize);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Memory type: 0, memory instance: 1, region size: 17179869184\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceWhileDebugFlagIsActiveThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;
    regionInfo[2].region = {I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * GB;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);

    DebugManager.flags.OverrideDrmRegion.set(1);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);

    DebugManager.flags.OverrideDrmRegion.set(0);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * GB, regionSize);

    DebugManager.flags.OverrideDrmRegion.set(-1);
    DebugManager.flags.ForceMemoryBankIndexOverride.set(1);

    auto &helper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *defaultHwInfo);
    if (helper.isBankOverrideRequired(*defaultHwInfo)) {
        EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
        EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    } else {
        EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
        EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    }
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * GB, regionSize);
}

using MemoryInfoTest = ::testing::Test;

HWTEST2_F(MemoryInfoTest, givenMemoryInfoWithRegionsWhenCreatingGemWithExtensionsThenReturnCorrectValues, NonDefaultIoctlsSupported) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    auto ret = memoryInfo->createGemExt(drm.get(), memClassInstance, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_EQ(1024u, drm->createExt.size);
}

HWTEST2_F(MemoryInfoTest, givenMemoryInfoWithRegionsWhenCreatingGemExtWithSingleRegionThenReturnCorrectValues, NonDefaultIoctlsSupported) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo);
    ASSERT_NE(nullptr, memoryInfo);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(drm.get(), 1, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_EQ(I915_MEMORY_CLASS_DEVICE, drm->memRegions.memory_class);
    EXPECT_EQ(1024u, drm->createExt.size);
}
