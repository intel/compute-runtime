/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock_dg1.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryInfo, givenMemoryRegionQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto memoryInfo = static_cast<MemoryInfoImpl *>(drm->getMemoryInfo());

    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(2u, memoryInfo->getDrmRegionInfos().size());
}

TEST(MemoryInfo, givenMemoryRegionQueryNotSupportedWhenQueryingMemoryInfoThenMemoryInfoIsNotCreated) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->i915QuerySuccessCount = 0;
    drm->queryMemoryInfo();

    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryRegionQueryWhenQueryingFailsThenMemoryInfoIsNotCreated) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryRegionInfoSuccessCount = 0;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmMockDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->i915QuerySuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmMockDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->queryMemoryRegionInfoSuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryEnabledWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 2);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[0].region.memory_instance, regionClassAndInstance.memory_instance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[1].region.memory_instance, regionClassAndInstance.memory_instance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryEnabledWhenAssignRegionsFromDistancesThenRegionsNotChanged) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 2);
    ASSERT_NE(nullptr, memoryInfo);
    memoryInfo->assignRegionsFromDistances(&regionInfo, 2);
    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[0].region.memory_instance, regionClassAndInstance.memory_instance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[1].region.memory_instance, regionClassAndInstance.memory_instance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithoutDeviceRegionWhenGettingDeviceRegionSizeThenReturnCorrectSize) {
    drm_i915_memory_region_info regionInfo[1] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 1);
    ASSERT_NE(nullptr, memoryInfo);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(0 * GB, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryDisabledWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(0);
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 2);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[0].region.memory_instance, regionClassAndInstance.memory_instance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[0].region.memory_instance, regionClassAndInstance.memory_instance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);
}

TEST(MemoryInfo, whenDebugVariablePrintMemoryRegionSizeIsSetAndGetMemoryRegionSizeIsCalledThenMessagePrintedToStdOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintMemoryRegionSizes.set(true);

    drm_i915_memory_region_info regionInfo[1] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probed_size = 16 * GB;

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 1);
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
    drm_i915_memory_region_info regionInfo[3] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;
    regionInfo[2].region = {I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probed_size = 32 * GB;

    auto memoryInfo = std::make_unique<MemoryInfoImpl>(regionInfo, 3);
    ASSERT_NE(nullptr, memoryInfo);

    DebugManager.flags.OverrideDrmRegion.set(1);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[2].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[2].region.memory_instance, regionClassAndInstance.memory_instance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);

    DebugManager.flags.OverrideDrmRegion.set(0);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memory_class, regionClassAndInstance.memory_class);
    EXPECT_EQ(regionInfo[1].region.memory_instance, regionClassAndInstance.memory_instance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * GB, regionSize);

    DebugManager.flags.OverrideDrmRegion.set(-1);
    DebugManager.flags.ForceMemoryBankIndexOverride.set(1);

    auto &helper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *defaultHwInfo);
    if (helper.isBankOverrideRequired(*defaultHwInfo)) {
        EXPECT_EQ(regionInfo[1].region.memory_class, regionClassAndInstance.memory_class);
        EXPECT_EQ(regionInfo[1].region.memory_instance, regionClassAndInstance.memory_instance);
    } else {
        EXPECT_EQ(regionInfo[2].region.memory_class, regionClassAndInstance.memory_class);
        EXPECT_EQ(regionInfo[2].region.memory_instance, regionClassAndInstance.memory_instance);
    }
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * GB, regionSize);
}
