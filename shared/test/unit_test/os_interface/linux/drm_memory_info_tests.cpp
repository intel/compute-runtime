/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/numa_library.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_library.h"
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

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    EXPECT_EQ(regionInfo[0].probedSize, memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank));

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *defaultHwInfo);
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

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *defaultHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
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

struct MultiTileMemoryInfoFixture : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        pHwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    }

    template <typename ArrayT>
    void setupMemoryInfo(ArrayT &regionInfo, uint32_t tileCount) {
        auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();

        GT_MULTI_TILE_ARCH_INFO &multiTileArch = rootDeviceEnvironment->getMutableHardwareInfo()->gtSystemInfo.MultiTileArchInfo;
        multiTileArch.IsValid = (tileCount > 0);
        multiTileArch.TileCount = tileCount;
        multiTileArch.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

        drm = std::make_unique<DrmMockWithCaptureHelper>(*rootDeviceEnvironment);

        memoryInfo = new MemoryInfo(regionInfo, *drm);
        drm->memoryInfo.reset(memoryInfo);
    }

    MemoryInfo *memoryInfo = nullptr;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmMockWithCaptureHelper> drm;
    const HardwareInfo *pHwInfo;
};

using MultiTileMemoryInfoTest = MultiTileMemoryInfoFixture;

TEST_F(MultiTileMemoryInfoTest, givenMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    setupMemoryInfo(regionInfo, 2);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank);
    EXPECT_EQ(8 * MemoryConstants::gigaByte, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *pHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * MemoryConstants::gigaByte, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * MemoryConstants::gigaByte, regionSize);
}

TEST_F(MultiTileMemoryInfoTest, givenDisabledLocalMemoryAndMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnSystemMemoryRegion) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    setupMemoryInfo(regionInfo, 2);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithPairHandleThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t pairHandle = 0;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, pairHandle, 0, -1, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());

    uint32_t handle = 0;
    ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, pairHandle, false);
    EXPECT_EQ(0, ret);
    ASSERT_EQ(2u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    EXPECT_EQ(-1, drm->getMockIoctlHelper()->createGemExtCalls[0].pairHandle);
    EXPECT_EQ(static_cast<int32_t>(pairHandle), drm->getMockIoctlHelper()->createGemExtCalls[1].pairHandle);
}

struct WhiteBoxNumaLibrary : Linux::NumaLibrary {
    using Linux::NumaLibrary::numaLibNameStr;
    using Linux::NumaLibrary::procGetMemPolicyStr;
    using Linux::NumaLibrary::procNumaAvailableStr;
    using Linux::NumaLibrary::procNumaMaxNodeStr;
    using GetMemPolicyPtr = NumaLibrary::GetMemPolicyPtr;
    using NumaAvailablePtr = NumaLibrary::NumaAvailablePtr;
    using NumaMaxNodePtr = NumaLibrary::NumaMaxNodePtr;
    using Linux::NumaLibrary::getMemPolicyFunction;
    using Linux::NumaLibrary::osLibrary;
};

