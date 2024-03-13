/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/xe/ioctl_helper_xe_tests.h"

using namespace NEO;

TEST(IoctlHelperXeTest, givenXeDrmVersionsWhenGettingIoctlHelperThenValidIoctlHelperIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_NE(nullptr, xeIoctlHelper);
}

TEST(IoctlHelperXeTest, whenGettingIfImmediateVmBindIsRequiredThenTrueIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    EXPECT_TRUE(ioctlHelper.isImmediateVmBindRequired());
}

TEST(IoctlHelperXeTest, givenXeDrmWhenGetPciBarrierMmapThenReturnsNullptr) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperXe ioctlHelper{*drm};

    auto ptr = ioctlHelper.pciBarrierMmap();
    EXPECT_EQ(ptr, nullptr);
}

TEST(IoctlHelperXeTest, whenChangingBufferBindingThenWaitIsNeededAlways) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperXe ioctlHelper{*drm};

    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(false));
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGemCreateExtWithRegionsThenDummyValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {0, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {1, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    MemRegionsVec memRegions = {regionInfo[0].region, regionInfo[1].region};

    uint32_t handle = 0u;
    uint32_t numOfChunks = 0;

    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, 0u, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt));
    EXPECT_FALSE(xeIoctlHelper->bindInfo.empty());
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm.createParamsCpuCaching);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGemCreateExtWithRegionsAndVmIdThenDummyValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    ASSERT_NE(nullptr, xeIoctlHelper);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {0, 0};
    regionInfo[0].probedSize = 8 * MemoryConstants::gigaByte;
    regionInfo[1].region = {1, 0};
    regionInfo[1].probedSize = 16 * MemoryConstants::gigaByte;
    MemRegionsVec memRegions = {regionInfo[0].region, regionInfo[1].region};

    uint32_t handle = 0u;
    uint32_t numOfChunks = 0;

    GemVmControl test = {};
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, 0u, handle, 0, test.vmId, -1, false, numOfChunks, std::nullopt, std::nullopt));
    EXPECT_FALSE(xeIoctlHelper->bindInfo.empty());
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm.createParamsCpuCaching);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGemCreateAndNoLocalMemoryThenProperValuesSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    drm.memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    ASSERT_NE(nullptr, xeIoctlHelper);

    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(0, drm.ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    uint32_t handle = xeIoctlHelper->createGem(size, memoryBanks);
    EXPECT_EQ(1, drm.ioctlCnt.gemCreate);
    EXPECT_FALSE(xeIoctlHelper->bindInfo.empty());

    EXPECT_EQ(size, drm.createParamsSize);
    EXPECT_EQ(0u, drm.createParamsFlags);
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm.createParamsCpuCaching);
    EXPECT_EQ(1u, drm.createParamsPlacement);

    // dummy mock handle
    EXPECT_EQ(handle, drm.createParamsHandle);
    EXPECT_EQ(handle, testValueGemCreate);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGemCreateWhenMemoryBanksZeroThenProperValuesSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(0);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    drm.memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    ASSERT_NE(nullptr, xeIoctlHelper);

    uint64_t size = 1234;
    uint32_t memoryBanks = 0u;

    EXPECT_EQ(0, drm.ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    uint32_t handle = xeIoctlHelper->createGem(size, memoryBanks);
    EXPECT_EQ(1, drm.ioctlCnt.gemCreate);
    EXPECT_FALSE(xeIoctlHelper->bindInfo.empty());

    EXPECT_EQ(size, drm.createParamsSize);
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm.createParamsCpuCaching);
    EXPECT_EQ(0u, drm.createParamsFlags);
    EXPECT_EQ(1u, drm.createParamsPlacement);

    // dummy mock handle
    EXPECT_EQ(handle, drm.createParamsHandle);
    EXPECT_EQ(handle, testValueGemCreate);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGemCreateAndLocalMemoryThenProperValuesSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    drm.memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    ASSERT_NE(nullptr, xeIoctlHelper);

    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(0, drm.ioctlCnt.gemCreate);
    EXPECT_TRUE(xeIoctlHelper->bindInfo.empty());
    uint32_t handle = xeIoctlHelper->createGem(size, memoryBanks);
    EXPECT_EQ(1, drm.ioctlCnt.gemCreate);
    EXPECT_FALSE(xeIoctlHelper->bindInfo.empty());

    EXPECT_EQ(size, drm.createParamsSize);
    EXPECT_EQ(DRM_XE_GEM_CPU_CACHING_WC, drm.createParamsCpuCaching);
    EXPECT_EQ(0u, drm.createParamsFlags);
    EXPECT_EQ(6u, drm.createParamsPlacement);

    // dummy mock handle
    EXPECT_EQ(handle, drm.createParamsHandle);
    EXPECT_EQ(handle, testValueGemCreate);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallSetGemTilingThenAlwaysTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);

    ASSERT_NE(nullptr, xeIoctlHelper);
    GemSetTiling setTiling{};
    uint32_t ret = xeIoctlHelper->setGemTiling(&setTiling);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(drm.ioctlCalled);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallGetGemTilingThenAlwaysTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);

    ASSERT_NE(nullptr, xeIoctlHelper);
    GemGetTiling getTiling{};
    uint32_t ret = xeIoctlHelper->getGemTiling(&getTiling);
    EXPECT_TRUE(ret);
    EXPECT_FALSE(drm.ioctlCalled);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeisVmBindPatIndexExtSupportedReturnsFalse) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<IoctlHelperXe>(drm);
    ASSERT_EQ(false, xeIoctlHelper->isVmBindPatIndexExtSupported());
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingAnyMethodThenDummyValueIsReturned) {
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
    EXPECT_NE(0, xeIoctlHelper->createGemExt(memRegions, 0u, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt));

    EXPECT_TRUE(xeIoctlHelper->isVmBindAvailable());

    EXPECT_FALSE(xeIoctlHelper->isSetPairAvailable());

    EXPECT_EQ(CacheRegion::none, xeIoctlHelper->closAlloc());

    EXPECT_EQ(0u, xeIoctlHelper->closAllocWays(CacheRegion::none, 0u, 0u));

    EXPECT_EQ(CacheRegion::none, xeIoctlHelper->closFree(CacheRegion::none));

    EXPECT_EQ(0, xeIoctlHelper->waitUserFence(0, 0, 0, 0, 0, 0));

    EXPECT_EQ(0u, xeIoctlHelper->getAtomicAdvise(false));

    EXPECT_EQ(0u, xeIoctlHelper->getAtomicAccess(AtomicAccessMode::none));

    EXPECT_EQ(0u, xeIoctlHelper->getPreferredLocationAdvise());

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getPreferredLocationRegion(PreferredLocation::none, 0));

    EXPECT_FALSE(xeIoctlHelper->setVmBoAdvise(0, 0, nullptr));

    EXPECT_FALSE(xeIoctlHelper->setVmBoAdviseForChunking(0, 0, 0, 0, nullptr));

    EXPECT_FALSE(xeIoctlHelper->isChunkingAvailable());

    EXPECT_FALSE(xeIoctlHelper->setVmPrefetch(0, 0, 0, 0));

    EXPECT_EQ(0u, xeIoctlHelper->getDirectSubmissionFlag());

    EXPECT_EQ(0u, xeIoctlHelper->getFlagsForVmBind(false, false, false));

    std::vector<QueryItem> queryItems;
    std::vector<DistanceInfo> distanceInfos;
    EXPECT_EQ(0, xeIoctlHelper->queryDistances(queryItems, distanceInfos));
    EXPECT_EQ(0u, distanceInfos.size());

    EXPECT_EQ(0u, xeIoctlHelper->getWaitUserFenceSoftFlag());

    EXPECT_EQ(0, xeIoctlHelper->execBuffer(nullptr, 0, 0));

    EXPECT_FALSE(xeIoctlHelper->completionFenceExtensionSupported(false));

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getHasPageFaultParamId());

    EXPECT_EQ(nullptr, xeIoctlHelper->createVmControlExtRegion({}));

    EXPECT_EQ(0u, xeIoctlHelper->getFlagsForVmCreate(false, false, false));

    GemContextCreateExt gcc;
    EXPECT_EQ(0u, xeIoctlHelper->createContextWithAccessCounters(gcc));

    EXPECT_EQ(0u, xeIoctlHelper->createCooperativeContext(gcc));

    VmBindExtSetPatT vmBindExtSetPat{};
    EXPECT_NO_THROW(xeIoctlHelper->fillVmBindExtSetPat(vmBindExtSetPat, 0, 0));

    VmBindExtUserFenceT vmBindExtUserFence{};
    EXPECT_NO_THROW(xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, 0, 0, 0));

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getCopyClassSaturatePCIECapability());

    EXPECT_EQ(std::nullopt, xeIoctlHelper->getCopyClassSaturateLinkCapability());

    EXPECT_EQ(0u, xeIoctlHelper->getVmAdviseAtomicAttribute());

    VmBindParams vmBindParams{};
    EXPECT_EQ(-1, xeIoctlHelper->vmBind(vmBindParams));

    EXPECT_EQ(-1, xeIoctlHelper->vmUnbind(vmBindParams));

    std::array<uint64_t, 12u> properties;
    EXPECT_FALSE(xeIoctlHelper->getEuStallProperties(properties, 0, 0, 0, 0, 0));

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

    // Default no translation:
    verifyDrmGetParamValue(static_cast<int>(DrmParam::execRender), DrmParam::execRender);
    // test exception:
    verifyDrmGetParamValue(DRM_XE_MEM_REGION_CLASS_VRAM, DrmParam::memoryClassDevice);
    verifyDrmGetParamValue(DRM_XE_MEM_REGION_CLASS_SYSMEM, DrmParam::memoryClassSystem);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_RENDER, DrmParam::engineClassRender);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_COPY, DrmParam::engineClassCopy);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_VIDEO_DECODE, DrmParam::engineClassVideo);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, DrmParam::engineClassVideoEnhance);
    verifyDrmGetParamValue(DRM_XE_ENGINE_CLASS_COMPUTE, DrmParam::engineClassCompute);
    verifyDrmGetParamValue(-1, DrmParam::engineClassInvalid);

    // Expect stringify
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
    verifyDrmParamString("ParamChipsetId", DrmParam::paramChipsetId);
    verifyDrmParamString("ParamRevision", DrmParam::paramRevision);
    verifyDrmParamString("ParamHasExecSoftpin", DrmParam::paramHasExecSoftpin);
    verifyDrmParamString("ParamHasPooledEu", DrmParam::paramHasPooledEu);
    verifyDrmParamString("ParamHasScheduler", DrmParam::paramHasScheduler);
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
    verifyDrmParamString("SchedulerCapPreemption", DrmParam::schedulerCapPreemption);
    verifyDrmParamString("TilingNone", DrmParam::tilingNone);
    verifyDrmParamString("TilingY", DrmParam::tilingY);

    verifyIoctlString(DrmIoctl::gemClose, "DRM_IOCTL_GEM_CLOSE");
    verifyIoctlString(DrmIoctl::gemCreate, "DRM_IOCTL_XE_GEM_CREATE");
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

    EXPECT_TRUE(xeIoctlHelper->completionFenceExtensionSupported(true));

    EXPECT_EQ(static_cast<uint32_t>(XE_NEO_VMCREATE_DISABLESCRATCH_FLAG |
                                    XE_NEO_VMCREATE_ENABLEPAGEFAULT_FLAG |
                                    XE_NEO_VMCREATE_USEVMBIND_FLAG),
              xeIoctlHelper->getFlagsForVmCreate(true, true, true));

    EXPECT_EQ(static_cast<uint64_t>(XE_NEO_BIND_CAPTURE_FLAG |
                                    XE_NEO_BIND_IMMEDIATE_FLAG |
                                    XE_NEO_BIND_MAKERESIDENT_FLAG),
              xeIoctlHelper->getFlagsForVmBind(true, true, true));

    uint32_t fabricId = 0, latency = 0, bandwidth = 0;
    EXPECT_FALSE(xeIoctlHelper->getFabricLatency(fabricId, latency, bandwidth));
}

