/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/os_interface/linux/xe/mock_drm_xe.h"
#include "shared/test/common/os_interface/linux/xe/mock_ioctl_helper_xe.h"
#include "shared/test/common/os_interface/linux/xe/xe_config_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using IoctlHelperXeTest = Test<XeConfigFixture>;

TEST_F(IoctlHelperXeTest, givenXeDrmVersionsWhenGettingIoctlHelperThenValidIoctlHelperIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
}

TEST_F(IoctlHelperXeTest, whenGettingIfImmediateVmBindIsRequiredThenTrueIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    EXPECT_TRUE(ioctlHelper.isImmediateVmBindRequired());
}

TEST_F(IoctlHelperXeTest, whenGettingIfSmallBarConfigIsAllowedThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    EXPECT_FALSE(ioctlHelper.isSmallBarConfigAllowed());
}

TEST_F(IoctlHelperXeTest, givenXeDrmWhenGetPciBarrierMmapThenReturnsNullptr) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    auto ptr = ioctlHelper.pciBarrierMmap();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(IoctlHelperXeTest, whenChangingBufferBindingThenWaitIsNeededAlways) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperXe ioctlHelper{*drm};

    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(false));
}

struct GemCreateExtFixture {

    GemCreateExtFixture() : hwInfo{*executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()},
                            drm{DrmMockXe::create(*executionEnvironment.rootDeviceEnvironments[0])} {}

    void setUp() {
        xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    }
    void tearDown() {}

    MockExecutionEnvironment executionEnvironment{};
    const HardwareInfo &hwInfo;
    std::unique_ptr<DrmMockXe> drm;
    MockIoctlHelperXe *xeIoctlHelper{nullptr};

    MemoryClassInstance systemMemory = {drm_xe_memory_class::DRM_XE_MEM_REGION_CLASS_SYSMEM, 0};
    MemoryClassInstance localMemory = {drm_xe_memory_class::DRM_XE_MEM_REGION_CLASS_VRAM, 1};

    size_t allocSize = 0u;
    uint32_t handle = 0u;
    uint32_t numOfChunks = 0u;
    uint64_t patIndex = 0u;
    int32_t pairHandle = -1;
    bool isChunked = false;
    bool isCoherent = false;
};
using IoctlHelperXeGemCreateExtTests = Test<GemCreateExtFixture>;

TEST_F(IoctlHelperXeGemCreateExtTests, givenIoctlHelperXeWhenCallingGemCreateExtWithRegionsThenDummyValueIsReturned) {
    MemRegionsVec memRegions = {systemMemory, localMemory};

    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, allocSize, handle, patIndex, std::nullopt, pairHandle, isChunked, numOfChunks, std::nullopt, std::nullopt, isCoherent));
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
}

TEST_F(IoctlHelperXeGemCreateExtTests, givenIoctlHelperXeWhenCallingGemCreateExtWithRegionsAndVmIdThenDummyValueIsReturned) {
    MemRegionsVec memRegions = {systemMemory, localMemory};

    GemVmControl test = {};
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, allocSize, handle, patIndex, test.vmId, pairHandle, isChunked, numOfChunks, std::nullopt, std::nullopt, isCoherent));
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
}

TEST_F(IoctlHelperXeGemCreateExtTests, givenIoctlHelperXeWhenCallingGemCreateExtWithRegionsAndCoherencyThenUncachedCPUCachingIsUsed) {
    MemRegionsVec memRegions = {systemMemory, localMemory};
    bool isCoherent = true;

    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, allocSize, handle, patIndex, std::nullopt, pairHandle, isChunked, numOfChunks, std::nullopt, std::nullopt, isCoherent));
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
}

TEST_F(IoctlHelperXeGemCreateExtTests, givenIoctlHelperXeWhenCallingGemCreateExtWithOnlySystemRegionAndCoherencyThenWriteBackCPUCachingIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeferBacking.set(1);

    MemRegionsVec memRegions = {systemMemory};
    bool isCoherent = true;

    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, allocSize, handle, patIndex, std::nullopt, pairHandle, isChunked, numOfChunks, std::nullopt, std::nullopt, isCoherent));
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WB, drm->createParamsCpuCaching);
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING), (drm->createParamsFlags & DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING));
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGemCreateAndNoLocalMemoryThenProperValuesSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    debugManager.flags.EnableDeferBacking.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(0, drm->ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    uint32_t handle = xeIoctlHelper->createGem(size, memoryBanks, false);
    EXPECT_EQ(1, drm->ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());

    EXPECT_EQ(size, drm->createParamsSize);
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING), (drm->createParamsFlags & DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING));
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
    EXPECT_EQ(1u, drm->createParamsPlacement);

    // dummy mock handle
    EXPECT_EQ(handle, drm->createParamsHandle);
    EXPECT_EQ(handle, testValueGemCreate);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGemCreateWhenMemoryBanksZeroThenProperValuesSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    debugManager.flags.EnableDeferBacking.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    uint64_t size = 1234;
    uint32_t memoryBanks = 0u;

    EXPECT_EQ(0, drm->ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    uint32_t handle = xeIoctlHelper->createGem(size, memoryBanks, false);
    EXPECT_EQ(1, drm->ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());

    EXPECT_EQ(size, drm->createParamsSize);
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING), (drm->createParamsFlags & DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING));
    EXPECT_EQ(1u, drm->createParamsPlacement);

    // dummy mock handle
    EXPECT_EQ(handle, drm->createParamsHandle);
    EXPECT_EQ(handle, testValueGemCreate);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGemCreateAndLocalMemoryThenProperValuesSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    debugManager.flags.EnableDeferBacking.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(0, drm->ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    uint32_t handle = xeIoctlHelper->createGem(size, memoryBanks, false);
    EXPECT_EQ(1, drm->ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());

    EXPECT_EQ(size, drm->createParamsSize);
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING), (drm->createParamsFlags & DRM_XE_GEM_CREATE_FLAG_DEFER_BACKING));
    EXPECT_EQ(6u, drm->createParamsPlacement);

    // dummy mock handle
    EXPECT_EQ(handle, drm->createParamsHandle);
    EXPECT_EQ(handle, testValueGemCreate);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGemCreateWithRegionsAndCoherencyThenUncachedCPUCachingIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    bool isCoherent = true;
    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(testValueGemCreate, xeIoctlHelper->createGem(size, memoryBanks, isCoherent));
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm->createParamsCpuCaching);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGemCreateWithOnlySystemRegionAndCoherencyThenWriteBackCPUCachingIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    bool isCoherent = true;
    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(testValueGemCreate, xeIoctlHelper->createGem(size, memoryBanks, isCoherent));
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WB, drm->createParamsCpuCaching);
}

TEST_F(IoctlHelperXeTest, givenLmemRegionsCpuVisibleSizeEqualToProbedSizeWhenMemoryInfoCreatedThenSmallBarIsNotDetected) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    EXPECT_FALSE(drm->memoryInfo->isSmallBarDetected());
}

TEST_F(IoctlHelperXeTest, givenLmemRegionCpuVisibleSizeSmallerThanProbedSizeWhenMemoryInfoCreatedThenSmallBarIsDetected) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(drm->queryMemUsage);
    auto &smallBarRegion = xeQueryMemUsage->mem_regions[0];
    EXPECT_EQ(smallBarRegion.mem_class, DRM_XE_MEM_REGION_CLASS_VRAM);
    smallBarRegion.cpu_visible_size = smallBarRegion.total_size - 1U;

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    EXPECT_TRUE(drm->memoryInfo->isSmallBarDetected());
}

TEST_F(IoctlHelperXeTest, givenLmemRegionCpuVisibleSizeBeingZeroWhenMemoryInfoCreatedThenSmallBarIsNotDetected) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(drm->queryMemUsage);
    auto &smallBarRegion = xeQueryMemUsage->mem_regions[2];
    EXPECT_EQ(smallBarRegion.mem_class, DRM_XE_MEM_REGION_CLASS_VRAM);
    smallBarRegion.cpu_visible_size = 0U;

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    EXPECT_FALSE(drm->memoryInfo->isSmallBarDetected());
}

