/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_banks.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryInfoPrelim, givenMemoryRegionQueryNotSupportedWhenQueryingMemoryInfoThenMemoryInfoIsNotCreated) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->memoryInfo.reset();
    drm->i915QuerySuccessCount = 0;
    drm->queryMemoryInfo();

    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(MemoryInfoPrelim, givenMemoryRegionQueryWhenQueryingFailsThenMemoryInfoIsNotCreated) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->memoryInfo.reset();
    drm->context.queryMemoryRegionInfoSuccessCount = 0;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->memoryInfo.reset();
    drm->i915QuerySuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->memoryInfo.reset();
    drm->context.queryMemoryRegionInfoSuccessCount = 1;
    drm->queryMemoryInfo();
    EXPECT_EQ(nullptr, drm->getMemoryInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST(MemoryInfoPrelim, givenNewMemoryInfoQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto memoryInfo = drm->getMemoryInfo();

    ASSERT_NE(nullptr, memoryInfo);
    auto &multiTileArchInfo = defaultHwInfo->gtSystemInfo.MultiTileArchInfo;
    auto tileCount = multiTileArchInfo.IsValid ? multiTileArchInfo.TileCount : 0u;
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());
}

struct DrmVmTestFixture {
    void SetUp() { // NOLINT(readability-identifier-naming)
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
        testHwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        testHwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = true;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile0 =
            testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile1 =
                testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile2 =
                    testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile3 = 1;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.TileMask = 0b1111;
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0], testHwInfo);
        ASSERT_NE(nullptr, drm);
    }

    void TearDown() {} // NOLINT(readability-identifier-naming)

    DebugManagerStateRestore restorer;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmQueryMock> drm;

    NEO::HardwareInfo *testHwInfo = nullptr;

    static constexpr uint8_t tileCount = 4u;
};

using DrmVmTestTest = Test<DrmVmTestFixture>;

TEST_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedWhenCreatingVirtualMemoryThenVmCreatedUsingNewRegion) {
    DebugManager.flags.EnableLocalMemory.set(1);
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
    DebugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0);

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
    NEO::DebugManager.flags.PrintDebugMessages.set(1);
    NEO::DebugManager.flags.EnableLocalMemory.set(1);
    drm->storedRetValForVmCreate = 1;

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
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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

        drm = std::make_unique<DrmQueryMock>(*rootDeviceEnvironment, rootDeviceEnvironment->getHardwareInfo());

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
    DebugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[1].probedSize = 16 * GB;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[2].probedSize = 32 * GB;

    setupMemoryInfo(regionInfo, 2);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *pHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * GB, regionSize);
}

TEST_F(MultiTileMemoryInfoPrelimTest, givenDisabledLocalMemoryAndMemoryInfoWithRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnSystemMemoryRegion) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(0);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[1].probedSize = 16 * GB;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[2].probedSize = 32 * GB;

    setupMemoryInfo(regionInfo, 2);

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *pHwInfo);
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
    DebugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[1].probedSize = 16 * GB;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[2].probedSize = 32 * GB;

    setupMemoryInfo(regionInfo, 2);
    //route to tile1 banks
    DebugManager.flags.OverrideDrmRegion.set(1);

    //system memory not affected
    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(8 * GB, regionSize);

    //overrite route to tile 1
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(0), *pHwInfo);
    EXPECT_EQ(regionInfo[2].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0));
    EXPECT_EQ(16 * GB, regionSize);

    //route to tile 0 banks
    DebugManager.flags.OverrideDrmRegion.set(0);

    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    EXPECT_EQ(regionInfo[1].region.memoryClass, regionClassAndInstance.memoryClass);
    EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(1));
    EXPECT_EQ(32 * GB, regionSize);

    // try to force tile 0 banks
    DebugManager.flags.OverrideDrmRegion.set(-1);
    DebugManager.flags.ForceMemoryBankIndexOverride.set(1);

    auto &helper = HwHelper::get(pHwInfo->platform.eRenderCoreFamily);
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::getBankForLocalMemory(1), *pHwInfo);
    if (helper.isBankOverrideRequired(*pHwInfo)) {
        EXPECT_EQ(regionInfo[1].region.memoryInstance, regionClassAndInstance.memoryInstance);
    } else {
        EXPECT_EQ(regionInfo[2].region.memoryInstance, regionClassAndInstance.memoryInstance);
    }

    //system memory not affected
    DebugManager.flags.OverrideDrmRegion.set(-1);
    DebugManager.flags.ForceMemoryBankIndexOverride.set(1);
    regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(MemoryBanks::MainBank, *pHwInfo);
    EXPECT_EQ(regionInfo[0].region.memoryInstance, regionClassAndInstance.memoryInstance);
}