TEST(IoctlHelperXeTest, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
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
}

TEST(IoctlHelperXeTest, verifyPublicFunctions) {

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

    auto verifyXeFlagsBindName = [&mockXeIoctlHelper](const char *name, auto flags) {
        EXPECT_STREQ(name, mockXeIoctlHelper->xeGetBindFlagsName(flags));
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

    verifyXeFlagsBindName("READ_ONLY", DRM_XE_VM_BIND_FLAG_READONLY);
    verifyXeFlagsBindName("IMMEDIATE", DRM_XE_VM_BIND_FLAG_IMMEDIATE);
    verifyXeFlagsBindName("NULL", DRM_XE_VM_BIND_FLAG_NULL);
    verifyXeFlagsBindName("Unknown flag", -1);

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

TEST(IoctlHelperXeTest, whenGettingFileNamesForFrequencyThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = std::make_unique<IoctlHelperXe>(drm);
    EXPECT_STREQ("/device/gt0/freq_max", ioctlHelper->getFileForMaxGpuFrequency().c_str());
    EXPECT_STREQ("/device/gt2/freq_max", ioctlHelper->getFileForMaxGpuFrequencyOfSubDevice(2).c_str());
    EXPECT_STREQ("/device/gt1/freq_rp0", ioctlHelper->getFileForMaxMemoryFrequencyOfSubDevice(1).c_str());
}

TEST(IoctlHelperXeTest, whenCallingIoctlThenProperValueIsReturned) {
    int ret;
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto mockXeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    drm.reset();
    {
        drm.testMode(1, -1);
        ret = mockXeIoctlHelper->initialize();
        EXPECT_EQ(0, ret);
    }
    drm.testMode(0);
    {
        GemUserPtr test = {};
        test.handle = 1;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemUserptr, &test);
        EXPECT_EQ(0, ret);
        GemClose cl = {};
        cl.handle = test.handle;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemClose, &cl);
        EXPECT_EQ(0, ret);
    }
    {
        GemVmControl test = {};
        drm.pageFaultSupported = false;
        uint32_t expectedVmCreateFlags = DRM_XE_VM_CREATE_FLAG_LR_MODE;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemVmCreate, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<int>(test.vmId), testValueVmId);
        EXPECT_EQ(test.flags, expectedVmCreateFlags);

        drm.pageFaultSupported = true;
        expectedVmCreateFlags = DRM_XE_VM_CREATE_FLAG_LR_MODE |
                                DRM_XE_VM_CREATE_FLAG_FAULT_MODE;
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
        EXPECT_EQ(0, ret);
        test.param = contextPrivateParamBoost;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextSetparam, &test);
        EXPECT_EQ(0, ret);
        test.param = static_cast<int>(DrmParam::contextParamEngines);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextSetparam, &test);
        EXPECT_EQ(-1, ret);
    }
    {
        auto hwInfo = drm.getRootDeviceEnvironment().getHardwareInfo();

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
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0x55fdd94d4e40ull, test.value);
        test.param = static_cast<int>(DrmParam::contextParamPersistence);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemContextGetparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(0x1ull, test.value);
    }
    {
        GemClose test = {};
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::gemClose, &test);
        EXPECT_EQ(0, ret);
    }
    auto engineInfo = mockXeIoctlHelper->createEngineInfo(false);
    EXPECT_NE(nullptr, engineInfo);
    {
        GetParam test = {};
        int dstvalue;
        test.value = &dstvalue;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(-1, ret);
        test.param = static_cast<int>(DrmParam::paramChipsetId);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
        test.param = static_cast<int>(DrmParam::paramRevision);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
        test.param = static_cast<int>(DrmParam::paramHasPageFault);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 0);
        test.param = static_cast<int>(DrmParam::paramHasExecSoftpin);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 1);
        test.param = static_cast<int>(DrmParam::paramHasScheduler);
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(static_cast<unsigned int>(dstvalue), 0x80000037);
        test.param = static_cast<int>(DrmParam::paramCsTimestampFrequency);
        mockXeIoctlHelper->xeTimestampFrequency = 1;
        ret = mockXeIoctlHelper->ioctl(DrmIoctl::getparam, &test);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(dstvalue, 1);
    }
    EXPECT_THROW(mockXeIoctlHelper->ioctl(DrmIoctl::gemContextCreateExt, NULL), std::runtime_error);
    drm.reset();
}

