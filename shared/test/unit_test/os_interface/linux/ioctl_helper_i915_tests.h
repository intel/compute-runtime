/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

template <typename IoctlHelperT>
struct IoctlHelperI915TablesTest : ::testing::Test {
    void SetUp() override {
        const bool isI915TestSupported =
            checkProductMatch<IsAtMostXeCore, supportedProductFamilies.size() - 1u>(
                defaultHwInfo->platform.eProductFamily);

        if (!isI915TestSupported) {
            GTEST_SKIP();
        }
    }

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperT ioctlHelper{*drm};
};

TYPED_TEST_SUITE_P(IoctlHelperI915TablesTest);

TYPED_TEST_P(IoctlHelperI915TablesTest, whenGettingCommonIoctlRequestValueThenProperValueIsReturned) {
    auto &ioctlHelper = this->ioctlHelper;
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
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemContextGetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemContextSetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::query), static_cast<unsigned int>(DRM_IOCTL_I915_QUERY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::primeFdToHandle), static_cast<unsigned int>(DRM_IOCTL_PRIME_FD_TO_HANDLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::primeHandleToFd), static_cast<unsigned int>(DRM_IOCTL_PRIME_HANDLE_TO_FD));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjFdToHandle), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjDestroy), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjWait), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjSignal), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_SIGNAL));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjTimelineWait), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::syncObjTimelineSignal), static_cast<unsigned int>(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemMmapOffset), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_MMAP_OFFSET));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfOpen), static_cast<unsigned int>(DRM_IOCTL_I915_PERF_OPEN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfEnable), static_cast<unsigned int>(I915_PERF_IOCTL_ENABLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfDisable), static_cast<unsigned int>(I915_PERF_IOCTL_DISABLE));
}

TYPED_TEST_P(IoctlHelperI915TablesTest, whenGettingCommonIoctlRequestStringThenProperStringIsReturned) {
    auto &ioctlHelper = this->ioctlHelper;
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
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemContextGetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemContextSetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::query).c_str(), "DRM_IOCTL_I915_QUERY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::primeFdToHandle).c_str(), "DRM_IOCTL_PRIME_FD_TO_HANDLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::primeHandleToFd).c_str(), "DRM_IOCTL_PRIME_HANDLE_TO_FD");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjFdToHandle).c_str(), "DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjDestroy).c_str(), "DRM_IOCTL_SYNCOBJ_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjWait).c_str(), "DRM_IOCTL_SYNCOBJ_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjSignal).c_str(), "DRM_IOCTL_SYNCOBJ_SIGNAL");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjTimelineWait).c_str(), "DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::syncObjTimelineSignal).c_str(), "DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemMmapOffset).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmCreate).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmDestroy).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfOpen).c_str(), "DRM_IOCTL_I915_PERF_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfEnable).c_str(), "I915_PERF_IOCTL_ENABLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfDisable).c_str(), "I915_PERF_IOCTL_DISABLE");
}

TYPED_TEST_P(IoctlHelperI915TablesTest, whenGettingCommonDrmParamValueThenProperValueIsReturned) {
    auto &ioctlHelper = this->ioctlHelper;
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
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryMemoryRegions), static_cast<int>(DRM_I915_QUERY_MEMORY_REGIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryTopologyInfo), static_cast<int>(DRM_I915_QUERY_TOPOLOGY_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::tilingNone), static_cast<int>(I915_TILING_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::tilingY), static_cast<int>(I915_TILING_Y));
}

TYPED_TEST_P(IoctlHelperI915TablesTest, whenGettingCommonDrmParamStringThenProperStringIsReturned) {
    auto &ioctlHelper = this->ioctlHelper;
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasPooledEu).c_str(), "I915_PARAM_HAS_POOLED_EU");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramEuTotal).c_str(), "I915_PARAM_EU_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramSubsliceTotal).c_str(), "I915_PARAM_SUBSLICE_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramMinEuInPool).c_str(), "I915_PARAM_MIN_EU_IN_POOL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramCsTimestampFrequency).c_str(), "I915_PARAM_CS_TIMESTAMP_FREQUENCY");
}

REGISTER_TYPED_TEST_SUITE_P(IoctlHelperI915TablesTest,
                            whenGettingCommonIoctlRequestValueThenProperValueIsReturned,
                            whenGettingCommonIoctlRequestStringThenProperStringIsReturned,
                            whenGettingCommonDrmParamValueThenProperValueIsReturned,
                            whenGettingCommonDrmParamStringThenProperStringIsReturned);