TEST_F(IoctlHelperXeTest, givenSysmemRegionCpuVisibleSizeSmallerThanProbedWhenMemoryInfoCreatedThenSmallBarIsDetected) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(drm->queryMemUsage);
    auto &sysmemRegion = xeQueryMemUsage->mem_regions[1];
    EXPECT_EQ(sysmemRegion.mem_class, DRM_XE_MEM_REGION_CLASS_SYSMEM);
    sysmemRegion.cpu_visible_size = sysmemRegion.total_size - 1U;

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    EXPECT_FALSE(drm->memoryInfo->isSmallBarDetected());
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallSetGemTilingThenAlwaysTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    drm->ioctlCalled = false;

    ASSERT_NE(nullptr, xeIoctlHelper);
    GemSetTiling setTiling{};
    uint32_t ret = xeIoctlHelper->setGemTiling(&setTiling);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(drm->ioctlCalled);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGetGemTilingThenAlwaysTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    drm->ioctlCalled = false;

    ASSERT_NE(nullptr, xeIoctlHelper);
    GemGetTiling getTiling{};
    uint32_t ret = xeIoctlHelper->getGemTiling(&getTiling);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(drm->ioctlCalled);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeisVmBindPatIndexExtSupportedReturnsFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_EQ(false, xeIoctlHelper->isVmBindPatIndexExtSupported());
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingAnyMethodThenDummyValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto verifyDrmParamString = [&xeIoctlHelper](const char *string, DrmParam drmParam) {
        EXPECT_STREQ(string, xeIoctlHelper->getDrmParamString(drmParam).c_str());
    };

    auto verifyIoctlString = [&xeIoctlHelper](DrmIoctl drmIoctl, const char *string) {
        EXPECT_STREQ(string, xeIoctlHelper->getIoctlString(drmIoctl).c_str());
    };

    auto verifyDrmGetParamValue = [&xeIoctlHelper](auto value, DrmParam drmParam) {
        EXPECT_EQ(value, xeIoctlHelper->getDrmParamValue(drmParam));
    };

    MemRegionsVec memRegions{};
    uint32_t handle = 0u;
    uint32_t numOfChunks = 0;
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, 0u, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt, false));

    EXPECT_TRUE(xeIoctlHelper->isVmBindAvailable());

    EXPECT_FALSE(xeIoctlHelper->isSetPairAvailable());

    EXPECT_EQ(CacheRegion::none, xeIoctlHelper->closAlloc(NEO::CacheLevel::level3));

    EXPECT_EQ(0u, xeIoctlHelper->closAllocWays(CacheRegion::none, 0u, 0u));

    EXPECT_EQ(CacheRegion::none, xeIoctlHelper->closFree(CacheRegion::none));

    EXPECT_EQ(0u, xeIoctlHelper->getAtomicAdvise(false));

    EXPECT_EQ(0u, xeIoctlHelper->getAtomicAccess(AtomicAccessMode::none));

    EXPECT_EQ(0u, xeIoctlHelper->getPreferredLocationAdvise());

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getPreferredLocationRegion(PreferredLocation::none, 0));

    EXPECT_EQ(0u, xeIoctlHelper->getPreferredLocationAdvise());

    EXPECT_TRUE(xeIoctlHelper->setVmBoAdvise(0, 0, nullptr));

    EXPECT_TRUE(xeIoctlHelper->setVmSharedSystemMemAdvise(0, 0, 0, 0, {0}));
    EXPECT_TRUE(xeIoctlHelper->setVmSharedSystemMemAdvise(0, 0, 0, 0, {0, 0}));

    EXPECT_TRUE(xeIoctlHelper->setVmBoAdviseForChunking(0, 0, 0, 0, nullptr));

    EXPECT_FALSE(xeIoctlHelper->isChunkingAvailable());

    EXPECT_EQ(0u, xeIoctlHelper->getDirectSubmissionFlag());

    EXPECT_EQ(0u, xeIoctlHelper->getFlagsForVmBind(false, false, false, false, false));

    std::vector<QueryItem> queryItems;
    std::vector<DistanceInfo> distanceInfos;
    EXPECT_EQ(0, xeIoctlHelper->queryDistances(queryItems, distanceInfos));
    EXPECT_EQ(0u, distanceInfos.size());

    EXPECT_EQ(0u, xeIoctlHelper->getWaitUserFenceSoftFlag());

    EXPECT_EQ(0, xeIoctlHelper->execBuffer(nullptr, 0, 0));

    EXPECT_FALSE(xeIoctlHelper->completionFenceExtensionSupported(false));

    EXPECT_EQ(nullptr, xeIoctlHelper->createVmControlExtRegion({}));

    GemContextCreateExt gcc;
    EXPECT_EQ(0u, xeIoctlHelper->createContextWithAccessCounters(gcc));

    EXPECT_EQ(0u, xeIoctlHelper->createCooperativeContext(gcc));

    VmBindExtSetPatT vmBindExtSetPat{};
    EXPECT_NO_THROW(xeIoctlHelper->fillVmBindExtSetPat(vmBindExtSetPat, 0, 0));

    VmBindExtUserFenceT vmBindExtUserFence{};
    EXPECT_NO_THROW(xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, 0, 0, 0));

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getVmAdviseAtomicAttribute());

    EXPECT_EQ(0u, xeIoctlHelper->getEuStallFdParameter());

    std::string uuid{};
    auto registerUuidResult = xeIoctlHelper->registerUuid(uuid, 0, 0, 0);
    EXPECT_EQ(0, registerUuidResult.retVal);
    EXPECT_EQ(0u, registerUuidResult.handle);

    auto registerStringClassUuidResult = xeIoctlHelper->registerStringClassUuid(uuid, 0, 0);
    EXPECT_EQ(0, registerStringClassUuidResult.retVal);
    EXPECT_EQ(0u, registerStringClassUuidResult.handle);

    EXPECT_EQ(0, xeIoctlHelper->unregisterUuid(0));

    EXPECT_FALSE(xeIoctlHelper->isContextDebugSupported());

    EXPECT_EQ(0, xeIoctlHelper->setContextDebugFlag(0));

    verifyDrmGetParamValue(-1, DrmParam::atomicClassUndefined);
    verifyDrmGetParamValue(-1, DrmParam::atomicClassDevice);
    verifyDrmGetParamValue(-1, DrmParam::atomicClassGlobal);
    verifyDrmGetParamValue(-1, DrmParam::atomicClassSystem);
    verifyDrmGetParamValue(DRM_XE_MEM_REGION_CLASS_VRAM, DrmParam::memoryClassDevice);
    verifyDrmGetParamValue(DRM_XE_MEM_REGION_CLASS_SYSMEM, DrmParam::memoryClassSystem);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_RENDER, DrmParam::engineClassRender);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_COPY, DrmParam::engineClassCopy);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_VIDEO_DECODE, DrmParam::engineClassVideo);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, DrmParam::engineClassVideoEnhance);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_COMPUTE, DrmParam::engineClassCompute);
    verifyDrmGetParamValue(-1, DrmParam::engineClassInvalid);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_RENDER, DrmParam::execRender);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_COPY, DrmParam::execBlt);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_COMPUTE, DrmParam::execDefault);

    // Expect stringify
    verifyDrmParamString("AtomicClassUndefined", DrmParam::atomicClassUndefined);
    verifyDrmParamString("AtomicClassDevice", DrmParam::atomicClassDevice);
    verifyDrmParamString("AtomicClassGlobal", DrmParam::atomicClassGlobal);
    verifyDrmParamString("AtomicClassSystem", DrmParam::atomicClassSystem);
    verifyDrmParamString("ContextCreateExtSetparam", DrmParam::contextCreateExtSetparam);
    verifyDrmParamString("ContextCreateExtSetparam", DrmParam::contextCreateExtSetparam);
    verifyDrmParamString("ContextCreateFlagsUseExtensions", DrmParam::contextCreateFlagsUseExtensions);
    verifyDrmParamString("ContextEnginesExtLoadBalance", DrmParam::contextEnginesExtLoadBalance);
    verifyDrmParamString("ContextParamEngines", DrmParam::contextParamEngines);
    verifyDrmParamString("ContextParamGttSize", DrmParam::contextParamGttSize);
    verifyDrmParamString("ContextParamPersistence", DrmParam::contextParamPersistence);
    verifyDrmParamString("ContextParamPriority", DrmParam::contextParamPriority);
    verifyDrmParamString("ContextParamRecoverable", DrmParam::contextParamRecoverable);
    verifyDrmParamString("ContextParamSseu", DrmParam::contextParamSseu);
    verifyDrmParamString("ContextParamVm", DrmParam::contextParamVm);
    verifyDrmParamString("EngineClassRender", DrmParam::engineClassRender);
    verifyDrmParamString("EngineClassCompute", DrmParam::engineClassCompute);
    verifyDrmParamString("EngineClassCopy", DrmParam::engineClassCopy);
    verifyDrmParamString("EngineClassVideo", DrmParam::engineClassVideo);
    verifyDrmParamString("EngineClassVideoEnhance", DrmParam::engineClassVideoEnhance);
    verifyDrmParamString("EngineClassInvalid", DrmParam::engineClassInvalid);
    verifyDrmParamString("EngineClassInvalidNone", DrmParam::engineClassInvalidNone);
    verifyDrmParamString("ExecBlt", DrmParam::execBlt);
    verifyDrmParamString("ExecDefault", DrmParam::execDefault);
    verifyDrmParamString("ExecNoReloc", DrmParam::execNoReloc);
    verifyDrmParamString("ExecRender", DrmParam::execRender);
    verifyDrmParamString("MemoryClassDevice", DrmParam::memoryClassDevice);
    verifyDrmParamString("MemoryClassSystem", DrmParam::memoryClassSystem);
    verifyDrmParamString("MmapOffsetWb", DrmParam::mmapOffsetWb);
    verifyDrmParamString("MmapOffsetWc", DrmParam::mmapOffsetWc);
    verifyDrmParamString("ParamHasPooledEu", DrmParam::paramHasPooledEu);
    verifyDrmParamString("ParamEuTotal", DrmParam::paramEuTotal);
    verifyDrmParamString("ParamSubsliceTotal", DrmParam::paramSubsliceTotal);
    verifyDrmParamString("ParamMinEuInPool", DrmParam::paramMinEuInPool);
    verifyDrmParamString("ParamCsTimestampFrequency", DrmParam::paramCsTimestampFrequency);
    verifyDrmParamString("ParamHasVmBind", DrmParam::paramHasVmBind);
    verifyDrmParamString("ParamHasPageFault", DrmParam::paramHasPageFault);
    verifyDrmParamString("QueryEngineInfo", DrmParam::queryEngineInfo);
    verifyDrmParamString("QueryHwconfigTable", DrmParam::queryHwconfigTable);
    verifyDrmParamString("QueryComputeSlices", DrmParam::queryComputeSlices);
    verifyDrmParamString("QueryMemoryRegions", DrmParam::queryMemoryRegions);
    verifyDrmParamString("QueryTopologyInfo", DrmParam::queryTopologyInfo);
    verifyDrmParamString("TilingNone", DrmParam::tilingNone);
    verifyDrmParamString("TilingY", DrmParam::tilingY);

    verifyIoctlString(DrmIoctl::gemClose, "DRM_IOCTL_GEM_CLOSE");
    verifyIoctlString(DrmIoctl::gemVmCreate, "DRM_IOCTL_XE_VM_CREATE");
    verifyIoctlString(DrmIoctl::gemVmDestroy, "DRM_IOCTL_XE_VM_DESTROY");
    verifyIoctlString(DrmIoctl::gemMmapOffset, "DRM_IOCTL_XE_GEM_MMAP_OFFSET");
    verifyIoctlString(DrmIoctl::gemCreate, "DRM_IOCTL_XE_GEM_CREATE");
    verifyIoctlString(DrmIoctl::gemExecbuffer2, "DRM_IOCTL_XE_EXEC");
    verifyIoctlString(DrmIoctl::gemVmBind, "DRM_IOCTL_XE_VM_BIND");
    verifyIoctlString(DrmIoctl::query, "DRM_IOCTL_XE_DEVICE_QUERY");
    verifyIoctlString(DrmIoctl::gemContextCreateExt, "DRM_IOCTL_XE_EXEC_QUEUE_CREATE");
    verifyIoctlString(DrmIoctl::gemContextDestroy, "DRM_IOCTL_XE_EXEC_QUEUE_DESTROY");
    verifyIoctlString(DrmIoctl::gemWaitUserFence, "DRM_IOCTL_XE_WAIT_USER_FENCE");
    verifyIoctlString(DrmIoctl::primeFdToHandle, "DRM_IOCTL_PRIME_FD_TO_HANDLE");
    verifyIoctlString(DrmIoctl::primeHandleToFd, "DRM_IOCTL_PRIME_HANDLE_TO_FD");
    verifyIoctlString(DrmIoctl::syncObjFdToHandle, "DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE");
    verifyIoctlString(DrmIoctl::syncObjWait, "DRM_IOCTL_SYNCOBJ_WAIT");
    verifyIoctlString(DrmIoctl::syncObjSignal, "DRM_IOCTL_SYNCOBJ_SIGNAL");
    verifyIoctlString(DrmIoctl::syncObjTimelineWait, "DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT");
    verifyIoctlString(DrmIoctl::syncObjTimelineSignal, "DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL");
    verifyIoctlString(DrmIoctl::getResetStats, "DRM_IOCTL_XE_EXEC_QUEUE_GET_PROPERTY");

    EXPECT_TRUE(xeIoctlHelper->completionFenceExtensionSupported(true));

    uint32_t fabricId = 0, latency = 0, bandwidth = 0;
    EXPECT_FALSE(xeIoctlHelper->getFabricLatency(fabricId, latency, bandwidth));
}

TEST_F(IoctlHelperXeTest, whenGettingFlagsForVmCreateThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);

    for (auto &disableScratch : ::testing::Bool()) {
        for (auto &enablePageFault : ::testing::Bool()) {
            for (auto &useVmBind : ::testing::Bool()) {
                auto flags = xeIoctlHelper->getFlagsForVmCreate(disableScratch, enablePageFault, useVmBind);
                EXPECT_EQ(static_cast<uint32_t>(DRM_XE_VM_CREATE_FLAG_LR_MODE), (flags & DRM_XE_VM_CREATE_FLAG_LR_MODE));
                if (enablePageFault) {
                    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_VM_CREATE_FLAG_FAULT_MODE), (flags & DRM_XE_VM_CREATE_FLAG_FAULT_MODE));
                }
                if (!disableScratch) {
                    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_VM_CREATE_FLAG_SCRATCH_PAGE), (flags & DRM_XE_VM_CREATE_FLAG_SCRATCH_PAGE));
                }
            }
        }
    }
}

TEST_F(IoctlHelperXeTest, whenGettingFlagsForVmBindThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);

    EXPECT_EQ(static_cast<uint64_t>(DRM_XE_VM_BIND_FLAG_DUMPABLE), xeIoctlHelper->getFlagsForVmBind(true, false, false, false, false));
    EXPECT_EQ(static_cast<uint64_t>(0), xeIoctlHelper->getFlagsForVmBind(false, false, false, false, false));
}

TEST_F(IoctlHelperXeTest, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    auto verifyIoctlRequestValue = [&xeIoctlHelper](auto value, DrmIoctl drmIoctl) {
        EXPECT_EQ(xeIoctlHelper->getIoctlRequestValue(drmIoctl), static_cast<unsigned int>(value));
    };

    verifyIoctlRequestValue(DRM_IOCTL_XE_EXEC, DrmIoctl::gemExecbuffer2);
    verifyIoctlRequestValue(DRM_IOCTL_XE_WAIT_USER_FENCE, DrmIoctl::gemWaitUserFence);
    verifyIoctlRequestValue(DRM_IOCTL_XE_GEM_CREATE, DrmIoctl::gemCreate);
    verifyIoctlRequestValue(DRM_IOCTL_XE_VM_BIND, DrmIoctl::gemVmBind);
    verifyIoctlRequestValue(DRM_IOCTL_XE_VM_CREATE, DrmIoctl::gemVmCreate);
    verifyIoctlRequestValue(DRM_IOCTL_GEM_CLOSE, DrmIoctl::gemClose);
    verifyIoctlRequestValue(DRM_IOCTL_XE_VM_DESTROY, DrmIoctl::gemVmDestroy);
    verifyIoctlRequestValue(DRM_IOCTL_XE_GEM_MMAP_OFFSET, DrmIoctl::gemMmapOffset);
    verifyIoctlRequestValue(DRM_IOCTL_XE_DEVICE_QUERY, DrmIoctl::query);
    verifyIoctlRequestValue(DRM_IOCTL_XE_EXEC_QUEUE_CREATE, DrmIoctl::gemContextCreateExt);
    verifyIoctlRequestValue(DRM_IOCTL_XE_EXEC_QUEUE_DESTROY, DrmIoctl::gemContextDestroy);
    verifyIoctlRequestValue(DRM_IOCTL_PRIME_FD_TO_HANDLE, DrmIoctl::primeFdToHandle);
    verifyIoctlRequestValue(DRM_IOCTL_PRIME_HANDLE_TO_FD, DrmIoctl::primeHandleToFd);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, DrmIoctl::syncObjFdToHandle);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_WAIT, DrmIoctl::syncObjWait);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_SIGNAL, DrmIoctl::syncObjSignal);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, DrmIoctl::syncObjTimelineWait);
    verifyIoctlRequestValue(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL, DrmIoctl::syncObjTimelineSignal);
}