TEST_F(MultiTileMemoryInfoPrelimTest, givenMemoryInfoWithoutRegionsWhenGettingMemoryRegionClassAndInstanceThenReturnInvalidMemoryRegion) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 8 * GB;

    setupMemoryInfo(regionInfo, 0);

    EXPECT_ANY_THROW(memoryInfo->getMemoryRegionClassAndInstance(1, *pHwInfo));

    auto regionSize = memoryInfo->getMemoryRegionSize(1);
    EXPECT_EQ(0 * GB, regionSize);
}

TEST_F(MultiTileMemoryInfoPrelimTest, whenDebugVariablePrintMemoryRegionSizeIsSetAndGetMemoryRegionSizeIsCalledThenMessagePrintedToStdOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintMemoryRegionSizes.set(true);

    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[0].probedSize = 16 * GB;

    setupMemoryInfo(regionInfo, 0);

    testing::internal::CaptureStdout();
    auto regionSize = memoryInfo->getMemoryRegionSize(MemoryBanks::MainBank);
    EXPECT_EQ(16 * GB, regionSize);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Memory type: 0, memory instance: 1, region size: 17179869184\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemWithExtensionsThenReturnCorrectValues) {
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {regionInfo[0].region, regionInfo[1].region};
    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    auto ret = memoryInfo->createGemExt(memClassInstance, 1024, handle, {});
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    ASSERT_TRUE(drm->context.receivedCreateGemExt);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithSingleRegionThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint32_t handle = 0;

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    ASSERT_EQ(1u, createExt->memoryRegions.size());
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, createExt->memoryRegions[0].memoryClass);
    EXPECT_EQ(1024u, drm->context.receivedCreateGemExt->size);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndPrivateBOSupportWhenCreatingGemExtWithSingleRegionThenValidVmIdIsSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    DebugManager.flags.EnablePrivateBO.set(true);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(false);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    auto validVmId = drm->getVirtualMemoryAddressSpace(0);
    EXPECT_EQ(validVmId, createExt->vmPrivateExt.vmId);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndNoPrivateBOSupportWhenCreatingGemExtWithSingleRegionThenVmIdIsNotSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    DebugManager.flags.EnablePrivateBO.set(false);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(false);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    EXPECT_EQ(std::nullopt, createExt->vmPrivateExt.vmId);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsAndPrivateBOSupportedAndIsPerContextVMRequiredIsTrueWhenCreatingGemExtWithSingleRegionThenVmIdIsNotSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    DebugManager.flags.EnablePrivateBO.set(true);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->setPerContextVMRequired(true);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);

    uint32_t handle = 0;
    auto ret = memoryInfo->createGemExtWithSingleRegion(1, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);

    const auto &createExt = drm->context.receivedCreateGemExt;
    ASSERT_TRUE(createExt);
    EXPECT_EQ(std::nullopt, createExt->vmPrivateExt.vmId);
}

TEST(MemoryInfo, givenMemoryInfoWithRegionsWhenCreatingGemExtWithMultipleRegionsThenReturnCorrectValues) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probedSize = 8 * GB;
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probedSize = 16 * GB;
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    regionInfo[2].probedSize = 16 * GB;
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2};
    regionInfo[3].probedSize = 16 * GB;
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 3};
    regionInfo[4].probedSize = 16 * GB;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto memoryInfo = std::make_unique<MemoryInfo>(regionInfo, *drm);
    ASSERT_NE(nullptr, memoryInfo);
    uint32_t handle = 0;
    uint32_t memoryRegions = 0b1011;
    auto ret = memoryInfo->createGemExtWithMultipleRegions(memoryRegions, 1024, handle);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
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
