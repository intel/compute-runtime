/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_impl.h"
#include "shared/test/unit_test/os_interface/linux/ioctl_helper_i915_tests.h"

using namespace NEO;

INSTANTIATE_TYPED_TEST_SUITE_P(Upstream, IoctlHelperI915TablesTest, ::testing::Types<IoctlHelperUpstream>);

using IoctlHelperUpstreamTest = ::testing::Test;

using IoctlHelperTestsUpstream = IoctlHelperUpstreamTest;

struct MockIoctlHelperUpstream : IoctlHelperUpstream {
    using IoctlHelperUpstream::getGpuTime;
    using IoctlHelperUpstream::initializeGetGpuTimeFunction;
    using IoctlHelperUpstream::IoctlHelperUpstream;
    using IoctlHelperUpstream::isSetPatSupported;

    void detectExtSetPatSupport() override {
        detectExtSetPatSupportCallCount++;
        size_t currentIoctlCallCount = ioctlCallCount;
        IoctlHelperUpstream::detectExtSetPatSupport();
        detectExtSetPatSupportIoctlCallCount += ioctlCallCount - currentIoctlCallCount;
    }

    void initializeGetGpuTimeFunction() override {
        initializeGetGpuTimeFunctionCallCount++;
        size_t currentIoctlCallCount = ioctlCallCount;
        IoctlHelperUpstream::initializeGetGpuTimeFunction();
        initializeGetGpuTimeFunctionIoctlCallCount += ioctlCallCount - currentIoctlCallCount;
    }

    int ioctl(DrmIoctl request, void *arg) override {
        ioctlCallCount++;
        if (request == DrmIoctl::gemCreateExt) {
            lastGemCreateContainedSetPat = checkWhetherGemCreateExtContainsSetPat(arg);
            if (overrideGemCreateExtReturnValue.has_value()) {
                return *overrideGemCreateExtReturnValue;
            }
        }
        return IoctlHelperUpstream::ioctl(request, arg);
    }

    bool checkWhetherGemCreateExtContainsSetPat(void *arg) {
        auto &gemCreateExt = *reinterpret_cast<drm_i915_gem_create_ext *>(arg);
        auto pExtensionBase = reinterpret_cast<i915_user_extension *>(gemCreateExt.extensions);
        while (pExtensionBase != nullptr) {
            if (pExtensionBase->name == I915_GEM_CREATE_EXT_SET_PAT) {
                return true;
            }
            pExtensionBase = reinterpret_cast<i915_user_extension *>(pExtensionBase->next_extension);
        }
        return false;
    }

    size_t detectExtSetPatSupportCallCount = 0;
    size_t detectExtSetPatSupportIoctlCallCount = 0;
    size_t initializeGetGpuTimeFunctionCallCount = 0;
    size_t initializeGetGpuTimeFunctionIoctlCallCount = 0;
    size_t ioctlCallCount = 0;
    std::optional<int> overrideGemCreateExtReturnValue{};
    bool lastGemCreateContainedSetPat = false;
};

HWTEST2_F(IoctlHelperUpstreamTest, whenInitializeIsCalledThenDetectExtSetPatSupportFunctionIsCalled, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableGemCreateExtSetPat.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperUpstream mockIoctlHelper{*drm};

    EXPECT_EQ(0u, mockIoctlHelper.detectExtSetPatSupportCallCount);
    EXPECT_FALSE(mockIoctlHelper.lastGemCreateContainedSetPat);
    EXPECT_EQ(0u, mockIoctlHelper.detectExtSetPatSupportIoctlCallCount);

    mockIoctlHelper.overrideGemCreateExtReturnValue = 0;
    mockIoctlHelper.initialize();
    EXPECT_EQ(1u, mockIoctlHelper.detectExtSetPatSupportCallCount);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedSetPat);
    EXPECT_EQ(2u, mockIoctlHelper.detectExtSetPatSupportIoctlCallCount); // create and close
    EXPECT_TRUE(mockIoctlHelper.isSetPatSupported);

    mockIoctlHelper.overrideGemCreateExtReturnValue = -1;
    mockIoctlHelper.initialize();
    EXPECT_EQ(2u, mockIoctlHelper.detectExtSetPatSupportCallCount);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedSetPat);
    EXPECT_EQ(3u, mockIoctlHelper.detectExtSetPatSupportIoctlCallCount); // only create
    EXPECT_FALSE(mockIoctlHelper.isSetPatSupported);

    debugManager.flags.DisableGemCreateExtSetPat.set(true);
    mockIoctlHelper.initialize();
    EXPECT_EQ(3u, mockIoctlHelper.detectExtSetPatSupportCallCount);
    EXPECT_EQ(3u, mockIoctlHelper.detectExtSetPatSupportIoctlCallCount); // no ioctl calls
    EXPECT_FALSE(mockIoctlHelper.isSetPatSupported);
}