TEST_F(IoctlHelperXeTest, verifyPublicFunctions) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    auto mockXeIoctlHelper = static_cast<MockIoctlHelperXe *>(xeIoctlHelper.get());

    auto verifyXeClassName = [&mockXeIoctlHelper](const char *name, auto xeClass) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetClassName(xeClass));
    };

    auto verifyXeOperationBindName = [&mockXeIoctlHelper](const char *name, auto bind) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetBindOperationName(bind));
    };

    auto verifyXeOperationAdviseName = [&mockXeIoctlHelper](const char *name, auto advise) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetAdviseOperationName(advise));
    };

    auto verifyXeFlagsBindName = [&mockXeIoctlHelper](const char *name, auto flags) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetBindFlagNames(flags).c_str());
    };

    auto verifyXeEngineClassName = [&mockXeIoctlHelper](const char *name, auto engineClass) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetengineClassName(engineClass));
    };

    verifyXeClassName("rcs", DRM_XE_ENGINE_CLASS_RENDER);
    verifyXeClassName("bcs", DRM_XE_ENGINE_CLASS_COPY);
    verifyXeClassName("vcs", DRM_XE_ENGINE_CLASS_VIDEO_DECODE);
    verifyXeClassName("vecs", DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE);
    verifyXeClassName("ccs", DRM_XE_ENGINE_CLASS_COMPUTE);

    verifyXeOperationBindName("MAP", DRM_XE_VM_BIND_OP_MAP);
    verifyXeOperationBindName("UNMAP", DRM_XE_VM_BIND_OP_UNMAP);
    verifyXeOperationBindName("MAP_USERPTR", DRM_XE_VM_BIND_OP_MAP_USERPTR);
    verifyXeOperationBindName("UNMAP ALL", DRM_XE_VM_BIND_OP_UNMAP_ALL);
    verifyXeOperationBindName("PREFETCH", DRM_XE_VM_BIND_OP_PREFETCH);
    verifyXeOperationBindName("Unknown operation", -1);

    verifyXeOperationAdviseName("Unknown operation", -1);

    verifyXeFlagsBindName("", 0);
    verifyXeFlagsBindName("NULL", DRM_XE_VM_BIND_FLAG_NULL);
    verifyXeFlagsBindName("READONLY", DRM_XE_VM_BIND_FLAG_READONLY);
    verifyXeFlagsBindName("IMMEDIATE", DRM_XE_VM_BIND_FLAG_IMMEDIATE);
    verifyXeFlagsBindName("DUMPABLE", DRM_XE_VM_BIND_FLAG_DUMPABLE);
    verifyXeFlagsBindName("Unknown flag", 1 << 31);

    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_RENDER", DRM_XE_ENGINE_CLASS_RENDER);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_COPY", DRM_XE_ENGINE_CLASS_COPY);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_VIDEO_DECODE", DRM_XE_ENGINE_CLASS_VIDEO_DECODE);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE", DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE);
    verifyXeEngineClassName("DRM_XE_ENGINE_CLASS_COMPUTE", DRM_XE_ENGINE_CLASS_COMPUTE);
    verifyXeEngineClassName("Unknown engine class", 0xffffffff);

    Query query{};
    QueryItem queryItem{};
    queryItem.queryId = 999999;
    queryItem.length = 0;
    queryItem.flags = 0;
    query.itemsPtr = reinterpret_cast<uint64_t>(&queryItem);
    query.numItems = 1;

    EXPECT_EQ(-1, mockXeIoctlHelper->ioctl(DrmIoctl::query, &query));
    queryItem.queryId = xeIoctlHelper->getDrmParamValue(DrmParam::queryHwconfigTable);
    mockXeIoctlHelper->ioctl(DrmIoctl::query, &query);
    EXPECT_EQ(0, queryItem.length);

    memset(&queryItem, 0, sizeof(queryItem));
    queryItem.queryId = xeIoctlHelper->getDrmParamValue(DrmParam::queryEngineInfo);
    mockXeIoctlHelper->ioctl(DrmIoctl::query, &query);
    EXPECT_EQ(0, queryItem.length);

    memset(&queryItem, 0, sizeof(queryItem));
    queryItem.queryId = xeIoctlHelper->getDrmParamValue(DrmParam::queryTopologyInfo);
    mockXeIoctlHelper->ioctl(DrmIoctl::query, &query);
    EXPECT_EQ(0, queryItem.length);
}

TEST_F(IoctlHelperXeTest, whenGettingFileNamesForFrequencyThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    ioctlHelper->initialize();
    EXPECT_STREQ("/device/tile0/gt0/freq0/max_freq", ioctlHelper->getFileForMaxGpuFrequency().c_str());
    EXPECT_STREQ("/device/tile0/gt0/freq0/max_freq", ioctlHelper->getFileForMaxGpuFrequencyOfSubDevice(0).c_str());
    EXPECT_STREQ("/device/tile1/gt2/freq0/rp0_freq", ioctlHelper->getFileForMaxMemoryFrequencyOfSubDevice(1).c_str());
}

TEST_F(IoctlHelperXeTest, whenCallingIoctlThenProperValueIsReturned) {
    int ret;
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockXeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    mockXeIoctlHelper->initialize();

    drm->reset();
    {
        drm->testMode(1, -1);
        ret = mockXeIoctlHelper->initialize();
        EXPECT_EQ(0, ret);
    }
    drm->testMode(0);
    {
        GemUserPtr test = {};
        test.userPtr = 2;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemUserptr, &test);
        EXPECT_EQ(0, ret);

        EXPECT_EQ(test.userPtr, mockXeIoctlHelper->bindInfo[0].userptr);
        GemClose cl = {};
        cl.userptr = test.userPtr;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemClose, &cl);
        EXPECT_EQ(0, ret);
    }
    {
        GemVmControl test = {};
        uint32_t expectedVmCreateFlags = DRM_XE_VM_CREATE_FLAG_LR_MODE;
        test.flags = expectedVmCreateFlags;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemVmCreate, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.vmId), testValueVmId);
        EXPECT_EQ(test.flags, expectedVmCreateFlags);

        expectedVmCreateFlags = DRM_XE_VM_CREATE_FLAG_LR_MODE |
                                DRM_XE_VM_CREATE_FLAG_FAULT_MODE;
        test.flags = expectedVmCreateFlags;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemVmCreate, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.vmId), testValueVmId);
        EXPECT_EQ(test.flags, expectedVmCreateFlags);
    }
    {
        GemVmControl test = {};
        test.vmId = testValueVmId;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemVmDestroy, &test);
        EXPECT_EQ(0, ret);
    }
    {
        GemMmapOffset test = {};
        test.handle = testValueMapOff;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemMmapOffset, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.offset), testValueMapOff);
    }
    {
        PrimeHandle test = {};
        test.fileDescriptor = testValuePrime;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::primeFdToHandle, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.handle), testValuePrime);
    }
    {
        PrimeHandle test = {};
        test.handle = testValuePrime;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::primeHandleToFd, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.fileDescriptor), testValuePrime);
    }
    {
        SyncObjHandle test = {};
        test.fd = 0;
        test.flags = 0;
        test.handle = 0;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::syncObjFdToHandle, &test);
        EXPECT_EQ(0, ret);
    }
    {
        SyncObjWait test = {};
        test.handles = static_cast<uintptr_t>(0x1234);
        test.timeoutNs = 0;
        test.countHandles = 1u;
        test.flags = 0;

        ret = mockXeIoctlHelper->ioctl(DrmIoctl::syncObjWait, &test);
        EXPECT_EQ(0, ret);
    }
    {
        SyncObjTimelineWait test = {};
        test.handles = static_cast<uintptr_t>(0x1234);
        test.points = static_cast<uintptr_t>(0x1234);
        test.timeoutNs = 0;
        test.countHandles = 1u;
        test.flags = 0;

        ret = mockXeIoctlHelper->ioctl(DrmIoctl::syncObjTimelineWait, &test);
        EXPECT_EQ(0, ret);
    }
    {
        SyncObjArray test = {};
        test.handles = static_cast<uintptr_t>(0x1234);
        test.countHandles = 1u;

        ret = mockXeIoctlHelper->ioctl(DrmIoctl::syncObjSignal, &test);
        EXPECT_EQ(0, ret);
    }
    {
        SyncObjTimelineArray test = {};
        test.handles = static_cast<uintptr_t>(0x1234);
        test.points = static_cast<uintptr_t>(0x1234);
        test.countHandles = 1u;

        ret = mockXeIoctlHelper->ioctl(DrmIoctl::syncObjTimelineSignal, &test);
        EXPECT_EQ(0, ret);
    }
    {
        drm_xe_gem_create test = {};
        test.handle = 0;
        test.placement = 1;
        test.flags = 0;
        test.size = 123;
        test.cpu_caching = DRM_XE_GEM_CPU_CACHING_WC;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemCreate, &test);
        EXPECT_EQ(0, ret);
    }
    {
        ResetStats test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getResetStats, &test);
        EXPECT_EQ(0, ret);
    }
    {
        Query test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::query, &test);
        EXPECT_EQ(-1, ret);
    }
    {
        GemContextParam test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::contextParamPersistence);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = contextPrivateParamBoost;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::contextParamEngines);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
    }
    {
        auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();

        GemContextParam test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextGetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::contextParamGttSize);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextGetparam, &test);
        EXPECT_EQ(0, ret);

        auto expectedAddressWidth = hwInfo->capabilityTable.gpuAddressSpace + 1u;
        EXPECT_EQ(expectedAddressWidth, test.value);
        test.param = static_cast<int>(DrmParam::contextParamSseu);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextGetparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::contextParamPersistence);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextGetparam, &test);
        EXPECT_EQ(-1, ret);
    }
    {
        GemClose test = {};
        test.handle = 1;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemClose, &test);
        EXPECT_EQ(0, ret);
    }
    auto engineInfo = mockXeIoctlHelper->createEngineInfo(false);
    EXPECT_NE(nullptr, engineInfo);
    EXPECT_TRUE(engineInfo->hasEngines());
    {
        GetParam test = {};
        int dstvalue;
        test.value = &dstvalue;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::paramHasPageFault);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::paramCsTimestampFrequency);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<uint32_t>(dstvalue), DrmMockXe::mockTimestampFrequency);
    }
    EXPECT_THROW(mockXeIoctlHelper->ioctl(DrmIoctl::gemContextCreateExt, NULL), std::runtime_error);
    drm->reset();
}

TEST_F(IoctlHelperXeTest, givenUnknownTopologyTypeWhenGetTopologyDataAndMapThenNotRecognizedTopologyIsIgnored) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    xeIoctlHelper->initialize();

    constexpr int16_t unknownTopology = -1;

    uint16_t tileId = 0;
    for (auto gtId = 0u; gtId < 4u; gtId++) {
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0b11'1111, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, unknownTopology, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
    }
    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 6;
    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSlices);

    EXPECT_EQ(6, topologyData.subSliceCount);
    EXPECT_EQ(6, topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(96, topologyData.euCount);
    EXPECT_EQ(16, topologyData.maxEusPerSubSlice);

    // verify topology map
    std::vector<int> expectedSliceIndices{0};
    ASSERT_EQ(expectedSliceIndices.size(), topologyMap[tileId].sliceIndices.size());
    ASSERT_TRUE(topologyMap[tileId].sliceIndices.size() > 0);

    for (auto i = 0u; i < expectedSliceIndices.size(); i++) {
        EXPECT_EQ(expectedSliceIndices[i], topologyMap[tileId].sliceIndices[i]);
    }

    std::vector<int> expectedSubSliceIndices{0, 1, 2, 3, 4, 5};
    ASSERT_EQ(expectedSubSliceIndices.size(), topologyMap[tileId].subsliceIndices.size());
    ASSERT_TRUE(topologyMap[tileId].subsliceIndices.size() > 0);

    for (auto i = 0u; i < expectedSubSliceIndices.size(); i++) {
        EXPECT_EQ(expectedSubSliceIndices[i], topologyMap[tileId].subsliceIndices[i]);
    }
}