TEST(IoctlHelperXeTest, givenGeomDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint16_t tileId = 0;
    for (auto gtId = 0u; gtId < 3u; gtId++) {
        drm.addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0b11'1111, 0, 0, 0, 0, 0, 0, 0});
        drm.addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm.addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});
    }
    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(6, topologyData.subSliceCount);
    EXPECT_EQ(6, topologyData.maxSubSliceCount);

    EXPECT_EQ(96, topologyData.euCount);
    EXPECT_EQ(16, topologyData.maxEuPerSubSlice);

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

TEST(IoctlHelperXeTest, givenComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint16_t tileId = 0;
    for (auto gtId = 0u; gtId < 3u; gtId++) {
        drm.addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm.addMockedQueryTopologyData(gtId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm.addMockedQueryTopologyData(gtId, DRM_XE_TOPO_EU_PER_DSS, 8, {0b1111'1111, 0, 0, 0, 0, 0, 0, 0});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(64, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSliceCount);

    EXPECT_EQ(512, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEuPerSubSlice);

    // verify topology map
    std::vector<int> expectedSliceIndices = {0};
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

TEST(IoctlHelperXeTest, givenOnlyMediaTypeWhenGetTopologyDataAndMapThenSubsliceIndicesNotSet) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm.queryGtList.begin());
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
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint16_t tileId = 0;
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 8, {0b1111'1111, 0, 0, 0, 0, 0, 0, 0});

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(0, topologyData.subSliceCount);
    EXPECT_EQ(0, topologyData.maxSubSliceCount);

    EXPECT_EQ(0, topologyData.euCount);
    EXPECT_EQ(0, topologyData.maxEuPerSubSlice);

    // verify topology map
    ASSERT_EQ(0u, topologyMap[tileId].sliceIndices.size());

    ASSERT_EQ(0u, topologyMap[tileId].subsliceIndices.size());
}

TEST(IoctlHelperXeTest, givenMainAndMediaTypesWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.queryGtList.resize(49);
    auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(drm.queryGtList.begin());
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
        0,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b100,                      // native mem regions
        0x011,                      // slow mem regions
    };
    xeQueryGtList->gt_list[2] = {
        DRM_XE_QUERY_GT_TYPE_MAIN, // type
        0,                         // tile_id
        0,                         // gt_id
        {0},                       // padding
        12500000,                  // reference_clock
        0b010,                     // native mem regions
        0x101,                     // slow mem regions
    };
    xeQueryGtList->gt_list[3] = {
        DRM_XE_QUERY_GT_TYPE_MEDIA, // type
        0,                          // tile_id
        0,                          // gt_id
        {0},                        // padding
        12500000,                   // reference_clock
        0b001,                      // native mem regions
        0x100,                      // slow mem regions
    };

    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    for (auto tileId = 0; tileId < 4; tileId++) {
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(64, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSliceCount);

    EXPECT_EQ(4096, topologyData.euCount);
    EXPECT_EQ(64, topologyData.maxEuPerSubSlice);
    EXPECT_EQ(2u, topologyMap.size());
    // verify topology map
    for (auto tileId : {0u, 1u}) {
        ASSERT_EQ(1u, topologyMap[tileId].sliceIndices.size());
        ASSERT_EQ(64u, topologyMap[tileId].subsliceIndices.size());
    }
}

struct DrmMockXe2T : public DrmMockXe {
    DrmMockXe2T(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockXe(rootDeviceEnvironment) {
        auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(queryMemUsage);
        xeQueryMemUsage->num_mem_regions = 3;
        xeQueryMemUsage->mem_regions[0] = {
            DRM_XE_MEM_REGION_CLASS_VRAM,  // class
            1,                             // instance
            MemoryConstants::pageSize,     // min page size
            2 * MemoryConstants::gigaByte, // total size
            MemoryConstants::megaByte      // used size
        };
        xeQueryMemUsage->mem_regions[1] = {
            DRM_XE_MEM_REGION_CLASS_SYSMEM, // class
            0,                              // instance
            MemoryConstants::pageSize,      // min page size
            MemoryConstants::gigaByte,      // total size
            MemoryConstants::kiloByte       // used size
        };
        xeQueryMemUsage->mem_regions[2] = {
            DRM_XE_MEM_REGION_CLASS_VRAM,  // class
            2,                             // instance
            MemoryConstants::pageSize,     // min page size
            4 * MemoryConstants::gigaByte, // total size
            MemoryConstants::gigaByte      // used size
        };
        queryGtList.resize(25);
        auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(queryGtList.begin());
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
            0,                         // gt_id
            {0},                       // padding
            12500000,                  // reference_clock
            0b010,                     // native mem regions
            0x101,                     // slow mem regions
        };
    }
};

TEST(IoctlHelperXeTest, given2TileAndComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe2T drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    // symetric tiles
    for (uint16_t tileId = 0; tileId < 2u; tileId++) {
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(64, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSliceCount);

    EXPECT_EQ(512, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEuPerSubSlice);

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

TEST(IoctlHelperXeTest, given2TileWithDisabledDssOn1TileAndComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe2T drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    // half dss disabled on tile 0
    uint16_t tileId = 0;
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0});
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});

    tileId = 1;
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
    drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(32, topologyData.subSliceCount);
    EXPECT_EQ(64, topologyData.maxSubSliceCount);

    EXPECT_EQ(256, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEuPerSubSlice);

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

TEST(IoctlHelperXeTest, given2TileWithDisabledEvenDssAndComputeDssWhenGetTopologyDataAndMapThenResultsAreCorrect) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe2T drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    for (uint16_t tileId = 0; tileId < 2u; tileId++) {
        // even dss disabled
        uint8_t data = 0b1010'1010;

        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_GEOMETRY, 8, {0, 0, 0, 0, 0, 0, 0, 0});
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_DSS_COMPUTE, 8, {data, data, data, data, data, data, data, data});
        drm.addMockedQueryTopologyData(tileId, DRM_XE_TOPO_EU_PER_DSS, 4, {0b1111'1111, 0, 0, 0});
    }

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    ASSERT_TRUE(result);

    // verify topology data
    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.maxSliceCount);

    EXPECT_EQ(32, topologyData.subSliceCount);
    EXPECT_EQ(32, topologyData.maxSubSliceCount);

    EXPECT_EQ(256, topologyData.euCount);
    EXPECT_EQ(8, topologyData.maxEuPerSubSlice);

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

TEST(IoctlHelperXeTest, givenInvalidTypeFlagGetTopologyDataAndMapThenReturnFalse) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint16_t tileId = 0;

    uint16_t incorrectFlagType = 128u;
    drm.addMockedQueryTopologyData(tileId, incorrectFlagType, 8, {0b11'1111, 0, 0, 0, 0, 0, 0, 0});
    drm.addMockedQueryTopologyData(tileId, incorrectFlagType, 8, {0, 0, 0, 0, 0, 0, 0, 0});
    drm.addMockedQueryTopologyData(tileId, incorrectFlagType, 8, {0b1111'1111, 0b1111'1111, 0, 0, 0, 0, 0, 0});

    DrmQueryTopologyData topologyData{};
    TopologyMap topologyMap{};

    auto result = xeIoctlHelper->getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
    EXPECT_FALSE(result);
}

TEST(IoctlHelperXeTest, whenCreatingEngineInfoThenProperEnginesAreDiscovered) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

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

TEST(IoctlHelperXeTest, whenCreatingMemoryInfoThenProperMemoryBanksAreDiscovered) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    EXPECT_NE(nullptr, memoryInfo);

    auto memoryClassInstance0 = memoryInfo->getMemoryRegionClassAndInstance(0, *defaultHwInfo);
    EXPECT_EQ(static_cast<uint16_t>(DRM_XE_MEM_REGION_CLASS_SYSMEM), memoryClassInstance0.memoryClass);
    EXPECT_EQ(0u, memoryClassInstance0.memoryInstance);
    EXPECT_EQ(MemoryConstants::gigaByte, memoryInfo->getMemoryRegionSize(0));

    auto memoryClassInstance1 = memoryInfo->getMemoryRegionClassAndInstance(0b01, *defaultHwInfo);
    EXPECT_EQ(static_cast<uint16_t>(DRM_XE_MEM_REGION_CLASS_VRAM), memoryClassInstance1.memoryClass);
    EXPECT_EQ(2u, memoryClassInstance1.memoryInstance);
    EXPECT_EQ(4 * MemoryConstants::gigaByte, memoryInfo->getMemoryRegionSize(0b01));

    auto memoryClassInstance2 = memoryInfo->getMemoryRegionClassAndInstance(0b10, *defaultHwInfo);
    EXPECT_EQ(static_cast<uint16_t>(DRM_XE_MEM_REGION_CLASS_VRAM), memoryClassInstance2.memoryClass);
    EXPECT_EQ(1u, memoryClassInstance2.memoryInstance);
    EXPECT_EQ(2 * MemoryConstants::gigaByte, memoryInfo->getMemoryRegionSize(0b10));

    auto &memoryRegions = memoryInfo->getDrmRegionInfos();
    EXPECT_EQ(3u, memoryRegions.size());

    EXPECT_EQ(0u, memoryRegions[0].region.memoryInstance);
    EXPECT_EQ(MemoryConstants::gigaByte, memoryRegions[0].probedSize);
    EXPECT_EQ(MemoryConstants::gigaByte - MemoryConstants::kiloByte, memoryRegions[0].unallocatedSize);

    EXPECT_EQ(2u, memoryRegions[1].region.memoryInstance);
    EXPECT_EQ(4 * MemoryConstants::gigaByte, memoryRegions[1].probedSize);
    EXPECT_EQ(3 * MemoryConstants::gigaByte, memoryRegions[1].unallocatedSize);

    EXPECT_EQ(1u, memoryRegions[2].region.memoryInstance);
    EXPECT_EQ(2 * MemoryConstants::gigaByte, memoryRegions[2].probedSize);
    EXPECT_EQ(2 * MemoryConstants::gigaByte - MemoryConstants::megaByte, memoryRegions[2].unallocatedSize);
}

TEST(IoctlHelperXeTest, givenIoctlFailureWhenCreatingMemoryInfoThenNoMemoryBanksAreDiscovered) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    drm.testMode(1, -1);
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    EXPECT_EQ(nullptr, memoryInfo);
}

TEST(IoctlHelperXeTest, givenNoMemoryRegionsWhenCreatingMemoryInfoThenMemoryInfoIsNotCreated) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(drm.queryMemUsage);
    xeQueryMemUsage->num_mem_regions = 0u;
    auto memoryInfo = xeIoctlHelper->createMemoryInfo();
    EXPECT_EQ(nullptr, memoryInfo);
}

