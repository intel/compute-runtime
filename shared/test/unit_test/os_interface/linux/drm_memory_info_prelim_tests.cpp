/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/numa_library.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryInfoPrelim, givenMemoryRegionQueryNotSupportedWhenQueryingMemoryInfoThenMemoryInfoIsNotCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->memoryInfo.reset();
    drm->i915QuerySuccessCount = 0;
    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();

    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(MemoryInfoPrelim, givenMemoryRegionQueryWhenQueryingFailsThenMemoryInfoIsNotCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->memoryInfo.reset();
    drm->context.queryMemoryRegionInfoSuccessCount = 0;
    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->memoryInfo.reset();
    drm->i915QuerySuccessCount = 1;
    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->memoryInfo.reset();
    drm->context.queryMemoryRegionInfoSuccessCount = 1;
    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST(MemoryInfoPrelim, givenNewMemoryInfoQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto memoryInfo = drm->getMemoryInfo();

    ASSERT_NE(nullptr, memoryInfo);
    auto &multiTileArchInfo = defaultHwInfo->gtSystemInfo.MultiTileArchInfo;
    auto tileCount = multiTileArchInfo.IsValid ? multiTileArchInfo.TileCount : 0u;
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());
}

struct DrmVmTestFixture {
    void setUp() {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        testHwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        testHwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = true;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile0 =
            testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile1 =
                testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile2 =
                    testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile3 = 1;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.TileMask = 0b1111;
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        ASSERT_NE(nullptr, drm);
    }

    void tearDown() {}

    DebugManagerStateRestore restorer;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmQueryMock> drm;

    NEO::HardwareInfo *testHwInfo = nullptr;

    static constexpr uint8_t tileCount = 4u;
};

using DrmVmTestTest = Test<DrmVmTestFixture>;

TEST_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedWhenCreatingVirtualMemoryThenVmCreatedUsingNewRegion) {
    debugManager.flags.EnableLocalMemory.set(1);
    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    drm->queryEngineInfo();
    EXPECT_EQ(5u, drm->ioctlCallsCount);
    drm->ioctlCallsCount = 0u;

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());

    drm->ioctlCount.reset();
    bool ret = drm->createVirtualMemoryAddressSpace(tileCount);
    EXPECT_TRUE(ret);
    EXPECT_EQ(tileCount, drm->ioctlCount.gemVmCreate.load());
    EXPECT_NE(0ull, drm->receivedGemVmControl.extensions);
}

TEST_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedAndDebugKeyDisabledWhenCreatingVirtualMemoryThenVmCreatedNotUsingRegion) {
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0);

    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    drm->ioctlCallsCount = 0u;
    drm->queryEngineInfo();

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());

    drm->ioctlCount.reset();
    bool ret = drm->createVirtualMemoryAddressSpace(tileCount);
    EXPECT_TRUE(ret);
    EXPECT_EQ(tileCount, drm->ioctlCount.gemVmCreate.load());
    EXPECT_EQ(0ull, drm->receivedGemVmControl.extensions);
}

TEST_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedWhenCreatingVirtualMemoryFailsThenExpectDebugInformation) {
    NEO::debugManager.flags.PrintDebugMessages.set(1);
    NEO::debugManager.flags.EnableLocalMemory.set(1);
    drm->storedRetValForVmCreate = 1;

    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    drm->ioctlCallsCount = 0u;
    drm->queryEngineInfo();

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());

    drm->ioctlCount.reset();
    testing::internal::CaptureStderr();
    bool ret = drm->createVirtualMemoryAddressSpace(tileCount);
    EXPECT_FALSE(ret);
    EXPECT_EQ(1, drm->ioctlCount.gemVmCreate.load());
    EXPECT_NE(0ull, drm->receivedGemVmControl.extensions);

    std::string output = testing::internal::GetCapturedStderr();
    auto pos = output.find("INFO: Cannot create Virtual Memory at memory bank");
    EXPECT_NE(std::string::npos, pos);
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

        drm = std::make_unique<DrmQueryMock>(*rootDeviceEnvironment);

        memoryInfo = new MemoryInfo(regionInfo, *drm);
        drm->memoryInfo.reset(memoryInfo);
        drm->queryEngineInfo();
    }

    MemoryInfo *memoryInfo = nullptr;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmQueryMock> drm;
    const HardwareInfo *pHwInfo;
};

