/*
 * Copyright (C) 2021-2025 Intel Corporation
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
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_impl.h"

using namespace NEO;

namespace NEO {
bool getGpuTimeSplit(Drm &drm, uint64_t *timestamp);
bool getGpuTime32(Drm &drm, uint64_t *timestamp);
bool getGpuTime36(Drm &drm, uint64_t *timestamp);
} // namespace NEO

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

TEST(IoctlHelperUpstreamTest, whenInitializeIsCalledThenDetectExtSetPatSupportFunctionIsCalled) {
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

TEST(IoctlHelperUpstreamTest, whenInitializeIsCalledThenInitializeGetGpuTimeFunctiontFunctionIsCalled) {
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

TEST(IoctlHelperUpstreamTest, whenGettingVmBindAvailabilityThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_FALSE(ioctlHelper.isVmBindAvailable());
    EXPECT_FALSE(ioctlHelper.isImmediateVmBindRequired());
}

TEST(IoctlHelperUpstreamTest, whenGettingIfSmallBarConfigIsAllowedThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_TRUE(ioctlHelper.isSmallBarConfigAllowed());
}

TEST(IoctlHelperUpstreamTest, whenChangingBufferBindingThenWaitIsNeverNeeded) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_FALSE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_FALSE(ioctlHelper.isWaitBeforeBindRequired(false));
}

TEST(IoctlHelperUpstreamTest, whenChangingBufferBindingThenWaitIsAddedWhenForced) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperUpstream ioctlHelper{*drm};

    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(false));
}

TEST(IoctlHelperUpstreamTest, whenGettingIoctlRequestStringThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::getparam).c_str(), "DRM_IOCTL_I915_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemExecbuffer2).c_str(), "DRM_IOCTL_I915_GEM_EXECBUFFER2");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemWait).c_str(), "DRM_IOCTL_I915_GEM_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemClose).c_str(), "DRM_IOCTL_GEM_CLOSE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemUserptr).c_str(), "DRM_IOCTL_I915_GEM_USERPTR");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCreate).c_str(), "DRM_IOCTL_I915_GEM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemSetDomain).c_str(), "DRM_IOCTL_I915_GEM_SET_DOMAIN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemSetTiling).c_str(), "DRM_IOCTL_I915_GEM_SET_TILING");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemGetTiling).c_str(), "DRM_IOCTL_I915_GEM_GET_TILING");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemContextCreateExt).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemContextDestroy).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::regRead).c_str(), "DRM_IOCTL_I915_REG_READ");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::getResetStats).c_str(), "DRM_IOCTL_I915_GET_RESET_STATS");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemContextGetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemContextSetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::query).c_str(), "DRM_IOCTL_I915_QUERY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::primeFdToHandle).c_str(), "DRM_IOCTL_PRIME_FD_TO_HANDLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::primeHandleToFd).c_str(), "DRM_IOCTL_PRIME_HANDLE_TO_FD");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjFdToHandle).c_str(), "DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjWait).c_str(), "DRM_IOCTL_SYNCOBJ_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjSignal).c_str(), "DRM_IOCTL_SYNCOBJ_SIGNAL");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjTimelineWait).c_str(), "DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjTimelineSignal).c_str(), "DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCreateExt).c_str(), "DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemMmapOffset).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmCreate).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmDestroy).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfOpen).c_str(), "DRM_IOCTL_I915_PERF_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfEnable).c_str(), "I915_PERF_IOCTL_ENABLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfDisable).c_str(), "I915_PERF_IOCTL_DISABLE");

    EXPECT_THROW(ioctlHelper.getIoctlString(DrmIoctl::dg1GemCreateExt), std::runtime_error);
}

TEST(IoctlHelperUpstreamTest, whenGettingIoctlRequestValueThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::getparam), static_cast<unsigned int>(DRM_IOCTL_I915_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemExecbuffer2), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_EXECBUFFER2));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemWait), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemClose), static_cast<unsigned int>(DRM_IOCTL_GEM_CLOSE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemUserptr), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_USERPTR));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemSetDomain), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_SET_DOMAIN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemSetTiling), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_SET_TILING));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemGetTiling), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_GET_TILING));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemContextCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemContextDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::regRead), static_cast<unsigned int>(DRM_IOCTL_I915_REG_READ));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::getResetStats), static_cast<unsigned int>(DRM_IOCTL_I915_GET_RESET_STATS));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemContextGetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemContextSetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::query), static_cast<unsigned int>(DRM_IOCTL_I915_QUERY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::primeFdToHandle), static_cast<unsigned int>(DRM_IOCTL_PRIME_FD_TO_HANDLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::primeHandleToFd), static_cast<unsigned int>(DRM_IOCTL_PRIME_HANDLE_TO_FD));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjFdToHandle), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjWait), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjSignal), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_SIGNAL));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjTimelineWait), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjTimelineSignal), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemMmapOffset), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_MMAP_OFFSET));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfOpen), static_cast<unsigned int>(DRM_IOCTL_I915_PERF_OPEN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfEnable), static_cast<unsigned int>(I915_PERF_IOCTL_ENABLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfDisable), static_cast<unsigned int>(I915_PERF_IOCTL_DISABLE));

    EXPECT_THROW(ioctlHelper.getIoctlRequestValue(DrmIoctl::dg1GemCreateExt), std::runtime_error);
}

TEST(IoctlHelperUpstreamTest, whenGettingDrmParamStringThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasPooledEu).c_str(), "I915_PARAM_HAS_POOLED_EU");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramEuTotal).c_str(), "I915_PARAM_EU_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramSubsliceTotal).c_str(), "I915_PARAM_SUBSLICE_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramMinEuInPool).c_str(), "I915_PARAM_MIN_EU_IN_POOL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramCsTimestampFrequency).c_str(), "I915_PARAM_CS_TIMESTAMP_FREQUENCY");
}

TEST(IoctlHelperUpstreamTest, whenGettingDrmParamValueThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextCreateExtSetparam), static_cast<int>(I915_CONTEXT_CREATE_EXT_SETPARAM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextCreateFlagsUseExtensions), static_cast<int>(I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextEnginesExtLoadBalance), static_cast<int>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamEngines), static_cast<int>(I915_CONTEXT_PARAM_ENGINES));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamGttSize), static_cast<int>(I915_CONTEXT_PARAM_GTT_SIZE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamPersistence), static_cast<int>(I915_CONTEXT_PARAM_PERSISTENCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamPriority), static_cast<int>(I915_CONTEXT_PARAM_PRIORITY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamRecoverable), static_cast<int>(I915_CONTEXT_PARAM_RECOVERABLE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamSseu), static_cast<int>(I915_CONTEXT_PARAM_SSEU));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::contextParamVm), static_cast<int>(I915_CONTEXT_PARAM_VM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassCompute), 4);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassRender), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassCopy), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassVideo), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassVideoEnhance), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassInvalid), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassInvalidNone), static_cast<int>(I915_ENGINE_CLASS_INVALID_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::execBlt), static_cast<int>(I915_EXEC_BLT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::execDefault), static_cast<int>(I915_EXEC_DEFAULT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::execNoReloc), static_cast<int>(I915_EXEC_NO_RELOC));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::execRender), static_cast<int>(I915_EXEC_RENDER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassDevice), static_cast<int>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassSystem), static_cast<int>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::mmapOffsetWb), static_cast<int>(I915_MMAP_OFFSET_WB));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::mmapOffsetWc), static_cast<int>(I915_MMAP_OFFSET_WC));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramHasPooledEu), static_cast<int>(I915_PARAM_HAS_POOLED_EU));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramEuTotal), static_cast<int>(I915_PARAM_EU_TOTAL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramSubsliceTotal), static_cast<int>(I915_PARAM_SUBSLICE_TOTAL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramMinEuInPool), static_cast<int>(I915_PARAM_MIN_EU_IN_POOL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramCsTimestampFrequency), static_cast<int>(I915_PARAM_CS_TIMESTAMP_FREQUENCY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryEngineInfo), static_cast<int>(DRM_I915_QUERY_ENGINE_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryHwconfigTable), static_cast<int>(DRM_I915_QUERY_HWCONFIG_BLOB));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryMemoryRegions), static_cast<int>(DRM_I915_QUERY_MEMORY_REGIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryTopologyInfo), static_cast<int>(DRM_I915_QUERY_TOPOLOGY_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryComputeSlices), 0);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::tilingNone), static_cast<int>(I915_TILING_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::tilingY), static_cast<int>(I915_TILING_Y));
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
                for (auto &bindLock : ::testing::Bool()) {
                    for (auto &readOnlyResource : ::testing::Bool()) {
                        auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLock, readOnlyResource);
                        EXPECT_EQ(0u, flags);
                    }
                }
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
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles, 0);
    EXPECT_EQ(nullptr, retVal);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenCreateGemExtThenReturnCorrectValue) {
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

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
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

TEST(IoctlHelperTestsUpstream, givenSetPatSupportedWhenCreateGemExtThenSetPatExtensionsIsAdded) {
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

TEST(IoctlHelperTestsUpstream, givenInvalidPatIndexWhenCreateGemExtThenSetPatExtensionsIsNotAdded) {
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

TEST(IoctlHelperTestsUpstream, givenSetPatSupportedWhenCreateGemExtWithDebugFlagThenPrintDebugInfoWithExtSetPat) {
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

TEST(IoctlHelperUpstreamTest, whenDetectExtSetPatSupportIsCalledWithDebugFlagThenPrintCorrectDebugInfo) {
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

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenClosAllocThenReturnNoneRegion) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc(NEO::CacheLevel::level3);

    EXPECT_EQ(CacheRegion::none, cacheRegion);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenClosFreeThenReturnNoneRegion) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(CacheRegion::region2);

    EXPECT_EQ(CacheRegion::none, cacheRegion);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenClosAllocWaysThenReturnZeroWays) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAllocWays(CacheRegion::region2, 3, 10);

    EXPECT_EQ(0, cacheRegion);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenGetAdviseThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_EQ(0u, ioctlHelper->getAtomicAdvise(false));
    EXPECT_EQ(0u, ioctlHelper->getAtomicAdvise(true));
    EXPECT_EQ(0u, ioctlHelper->getPreferredLocationAdvise());
    EXPECT_EQ(std::nullopt, ioctlHelper->getPreferredLocationRegion(PreferredLocation::none, 0));
    EXPECT_EQ(0u, ioctlHelper->getPreferredLocationArgs(MemAdvise::invalidAdvise));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenSetVmBoAdviseThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_TRUE(ioctlHelper->setVmBoAdvise(0, 0, nullptr));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenSetVmPrefetchThenReturnTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_TRUE(ioctlHelper->setVmPrefetch(0, 0, 0, 0));
}

TEST(IoctlHelperTestsUpstream, whenCallingIsEuStallSupportedThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();
    EXPECT_FALSE(ioctlHelper->isEuStallSupported());
}

TEST(IoctlHelperTestsUpstream, whenCallingPerfOpenEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();
    int32_t invalidFd = -1;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(ioctlHelper->perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
}

TEST(IoctlHelperTestsUpstream, whenCallingPerfDisableEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto ioctlHelper = drm->getIoctlHelper();
    int32_t invalidFd = -1;
    EXPECT_FALSE(ioctlHelper->perfDisableEuStallStream(&invalidFd));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenDirectSubmissionEnabledThenNoFlagsAdded) {
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

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryDistancesThenReturnEinval) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    std::vector<DistanceInfo> distanceInfos;
    std::vector<QueryItem> queries(4);
    auto ret = drm->getIoctlHelper()->queryDistances(queries, distanceInfos);
    EXPECT_EQ(0, ret);
    const bool queryUnsupported = std::all_of(queries.begin(), queries.end(),
                                              [](const QueryItem &item) { return item.length == -EINVAL; });
    EXPECT_TRUE(queryUnsupported);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryEngineInfoWithoutDeviceMemoryThenDontUseMultitile) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->memoryInfoQueried = true;
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0, engines);
    auto totalEnginesCount = engineInfo->getEngineInfos().size();
    EXPECT_TRUE(engineInfo->hasEngines());
    EXPECT_EQ(totalEnginesCount, engines.size());
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryEngineInfoWithDeviceMemoryAndDistancesUnsupportedThenDontUseMultitile) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockEngine>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    drm->memoryInfoQueried = true;
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto engineInfo = drm->getEngineInfo();
    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0, engines);
    auto totalEnginesCount = engineInfo->getEngineInfos().size();
    ASSERT_NE(nullptr, engineInfo);
    EXPECT_EQ(totalEnginesCount, engines.size());
}

TEST(IoctlHelperTestsUpstream, whenCreateContextWithAccessCountersIsCalledThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    GemContextCreateExt gcc{};
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(EINVAL), ioctlHelper.createContextWithAccessCounters(gcc));
}

TEST(IoctlHelperTestsUpstream, whenCreateCooperativeContexIsCalledThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    GemContextCreateExt gcc{};
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(EINVAL), ioctlHelper.createCooperativeContext(gcc));
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
    EXPECT_EQ(0, ioctlHelper.vmBind(vmBindParams));
}

TEST(IoctlHelperTestsUpstream, whenVmUnbindIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    VmBindParams vmBindParams{};
    EXPECT_EQ(0, ioctlHelper.vmUnbind(vmBindParams));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenCallingPerfOpenEuStallStreamThenFailueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    int32_t invalidFd = -1;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(ioctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
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

TEST(IoctlHelperTestsUpstream, whenUnregisterUuidIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.unregisterUuid(0));
}

TEST(IoctlHelperTestsUpstream, whenIsContextDebugSupportedIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(false, ioctlHelper.isContextDebugSupported());
}

TEST(IoctlHelperTestsUpstream, whenSetContextDebugFlagIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(0, ioctlHelper.setContextDebugFlag(0));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenInitializingThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    EXPECT_EQ(true, ioctlHelper.initialize());
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenGettingFabricLatencyThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    uint32_t fabricId = 0, latency = 0, bandwidth = 0;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

TEST(IoctlHelperTestsUpstream, WhenSetupIpVersionIsCalledThenIpVersionIsCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};

    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto &compilerProductHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    ioctlHelper.setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

TEST(IoctlHelperTestsUpstream, whenGettingGpuTimeThenSucceeds) {
    MockExecutionEnvironment executionEnvironment{};
    auto drm = std::make_unique<DrmMockTime>(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    ASSERT_EQ(true, ioctlHelper.initialize());

    uint64_t time = 0;
    auto success = getGpuTime32(*drm.get(), &time);
    EXPECT_TRUE(success);
    EXPECT_NE(0ULL, time);
    success = getGpuTime36(*drm.get(), &time);
    EXPECT_TRUE(success);
    EXPECT_NE(0ULL, time);
    success = getGpuTimeSplit(*drm.get(), &time);
    EXPECT_TRUE(success);
    EXPECT_NE(0ULL, time);
}

TEST(IoctlHelperTestsUpstream, givenInvalidDrmWhenGettingGpuTimeThenFails) {
    MockExecutionEnvironment executionEnvironment{};
    auto drm = std::make_unique<DrmMockFail>(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperUpstream ioctlHelper{*drm};
    ASSERT_EQ(true, ioctlHelper.initialize());

    uint64_t time = 0;
    auto success = getGpuTime32(*drm.get(), &time);
    EXPECT_FALSE(success);
    success = getGpuTime36(*drm.get(), &time);
    EXPECT_FALSE(success);
    success = getGpuTimeSplit(*drm.get(), &time);
    EXPECT_FALSE(success);
}

TEST(IoctlHelperTestsUpstream, whenGettingTimeThenTimeIsCorrect) {
    MockExecutionEnvironment executionEnvironment{};
    auto drm = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    MockIoctlHelperUpstream ioctlHelper{*drm};
    ASSERT_EQ(true, ioctlHelper.initialize());

    {
        EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTime36);
    }

    {
        drm->ioctlRes = -1;
        ioctlHelper.initializeGetGpuTimeFunction();
        EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTime32);
    }

    DrmMockCustom::IoctlResExt ioctlToPass = {1, 0};
    {
        drm->reset();
        drm->ioctlRes = -1;
        drm->ioctlResExt = &ioctlToPass; // 2nd ioctl is successful
        ioctlHelper.initializeGetGpuTimeFunction();
        EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTimeSplit);
        drm->ioctlResExt = &drm->none;
    }
}

TEST(IoctlHelperTestsUpstream, givenInitializeGetGpuTimeFunctionNotCalledWhenSetGpuCpuTimesIsCalledThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockCustom::create(rootDeviceEnvironment);
    IoctlHelperUpstream ioctlHelper{*drm};

    drm->ioctlRes = -1;
    TimeStampData pGpuCpuTime{};
    std::unique_ptr<MockOSTimeLinux> osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    auto ret = ioctlHelper.setGpuCpuTimes(&pGpuCpuTime, osTime.get());
    EXPECT_EQ(false, ret);
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenGetFdFromVmExportIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
    uint32_t vmId = 0, flags = 0;
    int32_t fd = 0;
    EXPECT_FALSE(ioctlHelper.getFdFromVmExport(vmId, flags, &fd));
}

TEST(IoctlHelperTestsUpstream, whenCallingGetResetStatsThenSuccessIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};

    ResetStats resetStats{};
    resetStats.contextId = 0;
    drm->resetStatsToReturn.push_back(resetStats);

    EXPECT_EQ(0, ioctlHelper.getResetStats(resetStats, nullptr, nullptr));
}

TEST(IoctlHelperTestsUpstream, whenCallingGetStatusAndFlagsForResetStatsThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(0u, ioctlHelper.getStatusForResetStats(true));
    EXPECT_EQ(0u, ioctlHelper.getStatusForResetStats(false));

    EXPECT_FALSE(ioctlHelper.validPageFault(0u));
}

TEST(IoctlHelperTestsUpstream, givenUpstreamWhenQueryDeviceParamsIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};

    uint32_t moduleId = 0;
    uint16_t serverType = 0;
    EXPECT_FALSE(ioctlHelper.queryDeviceParams(&moduleId, &serverType));
}

TEST(IoctlHelperTestsUpstream, givenPrelimWhenQueryDeviceCapsIsCalledThenNulloptIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};

    EXPECT_EQ(ioctlHelper.queryDeviceCaps(), std::nullopt);
}