TEST(IoctlHelperXeTest, givenIoctlFailureWhenCreatingEngineInfoThenNoEnginesAreDiscovered) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    drm.testMode(1, -1);
    auto engineInfo = xeIoctlHelper->createEngineInfo(true);
    EXPECT_EQ(nullptr, engineInfo);
}

TEST(IoctlHelperXeTest, givenEnabledFtrMultiTileArchWhenCreatingEngineInfoThenMultiTileArchInfoIsProperlySet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    DrmMockXe drm{rootDeviceEnvironment};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

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

TEST(IoctlHelperXeTest, givenDisabledFtrMultiTileArchWhenCreatingEngineInfoThenMultiTileArchInfoIsNotSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    DrmMockXe drm{rootDeviceEnvironment};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

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

TEST(IoctlHelperXeTest, whenCallingVmBindThenWaitUserFenceIsCalled) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    BindInfo mockBindInfo{};
    mockBindInfo.handle = 0x1234;
    xeIoctlHelper->bindInfo.push_back(mockBindInfo);

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = mockBindInfo.handle;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm.vmBindInputs.clear();
    drm.syncInputs.clear();
    drm.waitUserFenceInputs.clear();
    EXPECT_EQ(0, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(1u, drm.vmBindInputs.size());
    EXPECT_EQ(1u, drm.syncInputs.size());
    EXPECT_EQ(1u, drm.waitUserFenceInputs.size());
    {
        auto &sync = drm.syncInputs[0];

        EXPECT_EQ(fenceAddress, sync.addr);
        EXPECT_EQ(fenceValue, sync.timeline_value);

        auto &waitUserFence = drm.waitUserFenceInputs[0];

        EXPECT_EQ(fenceAddress, waitUserFence.addr);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_UFENCE_WAIT_OP_EQ), waitUserFence.op);
        EXPECT_EQ(0u, waitUserFence.flags);
        EXPECT_EQ(fenceValue, waitUserFence.value);
        EXPECT_EQ(static_cast<uint64_t>(DRM_XE_UFENCE_WAIT_MASK_U64), waitUserFence.mask);
        EXPECT_EQ(static_cast<int64_t>(XE_ONE_SEC), waitUserFence.timeout);
        EXPECT_EQ(0u, waitUserFence.exec_queue_id);
    }
    drm.vmBindInputs.clear();
    drm.syncInputs.clear();
    drm.waitUserFenceInputs.clear();
    EXPECT_EQ(0, xeIoctlHelper->vmUnbind(vmBindParams));
    EXPECT_EQ(1u, drm.vmBindInputs.size());
    EXPECT_EQ(1u, drm.syncInputs.size());
    EXPECT_EQ(1u, drm.waitUserFenceInputs.size());
    {
        auto &sync = drm.syncInputs[0];

        EXPECT_EQ(fenceAddress, sync.addr);
        EXPECT_EQ(fenceValue, sync.timeline_value);

        auto &waitUserFence = drm.waitUserFenceInputs[0];

        EXPECT_EQ(fenceAddress, waitUserFence.addr);
        EXPECT_EQ(static_cast<uint16_t>(DRM_XE_UFENCE_WAIT_OP_EQ), waitUserFence.op);
        EXPECT_EQ(0u, waitUserFence.flags);
        EXPECT_EQ(fenceValue, waitUserFence.value);
        EXPECT_EQ(static_cast<uint64_t>(DRM_XE_UFENCE_WAIT_MASK_U64), waitUserFence.mask);
        EXPECT_EQ(static_cast<int64_t>(XE_ONE_SEC), waitUserFence.timeout);
        EXPECT_EQ(0u, waitUserFence.exec_queue_id);
    }
}