TEST_F(IoctlHelperXeTest, givenVariousDssConfigInputsWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    xeIoctlHelper->initialize();

    for (auto &dssConfigType : {DRM_XE_TOPO_DSS_GEOMETRY, DRM_XE_TOPO_DSS_COMPUTE}) {
        for (auto &euPerDssConfigType : {DRM_XE_TOPO_EU_PER_DSS, DRM_XE_TOPO_SIMD16_EU_PER_DSS}) {

            drm->queryTopology.clear();

            uint16_t tileId = 0;
            for (auto gtId = 0u; gtId < 4u; gtId++) {
                drm->addMockedQueryTopologyData(gtId, dssConfigType, 8, {0x0fu, 0xff, 0u, 0xff, 0u, 0u, 0xff, 0xff});
                drm->addMockedQueryTopologyData(gtId, euPerDssConfigType, 8, {0b1111'1111, 0, 0, 0, 0, 0, 0, 0});
            }

            DrmQueryTopologyData topologyData{};
            TopologyMap topologyMap{};

            hwInfo.gtSystemInfo.MaxSlicesSupported = 4u;
            hwInfo.gtSystemInfo.MaxSubSlicesSupported = 32u;
            auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
            ASSERT_TRUE(result);

            // verify topology data
            EXPECT_EQ(3, topologyData.sliceCount);
            EXPECT_EQ(4, topologyData.maxSlices);

            EXPECT_EQ(20, topologyData.subSliceCount);
            EXPECT_EQ(8, topologyData.maxSubSlicesPerSlice);

            EXPECT_EQ(160, topologyData.euCount);
            EXPECT_EQ(8, topologyData.maxEusPerSubSlice);

            // verify topology map
            std::vector<int> expectedSliceIndices = {0, 1, 3};
            ASSERT_EQ(expectedSliceIndices.size(), topologyMap[tileId].sliceIndices.size());
            ASSERT_TRUE(topologyMap[tileId].sliceIndices.size() > 0);

            for (auto i = 0u; i < expectedSliceIndices.size(); i++) {
                EXPECT_EQ(expectedSliceIndices[i], topologyMap[tileId].sliceIndices[i]);
            }

            EXPECT_EQ(0u, topologyMap[tileId].subsliceIndices.size());
        }
    }
}

TEST_F(IoctlHelperXeTest, givenOnlyMediaTypeWhenGetTopologyDataAndMapThenSubsliceIndicesNotSet) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->queryGtList.resize(13); // 1 qword for num gts, 12 per gt
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm->queryGtList.begin());
    xeQueryGtList->num_gt = 1;
    xeQueryGtList->gt_list[0] = {
        DRM_XE_QUERY_GT_TYPE_MEDIA, // type
        0,                          // tile_id
        0,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b100,                      // native mem regions
        0x011,                      // slow mem regions
    };

    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    uint16_t tileId = 0;
    drm->addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
    drm->addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    drm->addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 8, {0b1111'1111, 0, 0, 0, 0, 0, 0, 0});

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    EXPECT_FALSE(result);

    // verify topology data
    EXPECT_EQ(0, topologyData.sliceCount);
    EXPECT_EQ(static_cast<int>(hwInfo.gtSystemInfo.MaxSlicesSupported), topologyData.maxSlices);

    EXPECT_EQ(0, topologyData.subSliceCount);
    EXPECT_EQ(static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported / topologyData.maxSlices), topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(0, topologyData.euCount);
    EXPECT_EQ(0, topologyData.maxEusPerSubSlice);

    // verify topology map
    ASSERT_EQ(0u, topologyMap[tileId].sliceIndices.size());

    ASSERT_EQ(0u, topologyMap[tileId].subsliceIndices.size());
}

TEST_F(IoctlHelperXeTest, givenMainAndMediaTypesWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->queryGtList.resize(49);
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm->queryGtList.begin());
    xeQueryGtList->num_gt = 4;
    xeQueryGtList->gt_list[0] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        0,                         // tile_id
        0,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b100,                     // native mem regions
        0x011,                     // slow mem regions
    };
    xeQueryGtList->gt_list[1] = {
        DRM_XE_QUERY_GT_TYPE_MEDIA, // type
        0,                          // tile_id
        1,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b100,                      // native mem regions
        0x011,                      // slow mem regions
    };
    xeQueryGtList->gt_list[2] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        1,                         // tile_id
        2,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b010,                     // native mem regions
        0x101,                     // slow mem regions
    };
    xeQueryGtList->gt_list[3] = {
        DRM_XE_QUERY_GT_TYPE_MEDIA, // type
        1,                          // tile_id
        3,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b001,                      // native mem regions
        0x100,                      // slow mem regions
    };

    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    for (auto tileId = 0; tileId < 4; tileId++) {
        drm->addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm->addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm->addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 64;

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSlices);

    EXPECT_EQ(64, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(4096, topologyData.euCount);
    EXPECT_EQ(64, topologyData.maxEusPerSubSlice);
    EXPECT_EQ(2u, topologyMap.size());
    // verify topology map
    for (auto tileId : {0u, 1u}) {
        ASSERT_EQ(1u, topologyMap[tileId].sliceIndices.size());
        ASSERT_EQ(64u, topologyMap[tileId].subsliceIndices.size());
    }
}

TEST_F(IoctlHelperXeTest, GivenSingleTileWithMainTypesWhenCallingGetTileIdFromGtIdThenExpectedValuesAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->queryGtList.resize(49);
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm->queryGtList.begin());
    xeQueryGtList->num_gt = 2;
    xeQueryGtList->gt_list[0] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        0,                         // tile_id
        0,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b100,                     // native mem regions
        0x011,                     // slow mem regions
    };
    xeQueryGtList->gt_list[1] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        0,                         // tile_id
        1,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b100,                     // native mem regions
        0x011,                     // slow mem regions
    };

    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    EXPECT_EQ(0u, xeIoctlHelper->getTileIdFromGtId(0));
    EXPECT_EQ(0u, xeIoctlHelper->getTileIdFromGtId(1));
}

TEST_F(IoctlHelperXeTest, GivenSingleTileWithMainAndMediaTypesWhenCallingGetGtIdFromTileIdThenExpectedValuesAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->queryGtList.resize(49);
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm->queryGtList.begin());
    xeQueryGtList->num_gt = 4;
    xeQueryGtList->gt_list[0] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        0,                         // tile_id
        0,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b100,                     // native mem regions
        0x011,                     // slow mem regions
    };
    xeQueryGtList->gt_list[1] = {
        DRM_XE_QUERY_GT_TYPE_MEDIA, // type
        0,                          // tile_id
        1,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b100,                      // native mem regions
        0x011,                      // slow mem regions
    };
    xeQueryGtList->gt_list[2] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        1,                         // tile_id
        2,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b010,                     // native mem regions
        0x101,                     // slow mem regions
    };
    xeQueryGtList->gt_list[3] = {
        DRM_XE_QUERY_GT_TYPE_MEDIA, // type
        1,                          // tile_id
        3,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b001,                      // native mem regions
        0x100,                      // slow mem regions
    };

    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    EXPECT_EQ(0u, xeIoctlHelper->getGtIdFromTileId(0, DRM_XE_ENGINE_CLASS_RENDER));
    EXPECT_EQ(2u, xeIoctlHelper->getGtIdFromTileId(1, DRM_XE_ENGINE_CLASS_COPY));
    EXPECT_EQ(1u, xeIoctlHelper->getGtIdFromTileId(0, DRM_XE_ENGINE_CLASS_VIDEO_DECODE));
    EXPECT_EQ(3u, xeIoctlHelper->getGtIdFromTileId(1, DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE));
}

TEST_F(IoctlHelperXeTest, given2TileAndComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    xeIoctlHelper->initialize();

    for (auto gtId = 0u; gtId < 4u; gtId++) {
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 64;
    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSlices);

    EXPECT_EQ(64, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(512, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEusPerSubSlice);

    // verify topology map
    for (auto tileId : {0u, 1u}) {
        std::vector<int> expectedSliceIndices{0};

        ASSERT_EQ(expectedSliceIndices.size(), topologyMap[tileId].sliceIndices.size());
        ASSERT_TRUE(topologyMap[tileId].sliceIndices.size() > 0);

        for (auto i = 0u; i < expectedSliceIndices.size(); i++) {
            EXPECT_EQ(expectedSliceIndices[i], topologyMap[tileId].sliceIndices[i]);
        }

        std::vector<int> expectedSubSliceIndices;
        expectedSubSliceIndices.reserve(64u);
        for (auto i = 0u; i < 64; i++) {
            expectedSubSliceIndices.emplace_back(i);
        }

        ASSERT_EQ(expectedSubSliceIndices.size(), topologyMap[tileId].subsliceIndices.size());
        ASSERT_TRUE(topologyMap[tileId].subsliceIndices.size() > 0);

        for (auto i = 0u; i < expectedSubSliceIndices.size(); i++) {
            EXPECT_EQ(expectedSubSliceIndices[i], topologyMap[tileId].subsliceIndices[i]);
        }
    }
}

TEST_F(IoctlHelperXeTest, given2TileWithDisabledDssOn1TileAndComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    xeIoctlHelper->initialize();

    for (auto gtId = 0u; gtId < 4u; gtId++) {
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        // half dss disabled on tile 0
        if (gtId == 0) {
            drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0});
        } else {
            drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        }
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 64;
    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSlices);

    EXPECT_EQ(32, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(256, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEusPerSubSlice);

    // verify topology map
    constexpr uint32_t nTiles = 2;
    std::vector<int> expectedSubSliceIndices[nTiles];
    expectedSubSliceIndices[0].reserve(32u);
    for (auto i = 0u; i < 32; i++) {
        expectedSubSliceIndices[0].emplace_back(i);
    }

    expectedSubSliceIndices[1].reserve(64u);
    for (auto i = 0u; i < 64; i++) {
        expectedSubSliceIndices[1].emplace_back(i);
    }

    for (auto tileId : {0u, 1u}) {
        std::vector<int> expectedSliceIndices{0};

        ASSERT_EQ(expectedSliceIndices.size(), topologyMap[tileId].sliceIndices.size());
        ASSERT_TRUE(topologyMap[tileId].sliceIndices.size() > 0);

        for (auto i = 0u; i < expectedSliceIndices.size(); i++) {
            EXPECT_EQ(expectedSliceIndices[i], topologyMap[tileId].sliceIndices[i]);
        }

        ASSERT_EQ(expectedSubSliceIndices[tileId].size(), topologyMap[tileId].subsliceIndices.size());
        ASSERT_TRUE(topologyMap[tileId].subsliceIndices.size() > 0);

        for (auto i = 0u; i < expectedSubSliceIndices[tileId].size(); i++) {
            EXPECT_EQ(expectedSubSliceIndices[tileId][i], topologyMap[tileId].subsliceIndices[i]);
        }
    }
}

TEST_F(IoctlHelperXeTest, given2TileWithDisabledEvenDssAndComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    xeIoctlHelper->initialize();

    // even dss disabled
    constexpr uint8_t data = 0b1010'1010;

    for (auto gtId = 0u; gtId < 4u; gtId++) {
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {data, data, data, data, data, data, data, data});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 64;
    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSlices);

    EXPECT_EQ(32, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(256, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEusPerSubSlice);

    // verify topology map

    for (auto tileId : {0u, 1u}) {
        std::vector<int> expectedSliceIndices{0};

        ASSERT_EQ(expectedSliceIndices.size(), topologyMap[tileId].sliceIndices.size());
        ASSERT_TRUE(topologyMap[tileId].sliceIndices.size() > 0);

        for (auto i = 0u; i < expectedSliceIndices.size(); i++) {
            EXPECT_EQ(expectedSliceIndices[i], topologyMap[tileId].sliceIndices[i]);
        }

        std::vector<int> expectedSubSliceIndices;
        expectedSubSliceIndices.reserve(32u);
        for (auto i = 0u; i < 32; i++) {
            auto dssIndex = i * 2 + 1;
            expectedSubSliceIndices.emplace_back(dssIndex);
        }

        ASSERT_EQ(expectedSubSliceIndices.size(), topologyMap[tileId].subsliceIndices.size());
        ASSERT_TRUE(topologyMap[tileId].subsliceIndices.size() > 0);

        for (auto i = 0u; i < expectedSubSliceIndices.size(); i++) {
            EXPECT_EQ(expectedSubSliceIndices[i], topologyMap[tileId].subsliceIndices[i]);
        }
    }
}

TEST_F(IoctlHelperXeTest, givenMissingDssGeometryOrDssComputeInTopologyWhenGetTopologyDataAndMapThenReturnFalse) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    xeIoctlHelper->initialize();

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};
    uint16_t gtId = 0;

    drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    EXPECT_FALSE(result);

    drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
    result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    EXPECT_TRUE(result);

    drm->queryTopology.clear();
    drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
    EXPECT_TRUE(result);
}

