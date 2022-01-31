/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

extern int handlePrelimRequests(unsigned long request, void *arg, int ioctlRetVal, int queryDistanceIoctlRetVal);
extern std::vector<uint8_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions);
extern std::vector<uint8_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines);

class DrmPrelimMock : public DrmMock {
  public:
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmPrelimMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment, HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        rootDeviceEnvironment.getMutableHardwareInfo()->platform.eProductFamily = IGFX_UNKNOWN;
    }

    int ioctlRetVal = 0;
    int queryDistanceIoctlRetVal = 0;

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }

    int handleRemainingRequests(unsigned long request, void *arg) override {
        return handlePrelimRequests(request, arg, ioctlRetVal, queryDistanceIoctlRetVal);
    }
};

class IoctlHelperPrelimFixture : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        drm->setupIoctlHelper();
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmPrelimMock> drm;
};

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtThenReturnSuccess) {
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = ioctlHelper->createGemExt(drm.get(), memClassInstance, 1024, handle);

    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    testing::internal::CaptureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{I915_MEMORY_CLASS_DEVICE, 0}};
    ioctlHelper->createGemExt(drm.get(), memClassInstance, 1024, handle);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, param: 0x1000000010001, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenTranslateToMemoryRegionsThenReturnSameData) {
    std::vector<MemoryRegion> expectedMemRegions(2);
    expectedMemRegions[0].region.memoryClass = I915_MEMORY_CLASS_SYSTEM;
    expectedMemRegions[0].region.memoryInstance = 0;
    expectedMemRegions[0].probedSize = 1024;
    expectedMemRegions[1].region.memoryClass = I915_MEMORY_CLASS_DEVICE;
    expectedMemRegions[1].region.memoryInstance = 0;
    expectedMemRegions[1].probedSize = 1024;

    auto regionInfo = getRegionInfo(expectedMemRegions);

    auto ioctlHelper = drm->getIoctlHelper();
    auto memRegions = ioctlHelper->translateToMemoryRegions(regionInfo);
    EXPECT_EQ(2u, memRegions.size());
    for (uint32_t i = 0; i < memRegions.size(); i++) {
        EXPECT_EQ(expectedMemRegions[i].region.memoryClass, memRegions[i].region.memoryClass);
        EXPECT_EQ(expectedMemRegions[i].region.memoryInstance, memRegions[i].region.memoryInstance);
        EXPECT_EQ(expectedMemRegions[i].probedSize, memRegions[i].probedSize);
        EXPECT_EQ(expectedMemRegions[i].unallocatedSize, memRegions[i].unallocatedSize);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCallIoctlThenProperIoctlRegistered) {
    drm_i915_gem_context_create_ext arg{};
    auto ret = IoctlHelper::ioctl(drm.get(), DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, &arg);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosAllocThenReturnCorrectRegion) {
    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(drm.get());

    EXPECT_EQ(CacheRegion::Region1, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocThenReturnNone) {
    drm->ioctlRetVal = -1;

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(drm.get());

    EXPECT_EQ(CacheRegion::None, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosFreeThenReturnCorrectRegion) {
    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(drm.get(), CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::Region2, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosFreeThenReturnNone) {
    drm->ioctlRetVal = -1;

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(drm.get(), CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::None, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosAllocWaysThenReturnCorrectRegion) {
    auto ioctlHelper = drm->getIoctlHelper();
    auto numWays = ioctlHelper->closAllocWays(drm.get(), CacheRegion::Region2, 3, 10);

    EXPECT_EQ(10u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocWaysThenReturnNone) {
    drm->ioctlRetVal = -1;

    auto ioctlHelper = drm->getIoctlHelper();
    auto numWays = ioctlHelper->closAllocWays(drm.get(), CacheRegion::Region2, 3, 10);

    EXPECT_EQ(0u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenWaitUserFenceThenCorrectValueReturned) {
    uint64_t gpuAddress = 0x1020304000ull;
    uint64_t value = 0x98765ull;
    auto ioctlHelper = drm->getIoctlHelper();
    for (uint32_t i = 0u; i < 4; i++) {
        auto ret = ioctlHelper->waitUserFence(drm.get(), 10u, gpuAddress, value, i, -1, 0u);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenGetHwConfigIoctlValThenCorrectValueReturned) {
    uint32_t ioctlVal = (1 << 16) | 6;
    EXPECT_EQ(ioctlVal, drm->getIoctlHelper()->getHwConfigIoctlVal());
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseFailsThenDontUpdateMemAdviceFlags) {
    drm->ioctlRetVal = -1;

    MockBufferObject bo(drm.get(), 0, 0, 1);
    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};
    memAdviseFlags.non_atomic = 1;

    allocation.setMemAdvise(drm.get(), memAdviseFlags);

    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_NE(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithNonAtomicIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(drm.get(), 0, 0, 1);
    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};

    for (auto nonAtomic : {true, false}) {
        memAdviseFlags.non_atomic = nonAtomic;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithDevicePreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(drm.get(), 0, 0, 1);
    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.device_preferred_location = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenGetDirectSubmissionFlagThenCorrectValueReturned) {
    uint32_t ioctlVal = (1u << 31);
    EXPECT_EQ(ioctlVal, drm->getIoctlHelper()->getDirectSubmissionFlag());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenAppendDrmContextFlagsThenCorrectFlagsSet) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DirectSubmissionDrmContext.set(-1);

    uint32_t ioctlVal = (1u << 31);

    drm_i915_gem_context_create_ext ctx{};
    drm->appendDrmContextFlags(ctx, true);
    EXPECT_EQ(ioctlVal, ctx.flags);

    ctx.flags = 0u;
    DebugManager.flags.DirectSubmissionDrmContext.set(0);

    drm->appendDrmContextFlags(ctx, true);
    EXPECT_EQ(0u, ctx.flags);

    DebugManager.flags.DirectSubmissionDrmContext.set(1);
    drm->appendDrmContextFlags(ctx, false);
    EXPECT_EQ(ioctlVal, ctx.flags);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenGetMemRegionsIoctlValThenCorrectValueReturned) {
    int32_t ioctlVal = (1 << 16) | 4;
    EXPECT_EQ(ioctlVal, drm->getIoctlHelper()->getMemRegionsIoctlVal());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenGetEngineInfoIoctlValThenCorrectValueReturned) {
    int32_t ioctlVal = (1 << 16) | 13;
    EXPECT_EQ(ioctlVal, drm->getIoctlHelper()->getEngineInfoIoctlVal());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenTranslateToEngineCapsThenReturnSameData) {
    std::vector<EngineCapabilities> expectedEngines(2);
    expectedEngines[0] = {{I915_ENGINE_CLASS_RENDER, 0}, 0};
    expectedEngines[1] = {{I915_ENGINE_CLASS_COPY, 1}, 0};

    auto engineInfo = getEngineInfo(expectedEngines);

    auto ioctlHelper = drm->getIoctlHelper();
    auto engines = ioctlHelper->translateToEngineCaps(engineInfo);
    EXPECT_EQ(2u, engines.size());
    for (uint32_t i = 0; i < engines.size(); i++) {
        EXPECT_EQ(expectedEngines[i].engine.engineClass, engines[i].engine.engineClass);
        EXPECT_EQ(expectedEngines[i].engine.engineInstance, engines[i].engine.engineInstance);
        EXPECT_EQ(expectedEngines[i].capabilities, engines[i].capabilities);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenQueryDistancesThenCorrectDistanceSet) {
    std::vector<DistanceInfo> distances(3);
    distances[0].engine = {I915_ENGINE_CLASS_RENDER, 0};
    distances[0].region = {I915_MEMORY_CLASS_DEVICE, 0};
    distances[1].engine = {I915_ENGINE_CLASS_RENDER, 1};
    distances[1].region = {I915_MEMORY_CLASS_DEVICE, 1};
    distances[2].engine = {I915_ENGINE_CLASS_COPY, 4};
    distances[2].region = {I915_MEMORY_CLASS_DEVICE, 2};
    std::vector<drm_i915_query_item> queryItems(distances.size());
    auto ret = drm->getIoctlHelper()->queryDistances(drm.get(), queryItems, distances);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(0, distances[0].distance);
    EXPECT_EQ(0, distances[1].distance);
    EXPECT_EQ(100, distances[2].distance);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoWithDeviceMemoryThenDistancesUsedAndMultileValuesSet) {
    std::vector<MemoryRegion> memRegions{
        {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions));
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto engineInfo = drm->getEngineInfo();

    auto &multiTileArchInfo = const_cast<GT_MULTI_TILE_ARCH_INFO &>(hwInfo->gtSystemInfo.MultiTileArchInfo);
    EXPECT_TRUE(multiTileArchInfo.IsValid);
    EXPECT_EQ(3, multiTileArchInfo.TileCount);
    EXPECT_EQ(7, multiTileArchInfo.TileMask);

    EXPECT_EQ(1024u, drm->memoryInfo->getMemoryRegionSize(1));
    EXPECT_EQ(1024u, drm->memoryInfo->getMemoryRegionSize(2));
    EXPECT_EQ(0u, drm->memoryInfo->getMemoryRegionSize(4));

    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0u, engines);
    EXPECT_EQ(3u, engines.size());

    engines.clear();
    engineInfo->getListOfEnginesOnATile(1u, engines);
    EXPECT_EQ(3u, engines.size());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoThenCorrectCCSFlagsSet) {
    std::vector<MemoryRegion> memRegions{
        {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions));
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto ccsInfo = hwInfo->gtSystemInfo.CCSInfo;
    EXPECT_TRUE(ccsInfo.IsValid);
    EXPECT_EQ(1u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ(1u, ccsInfo.Instances.CCSEnableMask);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenSysmanQueryEngineInfoThenAdditionalEnginesUsed) {
    std::vector<MemoryRegion> memRegions{
        {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions));
    EXPECT_TRUE(drm->sysmanQueryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0u, engines);
    EXPECT_EQ(5u, engines.size());

    engines.clear();
    engineInfo->getListOfEnginesOnATile(1u, engines);
    EXPECT_EQ(5u, engines.size());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoAndFailIoctlThenFalseReturned) {
    drm->queryDistanceIoctlRetVal = -1;

    std::vector<MemoryRegion> memRegions{
        {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions));
    EXPECT_FALSE(drm->queryEngineInfo());

    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    EXPECT_EQ(nullptr, engineInfo);
}