TEST(IoctlHelperXeTest, whenGemVmBindFailsThenErrorIsPropagated) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    BindInfo mockBindInfo{};
    mockBindInfo.handle = 0x1234;
    xeIoctlHelper->bindInfo.push_back(mockBindInfo);

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = mockBindInfo.handle;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm.waitUserFenceInputs.clear();

    int errorValue = -1;
    drm.gemVmBindReturn = errorValue;
    EXPECT_EQ(errorValue, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(0u, drm.waitUserFenceInputs.size());

    EXPECT_EQ(errorValue, xeIoctlHelper->vmUnbind(vmBindParams));
    EXPECT_EQ(0u, drm.waitUserFenceInputs.size());
}

TEST(IoctlHelperXeTest, whenUserFenceFailsThenErrorIsPropagated) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    BindInfo mockBindInfo{};
    mockBindInfo.handle = 0x1234;
    xeIoctlHelper->bindInfo.push_back(mockBindInfo);

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = mockBindInfo.handle;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    drm.waitUserFenceInputs.clear();

    int errorValue = -1;
    drm.waitUserFenceReturn = errorValue;

    EXPECT_EQ(errorValue, xeIoctlHelper->vmBind(vmBindParams));
    EXPECT_EQ(errorValue, xeIoctlHelper->vmUnbind(vmBindParams));
}