TEST_F(IoctlHelperXeTest, givenMissingEuPerDssInTopologyWhenGetTopologyDataAndMapThenSetupEuCountToZeroAndReturnTrue) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    xeIoctlHelper->initialize();

    const auto &tileIdToGtId = xeIoctlHelper->tileIdToGtId;
    auto numTiles = tileIdToGtId.size();
    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    uint16_t incorrectFlagType = 128u;
    drm->addMockedQueryTopologyData(0, incorrectFlagType, 8, {0b11'1111, 0, 0, 0, 0, 0, 0, 0});
    drm->addMockedQueryTopologyData(1, incorrectFlagType, 8, {0, 0, 0, 0, 0, 0, 0, 0});
    drm->addMockedQueryTopologyData(2, incorrectFlagType, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});

    for (auto tileId = 0u; tileId < numTiles; tileId++) {
        drm->addMockedQueryTopologyData(tileIdToGtId[tileId], DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(tileIdToGtId[tileId], DRM_XE_TOPO_DSS_COMPUTE, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
    }
    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 16;
    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    EXPECT_TRUE(result);

    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSlices);

    EXPECT_EQ(16, topologyData.subSliceCount);
    EXPECT_EQ(16, topologyData.maxSubSlicesPerSlice);

    EXPECT_EQ(0, topologyData.euCount);
    EXPECT_EQ(0, topologyData.maxEusPerSubSlice);
}

TEST_F(IoctlHelperXeTest, whenCreatingEngineInfoThenProperEnginesAreDiscovered) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    auto baseCopyEngine = aub_stream::EngineType::ENGINE_BCS;
    auto nextCopyEngine = aub_stream::EngineType::ENGINE_BCS1;
    const auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getProductHelper();
    if (baseCopyEngine != productHelper.getDefaultCopyEngine()) {
        baseCopyEngine = aub_stream::EngineType::ENGINE_BCS1;
        nextCopyEngine = aub_stream::EngineType::ENGINE_BCS2;
    }

    for (const auto &isSysmanEnabled : ::testing::Bool()) {
        auto engineInfo = xeIoctlHelper->createEngineInfo(isSysmanEnabled);

        EXPECT_NE(nullptr, engineInfo);

        auto rcsEngineType = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, *executionEnvironment->rootDeviceEnvironments[0]);
        auto rcsEngine = engineInfo->getEngineInstance(0, rcsEngineType);
        EXPECT_NE(nullptr, rcsEngine);
        EXPECT_EQ(0, rcsEngine->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_RENDER), rcsEngine->engineClass);
        EXPECT_EQ(0u, engineInfo->getEngineTileIndex(*rcsEngine));

        EXPECT_EQ(nullptr, engineInfo->getEngineInstance(1, rcsEngineType));

        auto baseCopyEngineInstance = engineInfo->getEngineInstance(0, baseCopyEngine);
        EXPECT_NE(nullptr, baseCopyEngineInstance);
        EXPECT_EQ(1, baseCopyEngineInstance->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COPY), baseCopyEngineInstance->engineClass);
        EXPECT_EQ(0u, engineInfo->getEngineTileIndex(*baseCopyEngineInstance));

        EXPECT_EQ(nullptr, engineInfo->getEngineInstance(1, baseCopyEngine));

        auto nextCopyEngineInstance = engineInfo->getEngineInstance(0, nextCopyEngine);
        EXPECT_NE(nullptr, nextCopyEngineInstance);
        EXPECT_EQ(2, nextCopyEngineInstance->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COPY), nextCopyEngineInstance->engineClass);
        EXPECT_EQ(0u, engineInfo->getEngineTileIndex(*nextCopyEngineInstance));

        EXPECT_EQ(nullptr, engineInfo->getEngineInstance(1, nextCopyEngine));

        auto ccsEngine0 = engineInfo->getEngineInstance(0, aub_stream::EngineType::ENGINE_CCS);
        EXPECT_NE(nullptr, ccsEngine0);
        EXPECT_EQ(3, ccsEngine0->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COMPUTE), ccsEngine0->engineClass);
        EXPECT_EQ(0u, engineInfo->getEngineTileIndex(*ccsEngine0));

        auto ccsEngine1 = engineInfo->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS);
        EXPECT_NE(nullptr, ccsEngine1);
        EXPECT_EQ(5, ccsEngine1->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COMPUTE), ccsEngine1->engineClass);
        EXPECT_EQ(1u, engineInfo->getEngineTileIndex(*ccsEngine1));

        auto ccs1Engine0 = engineInfo->getEngineInstance(0, aub_stream::EngineType::ENGINE_CCS1);
        EXPECT_NE(nullptr, ccs1Engine0);
        EXPECT_EQ(4, ccs1Engine0->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COMPUTE), ccs1Engine0->engineClass);
        EXPECT_EQ(0u, engineInfo->getEngineTileIndex(*ccs1Engine0));

        auto ccs1Engine1 = engineInfo->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS1);
        EXPECT_NE(nullptr, ccs1Engine1);
        EXPECT_EQ(6, ccs1Engine1->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COMPUTE), ccs1Engine1->engineClass);
        EXPECT_EQ(1u, engineInfo->getEngineTileIndex(*ccs1Engine1));

        auto ccs1Engine2 = engineInfo->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS2);
        EXPECT_NE(nullptr, ccs1Engine2);
        EXPECT_EQ(7, ccs1Engine2->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COMPUTE), ccs1Engine2->engineClass);
        EXPECT_EQ(1u, engineInfo->getEngineTileIndex(*ccs1Engine2));

        auto ccs1Engine3 = engineInfo->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS3);
        EXPECT_NE(nullptr, ccs1Engine3);
        EXPECT_EQ(8, ccs1Engine3->engineInstance);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_COMPUTE), ccs1Engine3->engineClass);
        EXPECT_EQ(1u, engineInfo->getEngineTileIndex(*ccs1Engine3));

        std::vector<EngineClassInstance> enginesOnTile0;
        std::vector<EngineClassInstance> enginesOnTile1;
        engineInfo->getListOfEnginesOnATile(0, enginesOnTile0);
        engineInfo->getListOfEnginesOnATile(1, enginesOnTile1);

        for (const auto &engine : enginesOnTile0) {
            EXPECT_NE(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_VIDEO_DECODE), engine.engineClass);
        }

        bool foundVideDecodeEngine = false;
        for (const auto &engine : enginesOnTile1) {
            if (engine.engineClass == DRM_XE_ENGINE_CLASS_VIDEO_DECODE) {
                EXPECT_EQ(9, engine.engineInstance);
                foundVideDecodeEngine = true;
            }
        }
        EXPECT_EQ(isSysmanEnabled, foundVideDecodeEngine);

        bool foundVideoEnhanceEngine = false;
        for (const auto &engine : enginesOnTile0) {
            if (engine.engineClass == DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE) {
                EXPECT_EQ(10, engine.engineInstance);
                foundVideoEnhanceEngine = true;
            }
        }
        EXPECT_EQ(isSysmanEnabled, foundVideoEnhanceEngine);

        for (const auto &engine : enginesOnTile1) {
            EXPECT_NE(static_cast<uint16_t>(DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE), engine.engineClass);
        }

        if (isSysmanEnabled) {
            EXPECT_EQ(6u, enginesOnTile0.size());
            EXPECT_EQ(5u, enginesOnTile1.size());
        } else {
            EXPECT_EQ(5u, enginesOnTile0.size());
            EXPECT_EQ(4u, enginesOnTile1.size());
        }
    }
}

TEST_F(IoctlHelperXeTest, whenCreatingMemoryInfoThenProperMemoryBanksAreDiscovered) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    EXPECT_NE(nullptr, memoryInfo);

    auto memoryClassInstance0 = memoryInfo->getMemoryRegionClassAndInstance(0, *defaultHwInfo);
    EXPECT_EQ(static_cast<uint16_t>(DRM_XE_MEM_REGION_CLASS_SYSMEM), memoryClassInstance0.memoryClass);
    EXPECT_EQ(0u, memoryClassInstance0.memoryInstance);
    EXPECT_EQ(MemoryConstants::gigaByte, memoryInfo->getMemoryRegionSize(0));

    auto memoryClassInstance1 = memoryInfo->getMemoryRegionClassAndInstance(0b01, *defaultHwInfo);
    EXPECT_EQ(static_cast<uint16_t>(DRM_XE_MEM_REGION_CLASS_VRAM), memoryClassInstance1.memoryClass);
    EXPECT_EQ(1u, memoryClassInstance1.memoryInstance);
    EXPECT_EQ(2 * MemoryConstants::gigaByte, memoryInfo->getMemoryRegionSize(0b01));

    auto memoryClassInstance2 = memoryInfo->getMemoryRegionClassAndInstance(0b10, *defaultHwInfo);
    EXPECT_EQ(static_cast<uint16_t>(DRM_XE_MEM_REGION_CLASS_VRAM), memoryClassInstance2.memoryClass);
    EXPECT_EQ(2u, memoryClassInstance2.memoryInstance);
    EXPECT_EQ(4 * MemoryConstants::gigaByte, memoryInfo->getMemoryRegionSize(0b10));

    auto &memoryRegions = memoryInfo->getDrmRegionInfos();
    EXPECT_EQ(3u, memoryRegions.size());

    EXPECT_EQ(0u, memoryRegions[0].region.memoryInstance);
    EXPECT_EQ(MemoryConstants::gigaByte, memoryRegions[0].probedSize);
    EXPECT_EQ(MemoryConstants::gigaByte - MemoryConstants::kiloByte, memoryRegions[0].unallocatedSize);

    EXPECT_EQ(2u, memoryRegions[2].region.memoryInstance);
    EXPECT_EQ(4 * MemoryConstants::gigaByte, memoryRegions[2].probedSize);
    EXPECT_EQ(3 * MemoryConstants::gigaByte, memoryRegions[2].unallocatedSize);

    EXPECT_EQ(1u, memoryRegions[1].region.memoryInstance);
    EXPECT_EQ(2 * MemoryConstants::gigaByte, memoryRegions[1].probedSize);
    EXPECT_EQ(2 * MemoryConstants::gigaByte - MemoryConstants::megaByte, memoryRegions[1].unallocatedSize);
}

TEST_F(IoctlHelperXeTest, givenXeDrmMemoryManagerWhenGetLocalMemorySizeIsCalledThenReturnMemoryRegionSizeCorrectly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    constexpr auto tileCount{3u};

    auto executionEnvironment{std::make_unique<MockExecutionEnvironment>()};
    auto &rootDeviceEnv{*executionEnvironment->rootDeviceEnvironments[0]};
    auto *hwInfo = rootDeviceEnv.getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = true;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
    rootDeviceEnv.osInterface.reset(new OSInterface{});
    rootDeviceEnv.osInterface->setDriverModel(DrmMockXe::create(rootDeviceEnv));

    auto *drm{rootDeviceEnv.osInterface->getDriverModel()->as<DrmMockXe>()};

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);
    xeIoctlHelper->initialize();
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    std::vector<size_t> lmemSizes{};
    for (const auto &region : memoryInfo->getLocalMemoryRegions()) {
        lmemSizes.push_back(region.probedSize);
    }

    drm->memoryInfo.reset(memoryInfo.release());
    drm->ioctlHelper.reset(xeIoctlHelper.release());

    TestedDrmMemoryManager memoryManager(*executionEnvironment);
    uint32_t tileMask = static_cast<uint32_t>(maxNBitValue(tileCount));
    EXPECT_EQ(memoryManager.getLocalMemorySize(0, tileMask), lmemSizes[0] + lmemSizes[1]);
    tileMask = static_cast<uint32_t>(0b01);
    EXPECT_EQ(memoryManager.getLocalMemorySize(0, tileMask), lmemSizes[1]);
    tileMask = static_cast<uint32_t>(0b10);
    EXPECT_EQ(memoryManager.getLocalMemorySize(0, tileMask), lmemSizes[0]);
    tileMask = static_cast<uint32_t>(0b100);
    EXPECT_EQ(memoryManager.getLocalMemorySize(0, tileMask), lmemSizes[1]);
}

TEST_F(IoctlHelperXeTest, givenIoctlFailureWhenCreatingMemoryInfoThenNoMemoryBanksAreDiscovered) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    drm->testMode(1, -1);
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    EXPECT_EQ(nullptr, memoryInfo);
}

TEST_F(IoctlHelperXeTest, givenNoMemoryRegionsWhenCreatingMemoryInfoThenMemoryInfoIsNotCreated) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(drm->queryMemUsage);
    xeQueryMemUsage->num_mem_regions = 0u;
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    EXPECT_EQ(nullptr, memoryInfo);
}

TEST_F(IoctlHelperXeTest, givenIoctlFailureWhenCreatingEngineInfoThenNoEnginesAreDiscovered) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    drm->testMode(1, -1);
    auto engineInfo = xeIoctlHelper->createEngineInfo(true);
    EXPECT_EQ(nullptr, engineInfo);
}

TEST_F(IoctlHelperXeTest, givenEnabledFtrMultiTileArchWhenCreatingEngineInfoThenMultiTileArchInfoIsProperlySet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto drm = DrmMockXe::create(rootDeviceEnvironment);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    for (const auto &isSysmanEnabled : ::testing::Bool()) {
        hwInfo->gtSystemInfo.MultiTileArchInfo = {};
        hwInfo->featureTable.flags.ftrMultiTileArch = true;
        auto engineInfo = xeIoctlHelper->createEngineInfo(isSysmanEnabled);
        EXPECT_NE(nullptr, engineInfo);

        EXPECT_TRUE(hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid);
        EXPECT_EQ(2u, hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount);
        EXPECT_EQ(0b11u, hwInfo->gtSystemInfo.MultiTileArchInfo.TileMask);
    }
}

TEST_F(IoctlHelperXeTest, givenDisabledFtrMultiTileArchWhenCreatingEngineInfoThenMultiTileArchInfoIsNotSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto drm = DrmMockXe::create(rootDeviceEnvironment);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    for (const auto &isSysmanEnabled : ::testing::Bool()) {
        hwInfo->gtSystemInfo.MultiTileArchInfo = {};
        hwInfo->featureTable.flags.ftrMultiTileArch = false;
        auto engineInfo = xeIoctlHelper->createEngineInfo(isSysmanEnabled);
        EXPECT_NE(nullptr, engineInfo);

        EXPECT_FALSE(hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid);
        EXPECT_EQ(0u, hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount);
        EXPECT_EQ(0u, hwInfo->gtSystemInfo.MultiTileArchInfo.TileMask);
    }
}

using IoctlHelperXeFenceWaitTest = ::testing::Test;

TEST_F(IoctlHelperXeFenceWaitTest, whenCallingVmBindThenWaitUserFenceIsCalled) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = 0x1234;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm->vmBindInputs.clear();
    drm->syncInputs.clear();
    drm->waitUserFenceInputs.clear();

    EXPECT_EQ(0u, vmBindParams.flags);
    vmBindParams.flags = 0x12345; // set non-zero to check if flags are passed
    auto expectedFlags = vmBindParams.flags;
    EXPECT_EQ(0, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(1u, drm->vmBindInputs.size());
    EXPECT_EQ(1u, drm->syncInputs.size());
    EXPECT_EQ(1u, drm->waitUserFenceInputs.size());
    auto expectedMask = std::numeric_limits<uint64_t>::max();
    auto expectedTimeout = 1000000000ll;
    {
        auto &sync = drm->syncInputs[0];

        EXPECT_EQ(fenceAddress, sync.addr);
        EXPECT_EQ(fenceValue, sync.timeline_value);

        auto &waitUserFence = drm->waitUserFenceInputs[0];

        EXPECT_EQ(fenceAddress, waitUserFence.addr);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_UFENCE_WAIT_OP_EQ), waitUserFence.op);
        EXPECT_EQ(0u, waitUserFence.flags);
        EXPECT_EQ(fenceValue, waitUserFence.value);
        EXPECT_EQ(expectedMask, waitUserFence.mask);
        EXPECT_EQ(expectedTimeout, waitUserFence.timeout);
        EXPECT_EQ(0u, waitUserFence.exec_queue_id);

        EXPECT_EQ(expectedFlags, drm->vmBindInputs[0].bind.flags);
    }
    drm->vmBindInputs.clear();
    drm->syncInputs.clear();
    drm->waitUserFenceInputs.clear();
    EXPECT_EQ(0, xeIoctlHelper->vmUnbind(vmBindParams));
    EXPECT_EQ(1u, drm->vmBindInputs.size());
    EXPECT_EQ(1u, drm->syncInputs.size());
    EXPECT_EQ(1u, drm->waitUserFenceInputs.size());
    {
        auto &sync = drm->syncInputs[0];

        EXPECT_EQ(fenceAddress, sync.addr);
        EXPECT_EQ(fenceValue, sync.timeline_value);

        auto &waitUserFence = drm->waitUserFenceInputs[0];

        EXPECT_EQ(fenceAddress, waitUserFence.addr);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_UFENCE_WAIT_OP_EQ), waitUserFence.op);
        EXPECT_EQ(0u, waitUserFence.flags);
        EXPECT_EQ(fenceValue, waitUserFence.value);
        EXPECT_EQ(expectedMask, waitUserFence.mask);
        EXPECT_EQ(expectedTimeout, waitUserFence.timeout);
        EXPECT_EQ(0u, waitUserFence.exec_queue_id);

        EXPECT_EQ(expectedFlags, drm->vmBindInputs[0].bind.flags);
    }
}

