/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_impl.h"

using namespace NEO;

TEST(IoctlHelperUpstreamTest, whenGettingVmBindAvailabilityThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_FALSE(ioctlHelper.isVmBindAvailable(drm.get()));
}

TEST(IoctlHelperUpstreamTest, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::Getparam), static_cast<unsigned int>(DRM_IOCTL_I915_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemExecbuffer2), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_EXECBUFFER2));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemWait), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemClose), static_cast<unsigned int>(DRM_IOCTL_GEM_CLOSE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemUserptr), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_USERPTR));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemSetDomain), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_SET_DOMAIN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemSetTiling), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_SET_TILING));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemGetTiling), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_GET_TILING));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::RegRead), static_cast<unsigned int>(DRM_IOCTL_I915_REG_READ));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GetResetStats), static_cast<unsigned int>(DRM_IOCTL_I915_GET_RESET_STATS));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextGetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextSetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::Query), static_cast<unsigned int>(DRM_IOCTL_I915_QUERY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::PrimeFdToHandle), static_cast<unsigned int>(DRM_IOCTL_PRIME_FD_TO_HANDLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::PrimeHandleToFd), static_cast<unsigned int>(DRM_IOCTL_PRIME_HANDLE_TO_FD));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemMmapOffset), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_MMAP_OFFSET));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_DESTROY));

    EXPECT_THROW(ioctlHelper.getIoctlRequestValue(DrmIoctl::DG1GemCreateExt), std::runtime_error);
}

TEST(IoctlHelperUpstreamTest, whenGettingDrmParamValueThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassCompute), 4);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassRender), static_cast<int>(I915_ENGINE_CLASS_RENDER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassCopy), static_cast<int>(I915_ENGINE_CLASS_COPY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassVideo), static_cast<int>(I915_ENGINE_CLASS_VIDEO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassVideoEnhance), static_cast<int>(I915_ENGINE_CLASS_VIDEO_ENHANCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassInvalid), static_cast<int>(I915_ENGINE_CLASS_INVALID));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassInvalidNone), static_cast<int>(I915_ENGINE_CLASS_INVALID_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecBlt), static_cast<int>(I915_EXEC_BLT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecDefault), static_cast<int>(I915_EXEC_DEFAULT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecNoReloc), static_cast<int>(I915_EXEC_NO_RELOC));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecRender), static_cast<int>(I915_EXEC_RENDER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::MemoryClassDevice), static_cast<int>(I915_MEMORY_CLASS_DEVICE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::MemoryClassSystem), static_cast<int>(I915_MEMORY_CLASS_SYSTEM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamChipsetId), static_cast<int>(I915_PARAM_CHIPSET_ID));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamRevision), static_cast<int>(I915_PARAM_REVISION));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasExecSoftpin), static_cast<int>(I915_PARAM_HAS_EXEC_SOFTPIN));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasPooledEu), static_cast<int>(I915_PARAM_HAS_POOLED_EU));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasScheduler), static_cast<int>(I915_PARAM_HAS_SCHEDULER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamEuTotal), static_cast<int>(I915_PARAM_EU_TOTAL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamSubsliceTotal), static_cast<int>(I915_PARAM_SUBSLICE_TOTAL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamMinEuInPool), static_cast<int>(I915_PARAM_MIN_EU_IN_POOL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamCsTimestampFrequency), static_cast<int>(I915_PARAM_CS_TIMESTAMP_FREQUENCY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryEngineInfo), static_cast<int>(DRM_I915_QUERY_ENGINE_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryHwconfigTable), static_cast<int>(DRM_I915_QUERY_HWCONFIG_TABLE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryMemoryRegions), static_cast<int>(DRM_I915_QUERY_MEMORY_REGIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryComputeSlices), 0);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::TilingNone), static_cast<int>(I915_TILING_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::TilingY), static_cast<int>(I915_TILING_Y));
}

TEST(IoctlHelperUpstreamTest, whenCreatingVmControlRegionExtThenNullptrIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    std::optional<MemoryClassInstance> regionInstanceClass = MemoryClassInstance{};

    EXPECT_TRUE(regionInstanceClass.has_value());
    EXPECT_EQ(nullptr, ioctlHelper.createVmControlExtRegion(regionInstanceClass));

    regionInstanceClass = {};
    EXPECT_FALSE(regionInstanceClass.has_value());
    EXPECT_EQ(nullptr, ioctlHelper.createVmControlExtRegion(regionInstanceClass));
}

TEST(IoctlHelperUpstreamTest, whenGettingFlagsForVmCreateThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    for (auto &disableScratch : ::testing::Bool()) {
        for (auto &enablePageFault : ::testing::Bool()) {
            for (auto &useVmBind : ::testing::Bool()) {
                auto flags = ioctlHelper.getFlagsForVmCreate(disableScratch, enablePageFault, useVmBind);
                EXPECT_EQ(0u, flags);
            }
        }
    }
}

TEST(IoctlHelperUpstreamTest, whenGettingFlagsForVmBindThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident);
                EXPECT_EQ(0u, flags);
            }
        }
    }
}