TEST(MemoryInfo, givenValidNumaLibraryPtrAndMemoryInfoWithoutMemoryPolicyEnabledWhenMemoryInfoIsCreatedThenNumaLibraryIsNotLoaded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(0);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(-1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    // setup numa library in MemoryInfo
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *, unsigned long[], unsigned long, void *, unsigned long) -> long { return 0; };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return 4; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    // register proc pointers
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_FALSE(memoryInfo->isMemPolicySupported());

    delete osLibrary;
    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(MemoryInfo, givenMemoryInfoWithMemoryPolicyEnabledWhenCallingCreateGemExtWithNonHostAllocationThenIoctlIsReturnedWithoutMemPolicy) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(1);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(-1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    // setup numa library in MemoryInfo
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *, unsigned long[], unsigned long, void *, unsigned long) -> long { return 0; };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return 4; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    // register proc pointers
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_TRUE(memoryInfo->isMemPolicySupported());

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    uint32_t numOfChunks = 0;
    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    EXPECT_EQ(1024u, drm->getMockIoctlHelper()->createGemExtCalls.back().allocSize);
    EXPECT_EQ(std::nullopt, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyMode);
    EXPECT_EQ(std::nullopt, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask);

    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(MemoryInfo, givenMemoryInfoWithMemoryPolicyEnabledWhenCallingCreateGemExtForHostAllocationThenIoctlIsCalledWithMemoryPolicy) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(1);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(-1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    constexpr static int numNuma = 4;
    // setup numa library in MemoryInfo
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *mode, unsigned long nodeMask[], unsigned long, void *, unsigned long) -> long {
        if (mode) {
            *mode = 0;
        }
        for (int i = 0; i < numNuma; i++) {
            nodeMask[i] = i;
        }
        return 0;
    };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return numNuma - 1; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    // register proc pointers
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);

    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_TRUE(memoryInfo->isMemPolicySupported());

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    uint32_t numOfChunks = 0;
    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, true);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    EXPECT_EQ(1024u, drm->getMockIoctlHelper()->createGemExtCalls.back().allocSize);
    EXPECT_EQ(0u, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyMode);
    EXPECT_EQ(4u, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask.value().size());
    for (auto i = 0u; i < drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask.value().size(); i++) {
        EXPECT_EQ(i, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask.value()[i]);
    }
    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(MemoryInfo, givenMemoryInfoWithMemoryPolicyEnabledAndOverrideMemoryPolicyModeWhenCallingCreateGemExtForHostAllocationThenIoctlIsCalledWithMemoryPolicy) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(1);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(0);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    constexpr static int numNuma = 4;
    // setup numa library in MemoryInfo
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *mode, unsigned long nodeMask[], unsigned long, void *, unsigned long) -> long {
        if (mode) {
            *mode = 3;
        }
        for (int i = 0; i < numNuma; i++) {
            nodeMask[i] = i;
        }
        return 0;
    };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return numNuma - 1; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    // register proc pointers
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);

    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_TRUE(memoryInfo->isMemPolicySupported());

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    uint32_t numOfChunks = 0;
    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, true);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    EXPECT_EQ(1024u, drm->getMockIoctlHelper()->createGemExtCalls.back().allocSize);
    EXPECT_EQ(0u, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyMode);
    EXPECT_EQ(4u, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask.value().size());
    for (auto i = 0u; i < drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask.value().size(); i++) {
        EXPECT_EQ(i, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask.value()[i]);
    }

    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(MemoryInfo, givenMemoryInfoWithMemoryPolicyEnabledWhenCallingCreateGemExtWithIncorrectGetMemPolicyHandlerThenIoctlIsReturnedWithoutMemPolicy) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(1);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(-1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    // setup numa library in MemoryInfo
    WhiteBoxNumaLibrary::GetMemPolicyPtr memPolicyHandler =
        [](int *, unsigned long[], unsigned long, void *, unsigned long) -> long { return -1; };
    WhiteBoxNumaLibrary::NumaAvailablePtr numaAvailableHandler =
        [](void) -> int { return 0; };
    WhiteBoxNumaLibrary::NumaMaxNodePtr numaMaxNodeHandler =
        [](void) -> int { return 4; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    MockOsLibraryCustom *osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    // register proc pointers
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procGetMemPolicyStr)] = reinterpret_cast<void *>(memPolicyHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaAvailableStr)] = reinterpret_cast<void *>(numaAvailableHandler);
    osLibrary->procMap[std::string(WhiteBoxNumaLibrary::procNumaMaxNodeStr)] = reinterpret_cast<void *>(numaMaxNodeHandler);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_TRUE(memoryInfo->isMemPolicySupported());

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    uint32_t numOfChunks = 0;
    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, true);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    EXPECT_EQ(1024u, drm->getMockIoctlHelper()->createGemExtCalls.back().allocSize);
    EXPECT_EQ(std::nullopt, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyMode);
    EXPECT_EQ(std::nullopt, drm->getMockIoctlHelper()->createGemExtCalls.back().memPolicyNodemask);

    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(MemoryInfo, givenMemoryInfoWithMemoryPolicyEnabledAndInvalidOsLibraryWhenCallingInitializingNumaLibraryThenMemPolicyIsNotSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(1);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(-1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    MockOsLibrary::loadLibraryNewObject = nullptr;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_FALSE(memoryInfo->isMemPolicySupported());

    MockOsLibrary::loadLibraryNewObject = nullptr;
    WhiteBoxNumaLibrary::osLibrary.reset();
}

TEST(MemoryInfo, givenMemoryInfoWithMemoryPolicyDisabledAndValidOsLibraryWhenCallingInitializingNumaLibraryThenMemPolicyIsNotSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHostAllocationMemPolicy.set(0);
    debugManager.flags.OverrideHostAllocationMemPolicyMode.set(-1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 32 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_FALSE(memoryInfo->isMemPolicySupported());
}
TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithChunkingWithSizeGreaterThanAllowedThenAllocationIsCreatedWithChunking) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t numOfChunks = 2;
    size_t allocSize = MemoryConstants::chunkThreshold * numOfChunks * 2;
    uint32_t pairHandle = -1;
    uint32_t handle = 0;
    bool isChunked = true;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    auto ret = memoryInfo->createGemExtWithMultipleRegions(1, allocSize, handle, 0, pairHandle, isChunked, numOfChunks, false);
    EXPECT_EQ(0, ret);
    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    EXPECT_TRUE(drm->getMockIoctlHelper()->createGemExtCalls[0].isChunked);
    EXPECT_EQ(numOfChunks, drm->getMockIoctlHelper()->createGemExtCalls[0].numOfChunks);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndPrivateBOSupportWhenCreatingGemExtWithSingleRegionThenValidVmIdIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.EnablePrivateBO.set(true);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(false);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());

    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    const auto &createExt = drm->getMockIoctlHelper()->createGemExtCalls.back();
    auto validVmId = drm->getVirtualMemoryAddressSpace(0);
    EXPECT_EQ(validVmId, createExt.vmId);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndNoPrivateBOSupportWhenCreatingGemExtWithSingleRegionThenVmIdIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.EnablePrivateBO.set(false);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(false);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());

    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    const auto &createExt = drm->getMockIoctlHelper()->createGemExtCalls.back();
    EXPECT_EQ(std::nullopt, createExt.vmId);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndPrivateBOSupportedAndIsPerContextVMRequiredIsTrueWhenCreatingGemExtWithSingleRegionThenVmIdIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.EnablePrivateBO.set(true);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(true);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());

    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    const auto &createExt = drm->getMockIoctlHelper()->createGemExtCalls.back();
    EXPECT_EQ(std::nullopt, createExt.vmId);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithMultipleRegionsThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2};
    regionInfo[3].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 3};
    regionInfo[4].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    uint32_t handle = 0;
    uint32_t memoryRegions = 0b1011;
    auto ret = memoryInfo->createGemExtWithMultipleRegions(memoryRegions, 1024, handle, 0, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());

    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    const auto &createExt = drm->getMockIoctlHelper()->createGemExtCalls.back();
    ASSERT_EQ(3u, createExt.memClassInstances.size());
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt.memClassInstances[0].memoryClass);
    EXPECT_EQ(0u, createExt.memClassInstances[0].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt.memClassInstances[1].memoryClass);
    EXPECT_EQ(1u, createExt.memClassInstances[1].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt.memClassInstances[2].memoryClass);
    EXPECT_EQ(3u, createExt.memClassInstances[2].memoryInstance);
    EXPECT_EQ(1024u, drm->getMockIoctlHelper()->createGemExtCalls.back().allocSize);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCallingCreatingGemExtWithMultipleRegionsAndChunkingThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2};
    regionInfo[3].probedSize = 16 * MemoryConstants::gigaByte;
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 3};
    regionInfo[4].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    uint32_t handle = 0;
    uint32_t memoryRegions = 0b1011;
    uint32_t numOfChunks = 2;
    size_t size = MemoryConstants::chunkThreshold * numOfChunks;
    auto ret = memoryInfo->createGemExtWithMultipleRegions(memoryRegions, size, handle, 0, -1, true, numOfChunks, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());

    ASSERT_EQ(1u, drm->getMockIoctlHelper()->createGemExtCalls.size());
    const auto &createExt = drm->getMockIoctlHelper()->createGemExtCalls.back();
    ASSERT_EQ(3u, createExt.memClassInstances.size());
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt.memClassInstances[0].memoryClass);
    EXPECT_EQ(0u, createExt.memClassInstances[0].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt.memClassInstances[1].memoryClass);
    EXPECT_EQ(1u, createExt.memClassInstances[1].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt.memClassInstances[2].memoryClass);
    EXPECT_EQ(3u, createExt.memClassInstances[2].memoryInstance);
    EXPECT_EQ(size, drm->getMockIoctlHelper()->createGemExtCalls.back().allocSize);
}