HWTEST2_F(IoctlHelperUpstreamTest, whenInitializeIsCalledThenInitializeGetGpuTimeFunctiontFunctionIsCalled, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperUpstream mockIoctlHelper{*drm};

    EXPECT_EQ(0u, mockIoctlHelper.initializeGetGpuTimeFunctionCallCount);
    EXPECT_EQ(0u, mockIoctlHelper.initializeGetGpuTimeFunctionIoctlCallCount);

    mockIoctlHelper.initialize();
    EXPECT_EQ(1u, mockIoctlHelper.initializeGetGpuTimeFunctionCallCount);
    EXPECT_EQ(2u, mockIoctlHelper.initializeGetGpuTimeFunctionIoctlCallCount);
    EXPECT_NE(nullptr, mockIoctlHelper.getGpuTime);
}

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingVmBindAvailabilityThenFalseIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_FALSE(ioctlHelper.isVmBindAvailable());
}

HWTEST2_F(IoctlHelperUpstreamTest, whenChangingBufferBindingThenRequiresUserFenceSetupIsFalse, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperUpstream ioctlHelper{*drm};

    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    EXPECT_FALSE(ioctlHelper.requiresUserFenceSetup(true));
    EXPECT_FALSE(ioctlHelper.requiresUserFenceSetup(false));
}

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingIoctlRequestStringThenProperStringIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCreateExt).c_str(), "DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::queryContextHealth).c_str(), "DRM_IOCTL_I915_GET_RESET_STATS");
}

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingIoctlRequestValueThenProperValueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::queryContextHealth), static_cast<unsigned int>(DRM_IOCTL_I915_GET_RESET_STATS));
}

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingDrmParamValueThenPropertValueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassCompute), 4);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryHwconfigTable), static_cast<int>(DRM_I915_QUERY_HWCONFIG_BLOB));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryComputeSlices), 0);
}

HWTEST2_F(IoctlHelperUpstreamTest, whenCreatingVmControlRegionExtThenNullptrIsReturned, IsAtMostXeCore) {
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

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingFlagsForVmCreateThenZeroIsReturned, IsAtMostXeCore) {
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

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingFlagsForVmBindThenZeroIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                for (auto &bindLock : ::testing::Bool()) {
                    for (auto &readOnlyResource : ::testing::Bool()) {
                        for (auto &resolveResource : ::testing::Bool()) {
                            auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLock, readOnlyResource, resolveResource);
                            EXPECT_EQ(0u, flags);
                        }
                    }
                }
            }
        }
    }
}