TEST(IoctlHelperUpstreamTest, whenGettingVmBindExtFromHandlesThenNullptrIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles);
    EXPECT_EQ(nullptr, retVal);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenCreateGemExtThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = ioctlHelper->createGemExt(drm.get(), memClassInstance, 1024, handle, {});

    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(1u, drm->numRegions);
    EXPECT_EQ(1024u, drm->createExt.size);
    EXPECT_EQ(I915_MEMORY_CLASS_DEVICE, drm->memRegions.memoryClass);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    testing::internal::CaptureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{I915_MEMORY_CLASS_DEVICE, 0}};
    ioctlHelper->createGemExt(drm.get(), memClassInstance, 1024, handle, {});

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenClosAllocThenReturnNoneRegion) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(drm.get());

    EXPECT_EQ(CacheRegion::None, cacheRegion);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenClosFreeThenReturnNoneRegion) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(drm.get(), CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::None, cacheRegion);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenClosAllocWaysThenReturnZeroWays) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAllocWays(drm.get(), CacheRegion::Region2, 3, 10);

    EXPECT_EQ(0, cacheRegion);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenGetAdviseThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_EQ(0u, ioctlHelper->getAtomicAdvise(false));
    EXPECT_EQ(0u, ioctlHelper->getAtomicAdvise(true));
    EXPECT_EQ(0u, ioctlHelper->getPreferredLocationAdvise());
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenSetVmBoAdviseThenReturnTrue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_TRUE(ioctlHelper->setVmBoAdvise(drm.get(), 0, 0, nullptr));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenSetVmPrefetchThenReturnTrue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_TRUE(ioctlHelper->setVmPrefetch(drm.get(), 0, 0, 0));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenDirectSubmissionEnabledThenNoFlagsAdded) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DirectSubmissionDrmContext.set(1);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t vmId = 0u;
    constexpr bool isCooperativeContextRequested = false;
    constexpr bool isDirectSubmissionRequested = true;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(0u, drm->receivedContextCreateFlags);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryDistancesThenReturnEinval) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    std::vector<DistanceInfo> distanceInfos;
    std::vector<QueryItem> queries(4);
    auto ret = drm->getIoctlHelper()->queryDistances(drm.get(), queries, distanceInfos);
    EXPECT_EQ(0u, ret);
    const bool queryUnsupported = std::all_of(queries.begin(), queries.end(),
                                              [](const QueryItem &item) { return item.length == -EINVAL; });
    EXPECT_TRUE(queryUnsupported);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryEngineInfoWithoutDeviceMemoryThenDontUseMultitile) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions));
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto engineInfo = drm->getEngineInfo();
    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0, engines);
    auto totalEnginesCount = engineInfo->engines.size();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(totalEnginesCount, engines.size());
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryEngineInfoWithDeviceMemoryAndDistancesUnsupportedThenDontUseMultitile) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions));
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto engineInfo = drm->getEngineInfo();
    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0, engines);
    auto totalEnginesCount = engineInfo->engines.size();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(totalEnginesCount, engines.size());
}

TEST(IoctlHelperTestsUpstream, whenCreateContextWithAccessCountersIsCalledThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    GemContextCreateExt gcc{};
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(EINVAL), ioctlHelper.createContextWithAccessCounters(drm.get(), gcc));
}

TEST(IoctlHelperTestsUpstream, whenCreateCooperativeContexIsCalledThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    GemContextCreateExt gcc{};
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(EINVAL), ioctlHelper.createCooperativeContext(drm.get(), gcc));
}

TEST(IoctlHelperTestsUpstream, whenFillVmBindSetPatThenNothingThrows) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindExtSetPatT vmBindExtSetPat{};
    EXPECT_NO_THROW(ioctlHelper.fillVmBindExtSetPat(vmBindExtSetPat, 0u, 0u));
}

TEST(IoctlHelperTestsUpstream, whenFillVmBindUserFenceThenNothingThrows) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindExtUserFenceT vmBindExtUserFence{};
    EXPECT_NO_THROW(ioctlHelper.fillVmBindExtUserFence(vmBindExtUserFence, 0u, 0u, 0u));
}

TEST(IoctlHelperTestsUpstream, whenVmBindIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    VmBindParams vmBindParams{};
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.vmBind(drm.get(), vmBindParams));
}

TEST(IoctlHelperTestsUpstream, whenVmUnbindIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindParams vmBindParams{};
    EXPECT_EQ(0, ioctlHelper.vmUnbind(drm.get(), vmBindParams));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenGettingEuStallPropertiesThenFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    std::array<uint64_t, 12u> properties = {};
    EXPECT_FALSE(ioctlHelper.getEuStallProperties(properties, 0x101, 0x102, 0x103, 1, 1));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenGettingEuStallFdParameterThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0u, ioctlHelper.getEuStallFdParameter());
}

TEST(IoctlHelperTestsUpstream, whenRegisterUuidIsCalledThenReturnNullHandle) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};

    {
        const auto [retVal, handle] = ioctlHelper.registerUuid(drm.get(), "", 0, 0, 0);
        EXPECT_EQ(0u, retVal);
        EXPECT_EQ(0u, handle);
    }

    {
        const auto [retVal, handle] = ioctlHelper.registerStringClassUuid(drm.get(), "", 0, 0);
        EXPECT_EQ(0u, retVal);
        EXPECT_EQ(0u, handle);
    }
}

TEST(IoctlHelperTestsUpstream, whenUnregisterUuidIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.unregisterUuid(drm.get(), 0));
}

TEST(IoctlHelperTestsUpstream, whenIsContextDebugSupportedIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(false, ioctlHelper.isContextDebugSupported(drm.get()));
}

TEST(IoctlHelperTestsUpstream, whenSetContextDebugFlagIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.setContextDebugFlag(drm.get(), 0));
}