TEST_F(IoctlHelperXeTest, givenVmBindWaitUserFenceTimeoutWhenCallingVmBindThenWaitUserFenceIsCalledWithSpecificTimeout) {
    DebugManagerStateRestore restorer;
    debugManager.flags.VmBindWaitUserFenceTimeout.set(5000000000ll);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = 0x1234;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm->vmBindInputs.clear();
    drm->syncInputs.clear();
    drm->waitUserFenceInputs.clear();

    EXPECT_EQ(0u, vmBindParams.flags);
    vmBindParams.flags = 0x12345; // set non-zero to check if flags are passed
    auto expectedFlags = vmBindParams.flags;
    EXPECT_EQ(0, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(1u, drm->vmBindInputs.size());
    EXPECT_EQ(1u, drm->syncInputs.size());
    EXPECT_EQ(1u, drm->waitUserFenceInputs.size());
    auto expectedMask = std::numeric_limits<uint64_t>::max();
    auto expectedTimeout = 5000000000ll;
    {
        auto &sync = drm->syncInputs[0];

        EXPECT_EQ(fenceAddress, sync.addr);
        EXPECT_EQ(fenceValue, sync.timeline_value);

        auto &waitUserFence = drm->waitUserFenceInputs[0];

        EXPECT_EQ(fenceAddress, waitUserFence.addr);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_UFENCE_WAIT_OP_EQ), waitUserFence.op);
        EXPECT_EQ(0u, waitUserFence.flags);
        EXPECT_EQ(fenceValue, waitUserFence.value);
        EXPECT_EQ(expectedMask, waitUserFence.mask);
        EXPECT_EQ(expectedTimeout, waitUserFence.timeout);
        EXPECT_EQ(0u, waitUserFence.exec_queue_id);

        EXPECT_EQ(expectedFlags, drm->vmBindInputs[0].bind.flags);
    }
}

TEST_F(IoctlHelperXeTest, whenGemVmBindFailsThenErrorIsPropagated) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = 0x1234;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm->waitUserFenceInputs.clear();

    int errorValue = -1;
    drm->gemVmBindReturn = errorValue;
    EXPECT_EQ(errorValue, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(0u, drm->waitUserFenceInputs.size());

    EXPECT_EQ(errorValue, xeIoctlHelper->vmUnbind(vmBindParams));
    EXPECT_EQ(0u, drm->waitUserFenceInputs.size());
}

TEST_F(IoctlHelperXeTest, whenUserFenceFailsThenErrorIsPropagated) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = 0x1234;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm->waitUserFenceInputs.clear();

    int errorValue = -1;
    drm->waitUserFenceReturn = errorValue;

    EXPECT_EQ(errorValue, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(errorValue, xeIoctlHelper->vmUnbind(vmBindParams));
}

TEST_F(IoctlHelperXeTest, WhenSetupIpVersionIsCalledThenIpVersionIsCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto &compilerProductHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    xeIoctlHelper->setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

TEST_F(IoctlHelperXeTest, whenXeShowBindTableIsCalledThenBindLogsArePrinted) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    BindInfo mockBindInfo{};
    mockBindInfo.userptr = 2u;
    mockBindInfo.addr = 3u;
    xeIoctlHelper->bindInfo.push_back(mockBindInfo);

    ::testing::internal::CaptureStderr();

    debugManager.flags.PrintXeLogs.set(true);
    xeIoctlHelper->xeShowBindTable();
    debugManager.flags.PrintXeLogs.set(false);

    std::string output = testing::internal::GetCapturedStderr();
    std::string expectedOutput1 = "show bind: (<index> <userptr> <addr>)\n";
    std::string expectedOutput2 = "0 x0000000000000002 x0000000000000003";

    EXPECT_NE(std::string::npos, output.find(expectedOutput1));
    EXPECT_NE(std::string::npos, output.find(expectedOutput2, expectedOutput1.size())) << output;
}

TEST_F(IoctlHelperXeTest, givenIoctlFailureWhenSetGpuCpuTimesIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXe::create(rootDeviceEnvironment);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    drm->testMode(1, -1);
    TimeStampData pGpuCpuTime{};
    std::unique_ptr<MockOSTimeLinux> osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    auto ret = xeIoctlHelper->setGpuCpuTimes(&pGpuCpuTime, osTime.get());
    EXPECT_EQ(false, ret);
}

TEST_F(IoctlHelperXeTest, givenIoctlFailureWhenSetGpuCpuTimesIsCalledThenProperValuesAreSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXe::create(rootDeviceEnvironment);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    uint64_t expectedCycles = 100000;
    uint64_t expectedTimestamp = 100;
    auto xeQueryEngineCycles = reinterpret_cast<drm_xe_query_engine_cycles *>(drm->queryEngineCycles);
    xeQueryEngineCycles->width = 32;
    xeQueryEngineCycles->engine_cycles = expectedCycles;
    xeQueryEngineCycles->cpu_timestamp = expectedTimestamp;

    TimeStampData pGpuCpuTime{};
    std::unique_ptr<MockOSTimeLinux> osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    auto ret = xeIoctlHelper->setGpuCpuTimes(&pGpuCpuTime, osTime.get());
    EXPECT_EQ(true, ret);
    EXPECT_EQ(pGpuCpuTime.gpuTimeStamp, expectedCycles);
    EXPECT_EQ(pGpuCpuTime.cpuTimeinNS, expectedTimestamp);
}

TEST_F(IoctlHelperXeTest, whenSetDefaultEngineIsCalledThenProperEngineIsSet) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(&hwInfo);
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();
    auto engineInfo = xeIoctlHelper->createEngineInfo(true);
    ASSERT_NE(nullptr, engineInfo);

    EXPECT_EQ(DRM_XE_ENGINE_CLASS_COMPUTE, xeIoctlHelper->getDefaultEngineClass(aub_stream::EngineType::ENGINE_CCS));
    EXPECT_EQ(DRM_XE_ENGINE_CLASS_RENDER, xeIoctlHelper->getDefaultEngineClass(aub_stream::EngineType::ENGINE_RCS));

    EXPECT_THROW(xeIoctlHelper->getDefaultEngineClass(aub_stream::EngineType::ENGINE_BCS), std::exception);
    EXPECT_THROW(xeIoctlHelper->getDefaultEngineClass(aub_stream::EngineType::ENGINE_VCS), std::exception);
    EXPECT_THROW(xeIoctlHelper->getDefaultEngineClass(aub_stream::EngineType::ENGINE_VECS), std::exception);
}

TEST_F(IoctlHelperXeTest, whenGettingPreemptionSupportThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    EXPECT_TRUE(xeIoctlHelper->isPreemptionSupported());
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenGetCpuCachingModeCalledThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(false, false), DRM_XE_GEM_CPU_CACHING_WC);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(false, true), DRM_XE_GEM_CPU_CACHING_WC);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(true, true), DRM_XE_GEM_CPU_CACHING_WB);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(true, false), DRM_XE_GEM_CPU_CACHING_WC);

    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(std::nullopt, false), DRM_XE_GEM_CPU_CACHING_WC);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(std::nullopt, true), DRM_XE_GEM_CPU_CACHING_WB);

    debugManager.flags.OverrideCpuCaching.set(DRM_XE_GEM_CPU_CACHING_WB);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(false, false), DRM_XE_GEM_CPU_CACHING_WB);
}

TEST_F(IoctlHelperXeTest, whenCallingVmBindThenPatIndexIsSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;
    uint64_t expectedPatIndex = 0xba;

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = 0x1234;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);
    vmBindParams.patIndex = expectedPatIndex;

    drm->vmBindInputs.clear();
    drm->syncInputs.clear();
    drm->waitUserFenceInputs.clear();
    ASSERT_EQ(0, xeIoctlHelper->vmBind(vmBindParams));
    ASSERT_EQ(1u, drm->vmBindInputs.size());

    EXPECT_EQ(drm->vmBindInputs[0].bind.pat_index, expectedPatIndex);
}

TEST_F(IoctlHelperXeTest, whenBindingDrmContextWithoutVirtualEnginesThenProperEnginesAreSelected) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    ioctlHelper->initialize();
    drm->memoryInfoQueried = true;
    drm->queryEngineInfo();

    unsigned int expectedValue = DRM_XE_ENGINE_CLASS_COMPUTE;
    uint16_t tileId = 1u;
    uint16_t expectedGtId = 2u;

    EXPECT_EQ(expectedValue, drm->bindDrmContext(0, tileId, aub_stream::EngineType::ENGINE_CCS));

    EXPECT_EQ(4u, ioctlHelper->contextParamEngine.size());
    auto expectedEngine = drm->getEngineInfo()->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS);
    auto notExpectedEngine = drm->getEngineInfo()->getEngineInstance(0, aub_stream::EngineType::ENGINE_CCS);
    EXPECT_NE(expectedEngine->engineInstance, notExpectedEngine->engineInstance);
    EXPECT_EQ(expectedEngine->engineInstance, ioctlHelper->contextParamEngine[0].engine_instance);
    EXPECT_EQ(expectedEngine->engineClass, ioctlHelper->contextParamEngine[0].engine_class);
    EXPECT_EQ(expectedGtId, ioctlHelper->contextParamEngine[0].gt_id);
}

TEST_F(IoctlHelperXeTest, whenBindingDrmContextWithVirtualEnginesThenProperEnginesAreSelected) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    ioctlHelper->initialize();
    drm->memoryInfoQueried = true;
    drm->queryEngineInfo();
    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;

    unsigned int expectedValue = DRM_XE_ENGINE_CLASS_COMPUTE;
    uint16_t tileId = 1u;
    uint16_t expectedGtId = 2u;
    EXPECT_EQ(expectedValue, drm->bindDrmContext(0, tileId, aub_stream::EngineType::ENGINE_CCS));

    EXPECT_EQ(2u, ioctlHelper->contextParamEngine.size());
    {
        auto expectedEngine = drm->getEngineInfo()->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS);
        auto notExpectedEngine = drm->getEngineInfo()->getEngineInstance(0, aub_stream::EngineType::ENGINE_CCS);
        EXPECT_NE(expectedEngine->engineInstance, notExpectedEngine->engineInstance);
        EXPECT_EQ(expectedEngine->engineInstance, ioctlHelper->contextParamEngine[0].engine_instance);
        EXPECT_EQ(expectedEngine->engineClass, ioctlHelper->contextParamEngine[0].engine_class);
        EXPECT_EQ(expectedGtId, ioctlHelper->contextParamEngine[0].gt_id);
    }
    {
        auto expectedEngine = drm->getEngineInfo()->getEngineInstance(1, aub_stream::EngineType::ENGINE_CCS1);
        auto notExpectedEngine = drm->getEngineInfo()->getEngineInstance(0, aub_stream::EngineType::ENGINE_CCS1);
        EXPECT_NE(expectedEngine->engineInstance, notExpectedEngine->engineInstance);
        EXPECT_EQ(expectedEngine->engineInstance, ioctlHelper->contextParamEngine[1].engine_instance);
        EXPECT_EQ(expectedEngine->engineClass, ioctlHelper->contextParamEngine[1].engine_class);
        EXPECT_EQ(expectedGtId, ioctlHelper->contextParamEngine[1].gt_id);
    }
}

TEST_F(IoctlHelperXeTest, whenCallingGetResetStatsThenSuccessIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    ASSERT_NE(nullptr, xeIoctlHelper);

    xeIoctlHelper->initialize();
    drm->memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());

    ResetStats resetStats{};
    resetStats.contextId = 0;

    EXPECT_EQ(0, xeIoctlHelper->getResetStats(resetStats, nullptr, nullptr));
}

TEST_F(IoctlHelperXeTest, whenCallingGetStatusAndFlagsForResetStatsThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    EXPECT_EQ(0u, ioctlHelper->getStatusForResetStats(true));
    EXPECT_EQ(0u, ioctlHelper->getStatusForResetStats(false));

    EXPECT_FALSE(ioctlHelper->validPageFault(0u));
}