TEST(IoctlHelperXeTest, WhenSetupIpVersionIsCalledThenIpVersionIsCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto &compilerProductHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    xeIoctlHelper->setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

TEST(IoctlHelperXeTest, whenXeShowBindTableIsCalledThenBindLogsArePrinted) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    BindInfo mockBindInfo{};
    mockBindInfo.handle = 1u;
    mockBindInfo.userptr = 2u;
    mockBindInfo.addr = 3u;
    mockBindInfo.size = 4u;
    xeIoctlHelper->bindInfo.push_back(mockBindInfo);

    ::testing::internal::CaptureStderr();

    debugManager.flags.PrintXeLogs.set(true);
    xeIoctlHelper->xeShowBindTable();
    debugManager.flags.PrintXeLogs.set(false);

    std::string output = testing::internal::GetCapturedStderr();
    std::string expectedOutput = R"(show bind: (<index> <handle> <userptr> <addr> <size>)
   0 x00000001 x0000000000000002 x0000000000000003 x0000000000000004
)";
    EXPECT_STREQ(expectedOutput.c_str(), output.c_str());
}

TEST(IoctlHelperXeTest, whenFillBindInfoForIpcHandleIsCalledThenBindInfoIsCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    uint32_t handle = 100;
    size_t size = 1024u;
    xeIoctlHelper->fillBindInfoForIpcHandle(handle, size);
    auto bindInfo = xeIoctlHelper->bindInfo[0];
    EXPECT_EQ(bindInfo.handle, handle);
    EXPECT_EQ(bindInfo.size, size);
}