HWTEST2_F(IoctlHelperUpstreamTest, whenGettingVmBindExtFromHandlesThenNullptrIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles, 0);
    EXPECT_EQ(nullptr, retVal);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenCreateGemExtThenReturnCorrectValue, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    uint32_t numOfChunks = 0;
    auto ret = ioctlHelper->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt, false);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(1u, drm->numRegions);
    EXPECT_EQ(1024u, drm->createExt.size);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, drm->memRegions.memoryClass);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenCreateGemExtWithDebugFlagThenPrintDebugInfo, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    uint32_t numOfChunks = 0;
    ioctlHelper->createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, std::nullopt, std::nullopt, false);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenSetPatSupportedWhenCreateGemExtThenSetPatExtensionsIsAdded, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperUpstream mockIoctlHelper{*drm};

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    mockIoctlHelper.isSetPatSupported = false;
    auto ret = mockIoctlHelper.createGemExt(memClassInstance, 1, handle, 0, {}, -1, false, 0, std::nullopt, std::nullopt, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, mockIoctlHelper.ioctlCallCount);
    EXPECT_FALSE(mockIoctlHelper.lastGemCreateContainedSetPat);

    mockIoctlHelper.isSetPatSupported = true;
    ret = mockIoctlHelper.createGemExt(memClassInstance, 1, handle, 0, {}, -1, false, 0, std::nullopt, std::nullopt, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2u, mockIoctlHelper.ioctlCallCount);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedSetPat);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenInvalidPatIndexWhenCreateGemExtThenSetPatExtensionsIsNotAdded, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperUpstream mockIoctlHelper{*drm};

    uint32_t handle = 0;
    mockIoctlHelper.isSetPatSupported = true;
    uint64_t invalidPatIndex = CommonConstants::unsupportedPatIndex;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = mockIoctlHelper.createGemExt(memClassInstance, 1, handle, invalidPatIndex, {}, -1, false, 0, std::nullopt, std::nullopt, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, mockIoctlHelper.ioctlCallCount);
    EXPECT_FALSE(mockIoctlHelper.lastGemCreateContainedSetPat);

    invalidPatIndex = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
    ret = mockIoctlHelper.createGemExt(memClassInstance, 1, handle, invalidPatIndex, {}, -1, false, 0, std::nullopt, std::nullopt, false);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2u, mockIoctlHelper.ioctlCallCount);
    EXPECT_FALSE(mockIoctlHelper.lastGemCreateContainedSetPat);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenSetPatSupportedWhenCreateGemExtWithDebugFlagThenPrintDebugInfoWithExtSetPat, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableGemCreateExtSetPat.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperUpstream mockIoctlHelper{*drm};

    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();

    uint32_t handle = 0;
    mockIoctlHelper.isSetPatSupported = true;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    uint32_t numOfChunks = 0;
    uint64_t patIndex = 5;
    mockIoctlHelper.createGemExt(memClassInstance, 1024, handle, patIndex, {}, -1, false, numOfChunks, std::nullopt, std::nullopt, false);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0, pat index: 5 }\nGEM_CREATE_EXT with EXT_MEMORY_REGIONS with EXT_SET_PAT has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

HWTEST2_F(IoctlHelperUpstreamTest, whenDetectExtSetPatSupportIsCalledWithDebugFlagThenPrintCorrectDebugInfo, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableGemCreateExtSetPat.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperUpstream mockIoctlHelper{*drm};

    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();
    mockIoctlHelper.overrideGemCreateExtReturnValue = 0;
    mockIoctlHelper.detectExtSetPatSupport();
    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("EXT_SET_PAT support is: enabled\n");
    EXPECT_EQ(expectedOutput, output);

    capture.captureStdout();
    mockIoctlHelper.overrideGemCreateExtReturnValue = -1;
    mockIoctlHelper.detectExtSetPatSupport();
    output = capture.getCapturedStdout();
    expectedOutput = "EXT_SET_PAT support is: disabled\n";
    EXPECT_EQ(expectedOutput, output);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenClosAllocThenReturnNoneRegion, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(NEO::CacheLevel::level3);

    EXPECT_EQ(CacheRegion::none, cacheRegion);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenClosFreeThenReturnNoneRegion, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(CacheRegion::region2);

    EXPECT_EQ(CacheRegion::none, cacheRegion);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenClosAllocWaysThenReturnZeroWays, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAllocWays(CacheRegion::region2, 3, 10);

    EXPECT_EQ(0, cacheRegion);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenGetAdviseThenReturnCorrectValue, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_EQ(0u, ioctlHelper->getAtomicAdvise(false));
    EXPECT_EQ(0u, ioctlHelper->getAtomicAdvise(true));
    EXPECT_EQ(0u, ioctlHelper->getVmAdviseAtomicAttribute());
    EXPECT_EQ(0u, ioctlHelper->getPreferredLocationAdvise());
    EXPECT_EQ(std::nullopt, ioctlHelper->getPreferredLocationRegion(PreferredLocation::none, 0));
    std::vector<MemoryRegion> memRegion{};
    EXPECT_EQ(0u, ioctlHelper->getPreferredLocationArgs(0, MemAdvise::invalidAdvise, memRegion));
    EXPECT_EQ(0u, ioctlHelper->getPreferredLocationArgs(1, MemAdvise::invalidAdvise, memRegion));
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenSetVmBoAdviseThenReturnTrue, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_TRUE(ioctlHelper->setVmBoAdvise(0, 0, nullptr));
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenSetVmPrefetchThenReturnTrue, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_TRUE(ioctlHelper->setVmPrefetch(0, 0, 0, 0));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenCallingIsEuStallSupportedThenFalseIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_FALSE(ioctlHelper->isEuStallSupported());
}