using MultiTileMemoryInfoPrelimTest = MultiTileMemoryInfoFixture;

TEST_F(MultiTileMemoryInfoPrelimTest, givenMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnCorrectValues) {
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

TEST_F(MultiTileMemoryInfoPrelimTest, givenDisabledLocalMemoryAndMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnSystemMemoryRegion) {
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

TEST_F(MultiTileMemoryInfoPrelimTest, givenMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceWhileDebugFlagIsActiveThenReturnCorrectValues) {
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
    // route to tile1 banks
    debugManager.flags.OverrideDrmRegion.set(1);

    // system memory not affected
    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank);
    EXPECT_EQ(8 * MemoryConstants::gigaByte, regionSize);

    // overrite route to tile 1
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *pHwInfo);
    EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(regionInfo[2].probedSize, regionSize);

    // route to tile 0 banks
    debugManager.flags.OverrideDrmRegion.set(0);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(regionInfo[1].probedSize, regionSize);

    // try to force tile 0 banks
    debugManager.flags.OverrideDrmRegion.set(-1);
    debugManager.flags.ForceMemoryBankIndexOverride.set(1);

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    if (gfxCoreHelper.isBankOverrideRequired(*pHwInfo, productHelper)) {
        EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    } else {
        EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    }

    // system memory not affected
    debugManager.flags.OverrideDrmRegion.set(-1);
    debugManager.flags.ForceMemoryBankIndexOverride.set(1);
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::mainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
}

TEST_F(MultiTileMemoryInfoPrelimTest, givenMemoryInfoWithoutRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnInvalidMemoryRegion) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;

    setupMemoryInfo(regionInfo, 0);

    EXPECT_ANY_THROW(memoryInfo->getMemoryRegionClassAndInstance(1, *pHwInfo));
    EXPECT_ANY_THROW(memoryInfo->getMemoryRegionSize(1));
}

