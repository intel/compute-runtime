/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryEnabledWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank);
    EXPECT_EQ(8 * MemoryConstants::gigaByte, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * MemoryConstants::gigaByte, regionSize);
}

TEST(MemoryInfo, givenMemoryInfoWithoutDeviceRegionWhenGettingDeviceRegionSizeThenReturnCorrectSize) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_ANY_THROW(memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo));
    EXPECT_ANY_THROW(memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0)));
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndLocalMemoryDisabledWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank);
    EXPECT_EQ(8 * MemoryConstants::gigaByte, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * MemoryConstants::gigaByte, regionSize);
}

TEST(MemoryInfo, whenDebugVariablePrintMemoryRegionSizeIsSetAndGetMemoryRegionSizeIsCalledThenMessagePrintedToStdOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintMemoryRegionSizes.set(true);

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    StreamCapture capture;
    capture.captureStdout();
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank);
    EXPECT_EQ(16 * MemoryConstants::gigaByte, regionSize);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("Memory type: 0, memory instance: 1, region size: 17179869184\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceWhileDebugFlagIsActiveThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    debugManager.flags.OverrideDrmRegion.set(1);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
    EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(regionInfo[2].probedSize, regionSize);

    debugManager.flags.OverrideDrmRegion.set(0);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *defaultHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(regionInfo[1].probedSize, regionSize);

    debugManager.flags.OverrideDrmRegion.set(-1);
    debugManager.flags.ForceMemoryBankIndexOverride.set(1);

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *defaultHwInfo);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    if (gfxCoreHelper.isBankOverrideRequired(*defaultHwInfo, productHelper)) {
        EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
        EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
        EXPECT_EQ(regionInfo[1].probedSize, regionSize);
    } else {
        EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
        EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
        EXPECT_EQ(regionInfo[2].probedSize, regionSize);
    }
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemWithExtensionsThenRequestIsForwardedToIoctlHelper) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = new MockIoctlHelperWithCapture(*drm);
    drm->ioctlHelper.reset(ioctlHelper);
    ioctlHelper->createGemExtHandle = 5u;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem)), 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice)), 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    uint32_t numOfChunks = 0;
    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(5u, handle);
    ASSERT_EQ(1u, ioctlHelper->createGemExtCalls.size());
    EXPECT_EQ(1024u, ioctlHelper->createGemExtCalls[0].allocSize);
    ASSERT_EQ(2u, ioctlHelper->createGemExtCalls[0].memClassInstances.size());
    EXPECT_EQ(regionInfo[0].region.memoryClass, ioctlHelper->createGemExtCalls[0].memClassInstances[0].memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryClass, ioctlHelper->createGemExtCalls[0].memClassInstances[1].memoryClass);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithSingleRegionThenDeviceRegionIsRequestedFromIoctlHelper) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = new MockIoctlHelperWithCapture(*drm);
    drm->ioctlHelper.reset(ioctlHelper);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem)), 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice)), 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, handle);
    ASSERT_EQ(1u, ioctlHelper->createGemExtCalls.size());
    EXPECT_EQ(1024u, ioctlHelper->createGemExtCalls[0].allocSize);
    ASSERT_EQ(1u, ioctlHelper->createGemExtCalls[0].memClassInstances.size());
    EXPECT_EQ(regionInfo[1].region.memoryClass, ioctlHelper->createGemExtCalls[0].memClassInstances[0].memoryClass);
}

TEST(MemoryInfo, givenIoctlHelperReturningMemoryRegionsWhenQueryingMemoryInfoThenDrmStoresReturnedMemoryInfo) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = new MockIoctlHelperWithCapture(*drm);
    drm->ioctlHelper.reset(ioctlHelper);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem)), 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice)), 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    ioctlHelper->memoryRegionsToReturn = regionInfo;

    drm->memoryInfoQueried = false;
    EXPECT_TRUE(drm->queryMemoryInfo());

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(2u, memoryInfo->getDrmRegionInfos().size());
}

TEST(MemoryInfo, givenIoctlHelperReturningNoMemoryInfoWhenQueryingMemoryInfoThenMemoryInfoIsNotSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);

    drm->memoryInfoQueried = false;
    EXPECT_FALSE(drm->queryMemoryInfo());
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
}