HWTEST2_F(IoctlHelperTestsUpstream, whenCallingPerfDisableEuStallStreamThenFailueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();
    int32_t invalidFd = -1;
    EXPECT_FALSE(ioctlHelper->perfDisableEuStallStream(&invalidFd));
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenDirectSubmissionEnabledThenNoFlagsAdded, IsAtMostXeCore) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DirectSubmissionDrmContext.set(1);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t vmId = 0u;
    constexpr bool isCooperativeContextRequested = false;
    constexpr bool isDirectSubmissionRequested = true;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(0u, drm->receivedContextCreateFlags);
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenQueryDistancesThenReturnEinval, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    std::vector<DistanceInfo> distanceInfos;
    std::vector<QueryItem> queries(4);
    auto ret = static_cast<IoctlHelperI915 *>(drm->getIoctlHelper())->queryDistances(queries, distanceInfos);
    EXPECT_EQ(0, ret);
    const bool queryUnsupported = std::all_of(queries.begin(), queries.end(),
                                              [](const QueryItem &item) { return item.length == -EINVAL; });
    EXPECT_TRUE(queryUnsupported);
}

HWTEST2_F(IoctlHelperTestsUpstream, whenCreateContextWithAccessCountersIsCalledThenErrorIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    GemContextCreateExt gcc{};
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(EINVAL), ioctlHelper.createContextWithAccessCounters(gcc));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenCreateCooperativeContexIsCalledThenErrorIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    GemContextCreateExt gcc{};
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(EINVAL), ioctlHelper.createCooperativeContext(gcc));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenFillVmBindSetPatThenNothingThrows, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindExtSetPatT vmBindExtSetPat{};
    EXPECT_NO_THROW(ioctlHelper.fillVmBindExtSetPat(vmBindExtSetPat, 0u, 0u));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenFillVmBindUserFenceThenNothingThrows, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindExtUserFenceT vmBindExtUserFence{};
    EXPECT_NO_THROW(ioctlHelper.fillVmBindExtUserFence(vmBindExtUserFence, 0u, 0u, 0u));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenVmBindIsCalledThenZeroIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    VmBindParams vmBindParams{};
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.vmBind(vmBindParams));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenVmUnbindIsCalledThenZeroIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindParams vmBindParams{};
    EXPECT_EQ(0, ioctlHelper.vmUnbind(vmBindParams));
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenCallingPerfOpenEuStallStreamThenFailueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    int32_t invalidFd = -1;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(ioctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenGettingEuStallFdParameterThenZeroIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0u, ioctlHelper.getEuStallFdParameter());
}

HWTEST2_F(IoctlHelperTestsUpstream, whenRegisterUuidIsCalledThenReturnNullHandle, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};

    {
        const auto [retVal, handle] = ioctlHelper.registerUuid("", 0, 0, 0);
        EXPECT_EQ(0, retVal);
        EXPECT_EQ(0u, handle);
    }

    {
        const auto [retVal, handle] = ioctlHelper.registerStringClassUuid("", 0, 0);
        EXPECT_EQ(0, retVal);
        EXPECT_EQ(0u, handle);
    }
}

HWTEST2_F(IoctlHelperTestsUpstream, whenUnregisterUuidIsCalledThenZeroIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.unregisterUuid(0));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenIsContextDebugSupportedIsCalledThenFalseIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(false, ioctlHelper.isContextDebugSupported());
}

HWTEST2_F(IoctlHelperTestsUpstream, whenSetContextDebugFlagIsCalledThenZeroIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.setContextDebugFlag(0));
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenInitializingThenTrueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(true, ioctlHelper.initialize());
}