TEST(IoctlHelperXeTest, givenIoctlFailureWhenGetTimestampFrequencyIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    drm.testMode(1, -1);
    uint64_t frequency;
    auto ret = xeIoctlHelper->getTimestampFrequency(frequency);
    EXPECT_EQ(false, ret);
}

TEST(IoctlHelperXeTest, whenGetTimestampFrequencyIsCalledThenProperFrequencyIsSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    uint32_t expectedFrequency = 100;
    xeIoctlHelper->xeTimestampFrequency = expectedFrequency;

    uint64_t frequency = 0;
    auto ret = xeIoctlHelper->getTimestampFrequency(frequency);
    EXPECT_EQ(true, ret);
    EXPECT_EQ(expectedFrequency, frequency);
}

TEST(IoctlHelperXeTest, givenIoctlFailureWhenSetGpuCpuTimesIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXe drm{rootDeviceEnvironment};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    drm.testMode(1, -1);
    TimeStampData pGpuCpuTime{};
    std::unique_ptr<MockOSTimeLinux> osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    auto ret = xeIoctlHelper->setGpuCpuTimes(&pGpuCpuTime, osTime.get());
    EXPECT_EQ(false, ret);
}

TEST(IoctlHelperXeTest, givenIoctlFailureWhenSetGpuCpuTimesIsCalledThenProperValuesAreSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    DrmMockXe drm{rootDeviceEnvironment};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);

    uint64_t expectedCycles = 100000;
    uint64_t expectedTimestamp = 100;
    auto xeQueryEngineCycles = reinterpret_cast<drm_xe_query_engine_cycles *>(drm.queryEngineCycles);
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