TEST_F(MultiTileMemoryInfoPrelimTest, whenDebugVariablePrintMemoryRegionSizeIsSetAndGetMemoryRegionSizeIsCalledThenMessagePrintedToStdOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintMemoryRegionSizes.set(true);

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 16 * MemoryConstants::gigaByte;

    setupMemoryInfo(regionInfo, 0);

    StreamCapture capture;
    capture.captureStdout();
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::mainBank);
    EXPECT_EQ(16 * MemoryConstants::gigaByte, regionSize);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("Memory type: 0, memory instance: 1, region size: 17179869184\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemWithExtensionsThenReturnCorrectValues) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t numOfChunks = 0;
    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    ASSERT_TRUE(drm->context.receivedCreateGemExt);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithSingleRegionThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint32_t handle = 0;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    ASSERT_EQ(1u, createExt->memoryRegions.size());
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[0].memoryClass);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->context.setPairQueryValue = 1;
    drm->context.setPairQueryReturn = 1;

    uint32_t pairHandle = 0;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, pairHandle, 0, -1, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    uint32_t handle = 0;
    ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, pairHandle, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2u, drm->ioctlCallsCount);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

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
    ASSERT_TRUE(drm->context.receivedCreateGemExt);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
    EXPECT_EQ(std::nullopt, drm->context.receivedCreateGemExt->memPolicyExt.mode);
    EXPECT_EQ(std::nullopt, drm->context.receivedCreateGemExt->memPolicyExt.nodeMask);

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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

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
    ASSERT_TRUE(drm->context.receivedCreateGemExt);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
    EXPECT_EQ(0u, drm->context.receivedCreateGemExt->memPolicyExt.mode);
    EXPECT_EQ(4u, drm->context.receivedCreateGemExt->memPolicyExt.nodeMask.value().size());
    for (auto i = 0u; i < drm->context.receivedCreateGemExt->memPolicyExt.nodeMask.value().size(); i++) {
        EXPECT_EQ(i, drm->context.receivedCreateGemExt->memPolicyExt.nodeMask.value()[i]);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

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
    ASSERT_TRUE(drm->context.receivedCreateGemExt);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
    EXPECT_EQ(0u, drm->context.receivedCreateGemExt->memPolicyExt.mode);
    EXPECT_EQ(4u, drm->context.receivedCreateGemExt->memPolicyExt.nodeMask.value().size());
    for (auto i = 0u; i < drm->context.receivedCreateGemExt->memPolicyExt.nodeMask.value().size(); i++) {
        EXPECT_EQ(i, drm->context.receivedCreateGemExt->memPolicyExt.nodeMask.value()[i]);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

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
    ASSERT_TRUE(drm->context.receivedCreateGemExt);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
    EXPECT_EQ(std::nullopt, drm->context.receivedCreateGemExt->memPolicyExt.mode);
    EXPECT_EQ(std::nullopt, drm->context.receivedCreateGemExt->memPolicyExt.nodeMask);

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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    ASSERT_FALSE(memoryInfo->isMemPolicySupported());
}
TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithChunkingButSizeLessThanAllowedThenExceptionIsThrown) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->context.chunkingQueryValue = 1;
    drm->context.chunkingQueryReturn = 1;

    uint32_t numOfChunks = 2;
    size_t allocSize = MemoryConstants::chunkThreshold / (numOfChunks * 2);
    uint32_t pairHandle = -1;
    uint32_t handle = 0;
    bool isChunked = true;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_THROW(memoryInfo->createGemExtWithMultipleRegions(1, allocSize, handle, 0, pairHandle, isChunked, numOfChunks, false), std::runtime_error);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->context.chunkingQueryValue = 1;
    drm->context.chunkingQueryReturn = 1;

    uint32_t numOfChunks = 2;
    size_t allocSize = MemoryConstants::chunkThreshold * numOfChunks * 2;
    uint32_t pairHandle = -1;
    uint32_t handle = 0;
    bool isChunked = true;
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    auto ret = memoryInfo->createGemExtWithMultipleRegions(1, allocSize, handle, 0, pairHandle, isChunked, numOfChunks, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(false);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    auto validVmId = drm->getVirtualMemoryAddressSpace(0);
    EXPECT_EQ(validVmId, createExt->vmPrivateExt.vmId);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(false);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    EXPECT_EQ(std::nullopt, createExt->vmPrivateExt.vmId);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(true);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle, 0, -1, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    EXPECT_EQ(std::nullopt, createExt->vmPrivateExt.vmId);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    uint32_t handle = 0;
    uint32_t memoryRegions = 0b1011;
    auto ret = memoryInfo->createGemExtWithMultipleRegions(memoryRegions, 1024, handle, 0, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    ASSERT_EQ(3u, createExt->memoryRegions.size());
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[0].memoryClass);
    EXPECT_EQ(0u, createExt->memoryRegions[0].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[1].memoryClass);
    EXPECT_EQ(1u, createExt->memoryRegions[1].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[2].memoryClass);
    EXPECT_EQ(3u, createExt->memoryRegions[2].memoryInstance);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCallingCreatingGemExtWithMultipleRegionsAndNotAllowedSizeThenExceptionIsThrown) {
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    uint32_t handle = 0;
    uint32_t memoryRegions = 0b1011;
    uint32_t numOfChunks = 2;
    EXPECT_THROW(memoryInfo->createGemExtWithMultipleRegions(memoryRegions, MemoryConstants::chunkThreshold / (numOfChunks * 2), handle, 0, -1, true, numOfChunks, false), std::runtime_error);
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
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    uint32_t handle = 0;
    uint32_t memoryRegions = 0b1011;
    uint32_t numOfChunks = 2;
    size_t size = MemoryConstants::chunkThreshold * numOfChunks;
    auto ret = memoryInfo->createGemExtWithMultipleRegions(memoryRegions, size, handle, 0, -1, true, numOfChunks, false);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    ASSERT_EQ(3u, createExt->memoryRegions.size());
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[0].memoryClass);
    EXPECT_EQ(0u, createExt->memoryRegions[0].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[1].memoryClass);
    EXPECT_EQ(1u, createExt->memoryRegions[1].memoryInstance);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[2].memoryClass);
    EXPECT_EQ(3u, createExt->memoryRegions[2].memoryInstance);
    EXPECT_EQ(size, drm->context.receivedCreateGemExt->size);
}