TEST_F(IoctlHelperXeTest, whenInitializeThenProperHwInfoIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->capabilityTable.gpuAddressSpace = 0;

    ioctlHelper->initialize();

    EXPECT_EQ((1ull << 48) - 1, hwInfo->capabilityTable.gpuAddressSpace);
    EXPECT_EQ(static_cast<uint32_t>(DrmMockXe::mockDefaultCxlType), hwInfo->capabilityTable.cxlType);

    EXPECT_EQ(DrmMockXe::mockMaxExecQueuePriority, ioctlHelper->maxExecQueuePriority);
}

TEST_F(IoctlHelperXeTest, givenMultipleBindInfosWhenGemCloseIsCalledThenProperHandleIsTaken) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    drm->gemCloseCalled = 0;

    BindInfo bindInfo{};
    bindInfo = {};
    bindInfo.userptr = 1;
    ioctlHelper->bindInfo.push_back(bindInfo);
    bindInfo.userptr = 2;
    ioctlHelper->bindInfo.push_back(bindInfo);

    GemClose gemClose{};
    gemClose.handle = 0;
    gemClose.userptr = 2;
    EXPECT_EQ(0, ioctlHelper->ioctl(DrmIoctl::gemClose, &gemClose));
    EXPECT_EQ(0, drm->gemCloseCalled);

    gemClose.handle = 2;
    gemClose.userptr = 0;
    EXPECT_EQ(0, ioctlHelper->ioctl(DrmIoctl::gemClose, &gemClose));
    EXPECT_EQ(1, drm->gemCloseCalled);
    EXPECT_EQ(2u, drm->passedGemClose.handle);

    gemClose.handle = 0;
    gemClose.userptr = 1;
    EXPECT_EQ(0, ioctlHelper->ioctl(DrmIoctl::gemClose, &gemClose));
    EXPECT_EQ(1, drm->gemCloseCalled);

    gemClose.handle = 1;
    gemClose.userptr = 0;
    EXPECT_EQ(0, ioctlHelper->ioctl(DrmIoctl::gemClose, &gemClose));
    EXPECT_EQ(2, drm->gemCloseCalled);
    EXPECT_EQ(1u, drm->passedGemClose.handle);

    EXPECT_EQ(0ul, ioctlHelper->bindInfo.size());
}

TEST_F(IoctlHelperXeTest, givenMultipleBindInfosWhenVmBindIsCalledThenProperHandleIsTaken) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    drm->vmBindInputs.clear();

    BindInfo bindInfo{};
    bindInfo.userptr = 1;
    ioctlHelper->bindInfo.push_back(bindInfo);
    bindInfo.userptr = 2;
    ioctlHelper->bindInfo.push_back(bindInfo);

    MockIoctlHelperXe::UserFenceExtension userFence{};
    userFence.tag = userFence.tagValue;
    userFence.addr = 0x1;
    VmBindParams vmBindParams{};
    vmBindParams.userFence = castToUint64(&userFence);
    vmBindParams.handle = 0;
    vmBindParams.userptr = 2;
    EXPECT_EQ(0, ioctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(1ul, drm->vmBindInputs.size());
    EXPECT_EQ(0u, drm->vmBindInputs[0].bind.obj);
    EXPECT_EQ(2u, drm->vmBindInputs[0].bind.obj_offset);
    drm->vmBindInputs.clear();

    vmBindParams.handle = 2;
    vmBindParams.userptr = 0;
    EXPECT_EQ(0, ioctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(1ul, drm->vmBindInputs.size());
    EXPECT_EQ(2u, drm->vmBindInputs[0].bind.obj);
    EXPECT_EQ(0u, drm->vmBindInputs[0].bind.obj_offset);
    drm->vmBindInputs.clear();

    vmBindParams.handle = 0;
    vmBindParams.userptr = 1;
    EXPECT_EQ(0, ioctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(1ul, drm->vmBindInputs.size());
    EXPECT_EQ(0u, drm->vmBindInputs[0].bind.obj);
    EXPECT_EQ(1u, drm->vmBindInputs[0].bind.obj_offset);
    drm->vmBindInputs.clear();

    vmBindParams.handle = 1;
    vmBindParams.userptr = 0;
    EXPECT_EQ(0, ioctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(1ul, drm->vmBindInputs.size());
    EXPECT_EQ(1u, drm->vmBindInputs[0].bind.obj);
    EXPECT_EQ(0u, drm->vmBindInputs[0].bind.obj_offset);
    drm->vmBindInputs.clear();

    vmBindParams.handle = 0;
    vmBindParams.userptr = 0;
    EXPECT_EQ(0, ioctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(1ul, drm->vmBindInputs.size());

    ioctlHelper->bindInfo.clear();
}

TEST_F(IoctlHelperXeTest, givenLowPriorityContextWhenSettingPropertiesThenCorrectIndexIsUsedAndReturend) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::lowPriority}));
    std::array<drm_xe_ext_set_property, MockIoctlHelperXe::maxContextSetProperties> extProperties{};
    uint32_t extIndex = 1;
    xeIoctlHelper->setContextProperties(osContext, 0, &extProperties, extIndex);

    EXPECT_EQ(reinterpret_cast<uint64_t>(&extProperties[1]), extProperties[0].base.next_extension);
    EXPECT_EQ(0u, extProperties[1].base.next_extension);
    EXPECT_EQ(2u, extIndex);
}

TEST_F(IoctlHelperXeTest, givenLowPriorityContextWhenCreatingDrmContextThenExtPropertyIsSetCorrectly) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlHelper->initialize();
    drm->memoryInfoQueried = true;
    drm->queryEngineInfo();
    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::lowPriority}));
    osContext.ensureContextInitialized(false);

    ASSERT_LE(1u, drm->execQueueProperties.size());
    EXPECT_EQ(static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_SET_PROPERTY_PRIORITY), drm->execQueueProperties[0].property);
    EXPECT_EQ(0u, drm->execQueueProperties[0].value);
}

TEST_F(IoctlHelperXeTest, whenInitializeThenGtListDataIsQueried) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    EXPECT_EQ(nullptr, ioctlHelper->xeGtListData);
    EXPECT_TRUE(ioctlHelper->queryGtListData.empty());

    EXPECT_TRUE(ioctlHelper->initialize());

    EXPECT_NE(nullptr, ioctlHelper->xeGtListData);
    EXPECT_FALSE(ioctlHelper->queryGtListData.empty());
    EXPECT_EQ(castToUint64(ioctlHelper->queryGtListData.data()), castToUint64(ioctlHelper->xeGtListData));
    EXPECT_EQ(drm->queryGtList.size(), ioctlHelper->queryGtListData.size());
}

TEST_F(IoctlHelperXeTest, givenNoGtListDataWhenInitializeThenInitializationFails) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    drm->queryGtList.resize(0);

    EXPECT_FALSE(ioctlHelper->initialize());
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetFlagsForVmBindThenExpectedValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(*drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                for (auto &bindLockedMemory : ::testing::Bool()) {
                    for (auto &readOnlyResource : ::testing::Bool()) {
                        auto flags = xeIoctlHelper->getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLockedMemory, readOnlyResource);
                        if (bindCapture) {
                            EXPECT_EQ(static_cast<uint32_t>(DRM_XE_VM_BIND_FLAG_DUMPABLE), (flags & DRM_XE_VM_BIND_FLAG_DUMPABLE));
                        }
                        if (flags == 0) {
                            EXPECT_FALSE(bindCapture);
                        }
                    }
                }
            }
        }
    }
}

TEST_F(IoctlHelperXeTest, whenGetFdFromVmExportIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    uint32_t vmId = 0, flags = 0;
    int32_t fd = 0;
    EXPECT_FALSE(xeIoctlHelper->getFdFromVmExport(vmId, flags, &fd));
}

TEST_F(IoctlHelperXeTest, whenCheckingGpuHangThenBanPropertyIsQueried) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);

    MockOsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    osContext.drmContextIds.push_back(0);
    drm->execQueueBanPropertyReturn = 0;
    EXPECT_FALSE(drm->checkResetStatus(osContext));
    EXPECT_FALSE(osContext.isHangDetected());

    drm->execQueueBanPropertyReturn = 1;
    EXPECT_TRUE(drm->checkResetStatus(osContext));
    EXPECT_TRUE(osContext.isHangDetected());
}

TEST_F(IoctlHelperXeTest, givenImmediateAndReadOnlyBindFlagsSupportedWhenGettingFlagsForVmBindThenCorrectFlagsAreReturned) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    for (const auto &bindImmediateSupport : ::testing::Bool()) {
        for (const auto &bindReadOnlySupport : ::testing::Bool()) {
            for (const auto &bindMakeResidentSupport : ::testing::Bool()) {
                uint64_t expectedFlags = DRM_XE_VM_BIND_FLAG_DUMPABLE;

                if (bindImmediateSupport) {
                    expectedFlags |= DRM_XE_VM_BIND_FLAG_IMMEDIATE;
                }
                if (bindReadOnlySupport) {
                    expectedFlags |= DRM_XE_VM_BIND_FLAG_READONLY;
                }
                if (bindMakeResidentSupport) {
                    expectedFlags |= DRM_XE_VM_BIND_FLAG_IMMEDIATE;
                }

                auto bindFlags = xeIoctlHelper->getFlagsForVmBind(true, bindImmediateSupport, bindMakeResidentSupport, false, bindReadOnlySupport);

                EXPECT_EQ(expectedFlags, bindFlags);
            }
        }
    }
}

struct DrmMockXePageFault : public DrmMockXe {
    static auto create(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto drm = std::unique_ptr<DrmMockXePageFault>(new DrmMockXePageFault{rootDeviceEnvironment});
        drm->initInstance();
        return drm;
    }

    int ioctl(DrmIoctl request, void *arg) override {
        if (supportsRecoverablePageFault) {
            return 0;
        }
        return -EINVAL;
    };
    bool supportsRecoverablePageFault = true;

  protected:
    // Don't call directly, use the create() function
    DrmMockXePageFault(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockXe(rootDeviceEnvironment) {}
};

TEST_F(IoctlHelperXeTest, givenPageFaultSupportIsSetWhenCallingIsPageFaultSupportedThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXePageFault::create(*executionEnvironment->rootDeviceEnvironments[0]);

    for (const auto &recoverablePageFault : ::testing::Bool()) {
        drm->supportsRecoverablePageFault = recoverablePageFault;
        auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);
        xeIoctlHelper->initialize();
        EXPECT_EQ(xeIoctlHelper->isPageFaultSupported(), recoverablePageFault);
    }
}

struct HwIpVersionFixture {

    HwIpVersionFixture() : hwInfo{*executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()},
                           drm{DrmMockXe::create(*executionEnvironment.rootDeviceEnvironments[0])} {}

    void setUp() {
        mockGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm->queryGtList.begin());
        mockGtList->gt_list[0].ip_ver_major = 0x2a5;
        mockGtList->gt_list[0].ip_ver_minor = 0xc3;
        mockGtList->gt_list[0].ip_ver_rev = 0x2f;
        mockGtList->gt_list[0].pad2 = 0xffff;
        mockGtList->gt_list[1].ip_ver_major = 0x2a6;
        mockGtList->gt_list[1].ip_ver_minor = 0xc4;
        mockGtList->gt_list[1].ip_ver_rev = 0x2e;
        mockGtList->gt_list[1].pad2 = 0xffff;
        mockGtList->gt_list[2].ip_ver_major = 0x2a7;
        mockGtList->gt_list[2].ip_ver_minor = 0xc5;
        mockGtList->gt_list[2].ip_ver_rev = 0x2d;
        mockGtList->gt_list[2].pad2 = 0xffff;
        mockGtList->gt_list[3].ip_ver_major = 0x2a8;
        mockGtList->gt_list[3].ip_ver_minor = 0xc6;
        mockGtList->gt_list[3].ip_ver_rev = 0x2c;
        mockGtList->gt_list[3].pad2 = 0xffff;
    }
    void tearDown() {}

    MockExecutionEnvironment executionEnvironment{};
    const HardwareInfo &hwInfo;
    std::unique_ptr<DrmMockXe> drm;
    drm_xe_query_gt_list *mockGtList{nullptr};
};
using IoctlHelperXeHwIpVersionTests = Test<HwIpVersionFixture>;

TEST_F(IoctlHelperXeHwIpVersionTests, WhenSetupIpVersionIsCalledAndGtListProvidesProperDataThenIpVersionIsTakenFromGtList) {
    EXPECT_EQ(mockGtList->gt_list[0].type, DRM_XE_QUERY_GT_TYPE_MAIN);

    NEO::HardwareIpVersion testHwIpVersion{};
    testHwIpVersion.architecture = mockGtList->gt_list[0].ip_ver_major;
    testHwIpVersion.release = mockGtList->gt_list[0].ip_ver_minor;
    testHwIpVersion.revision = mockGtList->gt_list[0].ip_ver_rev;

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);
    xeIoctlHelper->setupIpVersion();
    EXPECT_EQ(testHwIpVersion.value, hwInfo.ipVersion.value);
}

TEST_F(IoctlHelperXeHwIpVersionTests, WhenSetupIpVersionIsCalledAndGtListHasOnlyMediaEntriesThenIpVersionFallsBackToDefault) {
    for (size_t i = 0; i < mockGtList->num_gt; i++) {
        mockGtList->gt_list[i].type = DRM_XE_QUERY_GT_TYPE_MEDIA;
    }
    auto &compilerProductHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);
    xeIoctlHelper->setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