HWTEST2_F(IoctlHelperTestsUpstream, givenUpstreamWhenGettingFabricLatencyThenFalseIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    uint32_t fabricId = 0, latency = 0, bandwidth = 0;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

HWTEST2_F(IoctlHelperTestsUpstream, whenCallingGetContextHealthThenBatchCountersDecideTheBanReason, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};

    MockResetStats resetStats{};
    resetStats.contextId = 0;
    drm->resetStatsToReturn.push_back(resetStats);

    ContextHealth contextHealth{};
    contextHealth.contextId = 0;

    EXPECT_EQ(0, ioctlHelper.getContextHealth(contextHealth));
    EXPECT_EQ(ContextBanReason::none, contextHealth.banReason);

    // upstream i915 exposes neither a ban status word nor fault details
    EXPECT_FALSE(contextHealth.banned);
    EXPECT_FALSE(contextHealth.faultValid);

    drm->resetStatsToReturn.clear();
    resetStats.batchActive = 1;
    drm->resetStatsToReturn.push_back(resetStats);

    EXPECT_EQ(0, ioctlHelper.getContextHealth(contextHealth));
    EXPECT_EQ(ContextBanReason::gpuHang, contextHealth.banReason);
    EXPECT_FALSE(contextHealth.banned);
}

HWTEST2_F(IoctlHelperTestsUpstream, whenGettingEuDebugInterfaceTypeThenCorrectValueIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getEuDebugInterfaceType(), EuDebugInterfaceType::maxValue);
}

HWTEST2_F(IoctlHelperTestsUpstream, whenCheckingIsDrmFabricSupportedThenFalseIsReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_FALSE(ioctlHelper.isDrmFabricSupported());
}

struct DrmWithUpstreamIoctlHelperTest : public ::testing::Test {
    void SetUp() override {
        drm.ioctlHelper = std::make_unique<IoctlHelperUpstream>(drm);
        drm.ioctlCallsCount = 0;
    }

    MockExecutionEnvironment executionEnvironment{};
    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
};

HWTEST2_F(DrmWithUpstreamIoctlHelperTest, whenRegisterResourceClassesIsCalledThenFalseIsReturned, IsAtMostXeCore) {
    EXPECT_FALSE(drm.registerResourceClasses());
}

HWTEST2_F(DrmWithUpstreamIoctlHelperTest, whenRegisterResourceIsCalledThenNoIoctlIsCalled, IsAtMostXeCore) {
    auto handle = drm.registerResource(DrmResourceClass::maxSize, nullptr, 0);
    EXPECT_EQ(0u, handle);
    drm.unregisterResource(handle);
    EXPECT_EQ(0u, drm.ioctlCallsCount);
}

HWTEST2_F(DrmWithUpstreamIoctlHelperTest, whenRegisterIsaCookieIsCalledThenNoIoctlIsCalled, IsAtMostXeCore) {
    const uint32_t isaHandle = 2;
    EXPECT_EQ(0u, drm.registerIsaCookie(isaHandle));
    EXPECT_EQ(0u, drm.ioctlCallsCount);
}

HWTEST2_F(DrmWithUpstreamIoctlHelperTest, whenCheckingContextDebugSupportThenNoIoctlIsCalled, IsAtMostXeCore) {
    drm.checkContextDebugSupport();
    EXPECT_FALSE(drm.isContextDebugSupported());
    EXPECT_EQ(0u, drm.ioctlCallsCount);
}

HWTEST2_F(DrmWithUpstreamIoctlHelperTest, whenNotifyCommandQueueCreateDestroyAreCalledThenNoIoctlIsCalled, IsAtMostXeCore) {
    EXPECT_EQ(0u, drm.notifyFirstCommandQueueCreated(nullptr, 0));
    EXPECT_EQ(0u, drm.ioctlCallsCount);

    drm.notifyLastCommandQueueDestroyed(0);
    EXPECT_EQ(0u, drm.ioctlCallsCount);
}

HWTEST2_F(DrmWithUpstreamIoctlHelperTest, whenCallingIsDebugAttachAvailableThenFalseIsReturned, IsAtMostXeCore) {
    drm.allowDebugAttachCallBase = true;
    EXPECT_FALSE(drm.isDebugAttachAvailable());
}