TEST(IoctlHelperXeTest, whenSetDefaultEngineIsCalledThenProperEngineIsSet) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(&hwInfo);
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    auto engineInfo = xeIoctlHelper->createEngineInfo(true);
    ASSERT_NE(nullptr, engineInfo);

    xeIoctlHelper->setDefaultEngine(aub_stream::EngineType::ENGINE_CCS);
    EXPECT_EQ(DRM_XE_ENGINE_CLASS_COMPUTE, xeIoctlHelper->defaultEngine->engine_class);
    xeIoctlHelper->setDefaultEngine(aub_stream::EngineType::ENGINE_RCS);
    EXPECT_EQ(DRM_XE_ENGINE_CLASS_RENDER, xeIoctlHelper->defaultEngine->engine_class);

    EXPECT_THROW(xeIoctlHelper->setDefaultEngine(aub_stream::EngineType::ENGINE_BCS), std::exception);
    EXPECT_THROW(xeIoctlHelper->setDefaultEngine(aub_stream::EngineType::ENGINE_VCS), std::exception);
    EXPECT_THROW(xeIoctlHelper->setDefaultEngine(aub_stream::EngineType::ENGINE_VECS), std::exception);
}

TEST(IoctlHelperXeTest, givenNoEnginesWhenSetDefaultEngineIsCalledThenAbortIsThrown) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    auto defaultEngineType = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->capabilityTable.defaultEngineType;
    EXPECT_THROW(xeIoctlHelper->setDefaultEngine(defaultEngineType), std::exception);
}

TEST(IoctlHelperXeTest, givenIoctlHelperXeAndDebugOverrideEnabledWhenGetCpuCachingModeCalledThenOverriddenValueIsReturned) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    drm.memoryInfo.reset(xeIoctlHelper->createMemoryInfo().release());
    ASSERT_NE(nullptr, xeIoctlHelper);

    debugManager.flags.OverrideCpuCaching.set(DRM_XE_GEM_CPU_CACHING_WB);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(), DRM_XE_GEM_CPU_CACHING_WB);

    debugManager.flags.OverrideCpuCaching.set(DRM_XE_GEM_CPU_CACHING_WC);
    EXPECT_EQ(xeIoctlHelper->getCpuCachingMode(), DRM_XE_GEM_CPU_CACHING_WC);
}

TEST(IoctlHelperXeTest, whenCallingVmBindThenPatIndexIsSet) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;
    uint64_t expectedPatIndex = 0xba;

    BindInfo mockBindInfo{};
    mockBindInfo.handle = 0x1234;
    xeIoctlHelper->bindInfo.push_back(mockBindInfo);

    VmBindExtUserFenceT vmBindExtUserFence{};

    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = mockBindInfo.handle;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);
    vmBindParams.patIndex = expectedPatIndex;

    drm.vmBindInputs.clear();
    drm.syncInputs.clear();
    drm.waitUserFenceInputs.clear();
    ASSERT_EQ(0, xeIoctlHelper->vmBind(vmBindParams));
    ASSERT_EQ(1u, drm.vmBindInputs.size());

    EXPECT_EQ(drm.vmBindInputs[0].bind.pat_index, expectedPatIndex);
}