TEST_F(IoctlHelperXeHwIpVersionTests, WhenSetupIpVersionIsCalledAndGtListEntryHasZeroMajorVersionThenThisEntryIsConsideredInvalid) {
    EXPECT_EQ(mockGtList->gt_list[0].type, DRM_XE_QUERY_GT_TYPE_MAIN);
    EXPECT_EQ(mockGtList->gt_list[2].type, DRM_XE_QUERY_GT_TYPE_MAIN);
    mockGtList->gt_list[0].ip_ver_major = static_cast<uint16_t>(0u);

    NEO::HardwareIpVersion testHwIpVersion{};
    testHwIpVersion.architecture = mockGtList->gt_list[2].ip_ver_major;
    testHwIpVersion.release = mockGtList->gt_list[2].ip_ver_minor;
    testHwIpVersion.revision = mockGtList->gt_list[2].ip_ver_rev;

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);
    xeIoctlHelper->setupIpVersion();
    EXPECT_EQ(testHwIpVersion.value, hwInfo.ipVersion.value);
}

TEST_F(IoctlHelperXeHwIpVersionTests, WhenSetupIpVersionIsCalledAndIoctlReturnsNoDataThenIpVersionFallsBackToDefault) {
    drm->testMode(true, 0);
    auto &compilerProductHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(*drm);

    xeIoctlHelper->setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperWhenSettingExtContextThenCallExternalIoctlFunction) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    bool ioctlCalled = false;
    ResetStats resetStats{};
    EXPECT_TRUE(ioctlHelper.ioctl(DrmIoctl::getResetStats, &resetStats));
    EXPECT_FALSE(ioctlCalled);

    int handle = 0;
    IoctlFunc ioctl = [&](void *, int, unsigned long int, void *, bool) { ioctlCalled=true; return 0; };
    ExternalCtx ctx{&handle, ioctl};

    ioctlHelper.setExternalContext(&ctx);
    ioctlCalled = false;
    EXPECT_EQ(0, ioctlHelper.ioctl(DrmIoctl::getResetStats, &resetStats));
    EXPECT_TRUE(ioctlCalled);

    ioctlHelper.setExternalContext(nullptr);
    ioctlCalled = false;
    EXPECT_TRUE(ioctlHelper.ioctl(DrmIoctl::getResetStats, &resetStats));
    EXPECT_FALSE(ioctlCalled);
}
TEST_F(IoctlHelperXeTest, givenL3BankWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

    xeIoctlHelper->initialize();

    for (auto gtId = 0u; gtId < 4u; gtId++) {
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0b11'1111, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
        drm->addMockedQueryTopologyData(gtId, DRM_XE_TOPO_L3_BANK, 8, {0b1111'0011, 0b1001'1111, 0, 0, 0, 0, 0, 0});
    }
    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(12, topologyData.numL3Banks);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperWhenGettingFenceAddressThenReturnCorrectValue) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    auto fenceAddr = ioctlHelper.getPagingFenceAddress(0, nullptr);
    EXPECT_EQ(drm->getFenceAddr(0), fenceAddr);

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::lowPriority}));
    fenceAddr = ioctlHelper.getPagingFenceAddress(0, &osContext);
    EXPECT_EQ(osContext.getFenceAddr(0), fenceAddr);
}

TEST_F(IoctlHelperXeTest, givenIoctlWhenQueryDeviceParamsIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    uint32_t moduleId = 0;
    uint16_t serverType = 0;
    EXPECT_FALSE(xeIoctlHelper->queryDeviceParams(&moduleId, &serverType));
}

TEST_F(IoctlHelperXeTest, givenPrelimWhenQueryDeviceCapsIsCalledThenNullptrIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());

    EXPECT_EQ(xeIoctlHelper->queryDeviceCaps(), nullptr);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingSetVmPrefetchThenVmBindIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    uint64_t start = 0x12u;
    uint64_t length = 0x34u;
    uint32_t subDeviceId = 0u;
    uint32_t vmId = 1u;

    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    MemoryClassInstance targetMemoryRegion = memoryInfo->getLocalMemoryRegions()[subDeviceId].region;
    ASSERT_NE(nullptr, memoryInfo);
    drm->memoryInfo.reset(memoryInfo.release());
    int memoryClassDevice = static_cast<int>(DrmParam::memoryClassDevice);
    uint32_t region = (memoryClassDevice << 16u) | subDeviceId;

    EXPECT_TRUE(xeIoctlHelper->setVmPrefetch(start, length, region, vmId));
    EXPECT_EQ(1u, drm->vmBindInputs.size());

    EXPECT_EQ(drm->vmBindInputs[0].vm_id, vmId);
    EXPECT_EQ(drm->vmBindInputs[0].bind.addr, alignDown(start, MemoryConstants::pageSize));
    EXPECT_EQ(drm->vmBindInputs[0].bind.range, alignSizeWholePage(reinterpret_cast<void *>(start), length));
    EXPECT_EQ(drm->vmBindInputs[0].bind.prefetch_mem_region_instance, targetMemoryRegion.memoryInstance);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingSetVmPrefetchOnSecondTileThenVmBindIsCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    uint64_t start = 0x12u;
    uint64_t length = 0x34u;
    uint32_t subDeviceId = 1u;
    uint32_t vmId = 1u;

    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    MemoryClassInstance targetMemoryRegion = memoryInfo->getLocalMemoryRegions()[subDeviceId].region;
    ASSERT_NE(nullptr, memoryInfo);
    drm->memoryInfo.reset(memoryInfo.release());

    int memoryClassDevice = static_cast<int>(DrmParam::memoryClassDevice);
    uint32_t region = (memoryClassDevice << 16u) | subDeviceId;

    EXPECT_TRUE(xeIoctlHelper->setVmPrefetch(start, length, region, vmId));
    EXPECT_EQ(1u, drm->vmBindInputs.size());

    EXPECT_EQ(drm->vmBindInputs[0].vm_id, vmId);
    EXPECT_EQ(drm->vmBindInputs[0].bind.addr, alignDown(start, MemoryConstants::pageSize));
    EXPECT_EQ(drm->vmBindInputs[0].bind.range, alignSizeWholePage(reinterpret_cast<void *>(start), length));
    EXPECT_EQ(drm->vmBindInputs[0].bind.prefetch_mem_region_instance, targetMemoryRegion.memoryInstance);
}

TEST_F(IoctlHelperXeTest, givenIoctlFailureWhenCallingSetVmPrefetchThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXe::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXe *>(drm->getIoctlHelper());
    xeIoctlHelper->initialize();

    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    drm->memoryInfo.reset(memoryInfo.release());
    drm->testMode(1, -1);

    EXPECT_FALSE(xeIoctlHelper->setVmPrefetch(0u, 0u, 0u, 0u));
    EXPECT_EQ(0u, drm->vmBindInputs.size());
}

TEST_F(IoctlHelperXeTest, givenXeIoctlHelperWhenIsTimestampsRefreshEnabledCalledThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_TRUE(xeIoctlHelper->isTimestampsRefreshEnabled());
}

TEST_F(IoctlHelperXeTest, givenXeIoctlHelperWhenHasContextFreqHintCalledThenReturnFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_FALSE(xeIoctlHelper->hasContextFreqHint());
}

TEST_F(IoctlHelperXeTest, whenIoctlFailsOnQueryConfigSizeThenQueryDeviceIdAndRevisionFails) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        return -1;
    };
    EXPECT_FALSE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
}

TEST_F(IoctlHelperXeTest, givenZeroConfigSizeWhenQueryDeviceIdAndRevisionThenFail) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                deviceQuery->size = 0;
                return 0;
            }
        }
        return -1;
    };
    EXPECT_FALSE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
}

TEST_F(IoctlHelperXeTest, whenIoctlFailsOnQueryConfigThenQueryDeviceIdAndRevisionFails) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                if (deviceQuery->data) {
                    return -1;
                }
                deviceQuery->size = 1;
                return 0;
            }
        }
        return -1;
    };
    EXPECT_FALSE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
}

TEST_F(IoctlHelperXeTest, whenQueryDeviceIdAndRevisionThenProperValuesAreSet) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    auto hwInfo = drm->getRootDeviceEnvironment().getMutableHardwareInfo();

    static constexpr uint16_t mockDeviceId = 0x1234;
    static constexpr uint16_t mockRevisionId = 0xB;

    hwInfo->platform.usDeviceID = 0;
    hwInfo->platform.usRevId = 0;

    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                if (deviceQuery->data) {
                    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(deviceQuery->data);
                    config->num_params = DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1;
                    config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] = (static_cast<uint32_t>(mockRevisionId) << 16) | mockDeviceId;
                } else {
                    deviceQuery->size = (DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1) * sizeof(uint64_t);
                }

                return 0;
            }
        }
        return -1;
    };
    EXPECT_TRUE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));

    EXPECT_EQ(mockDeviceId, hwInfo->platform.usDeviceID);
    EXPECT_EQ(mockRevisionId, hwInfo->platform.usRevId);
}

TEST_F(IoctlHelperXeTest, whenQueryDeviceIdAndRevisionConfigFlagHasGpuAddrMirrorSetThenSharedSystemAllocEnableTrue) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                if (deviceQuery->data) {
                    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(deviceQuery->data);
                    config->num_params = DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1;
                    config->info[DRM_XE_QUERY_CONFIG_FLAGS] = DRM_XE_QUERY_CONFIG_FLAG_HAS_CPU_ADDR_MIRROR;
                } else {
                    deviceQuery->size = (DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1) * sizeof(uint64_t);
                }
                return 0;
            }
        }
        return -1;
    };

    EXPECT_TRUE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
    EXPECT_TRUE(drm->isSharedSystemAllocEnabled());
    uint64_t caps = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    drm->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = caps;
    drm->adjustSharedSystemMemCapabilities();
    EXPECT_EQ(caps, drm->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities);
    EXPECT_TRUE(drm->hasPageFaultSupport());
}

TEST_F(IoctlHelperXeTest, whenQueryDeviceIdAndRevisionConfigFlagHasGpuAddrMirrorSetButDebugFlagNotSetThenSharedSystemAllocEnableFalse) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(-1);
    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                if (deviceQuery->data) {
                    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(deviceQuery->data);
                    config->num_params = DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1;
                    config->info[DRM_XE_QUERY_CONFIG_FLAGS] = DRM_XE_QUERY_CONFIG_FLAG_HAS_CPU_ADDR_MIRROR;
                } else {
                    deviceQuery->size = (DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1) * sizeof(uint64_t);
                }
                return 0;
            }
        }
        return -1;
    };

    EXPECT_TRUE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
    EXPECT_FALSE(drm->isSharedSystemAllocEnabled());
    EXPECT_FALSE(drm->hasPageFaultSupport());
}

TEST_F(IoctlHelperXeTest, whenQueryDeviceIdAndRevisionAndConfigFlagHasGpuAddrMirrorClearThenSharedSystemAllocEnableFalse) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    DebugManagerStateRestore restore;
    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                if (deviceQuery->data) {
                    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(deviceQuery->data);
                    config->num_params = DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1;
                    config->info[DRM_XE_QUERY_CONFIG_FLAGS] = 0;
                } else {
                    deviceQuery->size = (DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1) * sizeof(uint64_t);
                }
                return 0;
            }
        }
        return -1;
    };

    EXPECT_TRUE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
    EXPECT_FALSE(drm->isSharedSystemAllocEnabled());
    uint64_t caps = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    drm->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = caps;
    drm->adjustSharedSystemMemCapabilities();
    EXPECT_EQ(0lu, drm->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities);
}

TEST_F(IoctlHelperXeTest, whenQueryDeviceIdAndRevisionAndSharedSystemUsmSupportDebugFlagClearThenSharedSystemAllocEnableFalse) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    DebugManagerStateRestore restore;
    debugManager.flags.EnableRecoverablePageFaults.set(0);
    mockIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_XE_DEVICE_QUERY) {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            if (deviceQuery->query == DRM_XE_DEVICE_QUERY_CONFIG) {
                if (deviceQuery->data) {
                    struct drm_xe_query_config *config = reinterpret_cast<struct drm_xe_query_config *>(deviceQuery->data);
                    config->num_params = DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1;
                    config->info[DRM_XE_QUERY_CONFIG_FLAGS] = DRM_XE_QUERY_CONFIG_FLAG_HAS_CPU_ADDR_MIRROR;
                } else {
                    deviceQuery->size = (DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID + 1) * sizeof(uint64_t);
                }
                return 0;
            }
        }
        return -1;
    };

    EXPECT_TRUE(IoctlHelperXe::queryDeviceIdAndRevision(*drm));
    EXPECT_FALSE(drm->isSharedSystemAllocEnabled());
    uint64_t caps = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    drm->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = caps;
    drm->adjustSharedSystemMemCapabilities();
    EXPECT_EQ(0lu, drm->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities);
    EXPECT_FALSE(drm->hasPageFaultSupport());
}

TEST_F(IoctlHelperXeTest, givenXeIoctlHelperAndDeferBackingFlagSetToTrueWhenMakeResidentBeforeLockNeededIsCalledThenVerifyTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableDeferBacking.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_TRUE(xeIoctlHelper->makeResidentBeforeLockNeeded());
}
