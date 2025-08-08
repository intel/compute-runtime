/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include <fcntl.h>

using namespace NEO;

extern std::vector<uint64_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions);
extern std::vector<uint64_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines);

namespace NEO {
bool getGpuTimeSplitted(Drm &drm, uint64_t *timestamp);
bool getGpuTime32(Drm &drm, uint64_t *timestamp);
bool getGpuTime36(Drm &drm, uint64_t *timestamp);
} // namespace NEO

struct MockIoctlHelperPrelim : public IoctlHelperPrelim20 {
    using IoctlHelperPrelim20::getGpuTime;
    using IoctlHelperPrelim20::initializeGetGpuTimeFunction;
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    using IoctlHelperPrelim20::translateToMemoryRegions;
};

struct IoctlPrelimHelperTests : ::testing::Test {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    MockIoctlHelperPrelim ioctlHelper{*drm};
};

TEST_F(IoctlPrelimHelperTests, whenGettingIfImmediateVmBindIsRequiredThenFalseIsReturned) {
    EXPECT_FALSE(ioctlHelper.isImmediateVmBindRequired());
}

TEST_F(IoctlPrelimHelperTests, whenGettingIfSmallBarConfigIsAllowedThenTrueIsReturned) {
    EXPECT_TRUE(ioctlHelper.isSmallBarConfigAllowed());
}

TEST_F(IoctlPrelimHelperTests, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
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
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::getResetStatsPrelim), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GET_RESET_STATS));
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
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmBind), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_BIND));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmUnbind), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemWaitUserFence), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCreateExt), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmAdvise), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmPrefetch), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::uuidRegister), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_UUID_REGISTER));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::uuidUnregister), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::debuggerOpen), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemClosReserve), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemClosFree), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemCacheReserve), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemMmapOffset), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_MMAP_OFFSET));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::gemVmDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfOpen), static_cast<unsigned int>(DRM_IOCTL_I915_PERF_OPEN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfEnable), static_cast<unsigned int>(I915_PERF_IOCTL_ENABLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::perfDisable), static_cast<unsigned int>(I915_PERF_IOCTL_DISABLE));

    EXPECT_THROW(ioctlHelper.getIoctlRequestValue(DrmIoctl::dg1GemCreateExt), std::runtime_error);
}

TEST_F(IoctlPrelimHelperTests, whenGettingDrmParamStringThenProperStringIsReturned) {
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasPooledEu).c_str(), "I915_PARAM_HAS_POOLED_EU");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramEuTotal).c_str(), "I915_PARAM_EU_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramSubsliceTotal).c_str(), "I915_PARAM_SUBSLICE_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramMinEuInPool).c_str(), "I915_PARAM_MIN_EU_IN_POOL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramCsTimestampFrequency).c_str(), "I915_PARAM_CS_TIMESTAMP_FREQUENCY");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasVmBind).c_str(), "PRELIM_I915_PARAM_HAS_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::paramHasPageFault).c_str(), "PRELIM_I915_PARAM_HAS_PAGE_FAULT");
}

TEST_F(IoctlPrelimHelperTests, whenGettingIoctlRequestStringThenProperStringIsReturned) {
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
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::getResetStatsPrelim).c_str(), "PRELIM_DRM_IOCTL_I915_GET_RESET_STATS");
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
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmBind).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmUnbind).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemWaitUserFence).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCreateExt).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmAdvise).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmPrefetch).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::uuidRegister).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_REGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::uuidUnregister).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::debuggerOpen).c_str(), "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemClosReserve).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemClosFree).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemCacheReserve).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemMmapOffset).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmCreate).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::gemVmDestroy).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfOpen).c_str(), "DRM_IOCTL_I915_PERF_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfEnable).c_str(), "I915_PERF_IOCTL_ENABLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::perfDisable).c_str(), "I915_PERF_IOCTL_DISABLE");

    EXPECT_THROW(ioctlHelper.getIoctlString(DrmIoctl::dg1GemCreateExt), std::runtime_error);
}

TEST_F(IoctlPrelimHelperTests, whenGettingDrmParamValueThenPropertValueIsReturned) {
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
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::engineClassCompute), static_cast<int>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE));
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
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramHasVmBind), static_cast<int>(PRELIM_I915_PARAM_HAS_VM_BIND));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::paramHasPageFault), static_cast<int>(PRELIM_I915_PARAM_HAS_PAGE_FAULT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryEngineInfo), static_cast<int>(DRM_I915_QUERY_ENGINE_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryHwconfigTable), static_cast<int>(PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryMemoryRegions), static_cast<int>(DRM_I915_QUERY_MEMORY_REGIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryComputeSlices), static_cast<int>(PRELIM_DRM_I915_QUERY_COMPUTE_SUBSLICES));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::queryTopologyInfo), static_cast<int>(DRM_I915_QUERY_TOPOLOGY_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::tilingNone), static_cast<int>(I915_TILING_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::tilingY), static_cast<int>(I915_TILING_Y));
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToMemoryRegionsThenReturnSameData) {
    std::vector<MemoryRegion> expectedMemRegions(2);
    expectedMemRegions[0].region.memoryClass = prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_SYSTEM;
    expectedMemRegions[0].region.memoryInstance = 0;
    expectedMemRegions[0].probedSize = 1024;
    expectedMemRegions[0].cpuVisibleSize = 1024;
    expectedMemRegions[1].region.memoryClass = prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE;
    expectedMemRegions[1].region.memoryInstance = 0;
    expectedMemRegions[1].probedSize = 1024;
    expectedMemRegions[1].cpuVisibleSize = 256;

    auto regionInfo = getRegionInfo(expectedMemRegions);

    auto memRegions = ioctlHelper.translateToMemoryRegions(regionInfo);
    EXPECT_EQ(2u, memRegions.size());
    for (uint32_t i = 0; i < memRegions.size(); i++) {
        EXPECT_EQ(expectedMemRegions[i].region.memoryClass, memRegions[i].region.memoryClass);
        EXPECT_EQ(expectedMemRegions[i].region.memoryInstance, memRegions[i].region.memoryInstance);
        EXPECT_EQ(expectedMemRegions[i].probedSize, memRegions[i].probedSize);
        EXPECT_EQ(expectedMemRegions[i].unallocatedSize, memRegions[i].unallocatedSize);
        EXPECT_EQ(expectedMemRegions[i].cpuVisibleSize, memRegions[i].cpuVisibleSize);
    }
}

TEST_F(IoctlPrelimHelperTests, givenEmptyRegionInstanceClassWhenCreatingVmControlRegionExtThenNullptrIsReturned) {
    std::optional<MemoryClassInstance> regionInstanceClass{};

    EXPECT_FALSE(regionInstanceClass.has_value());
    EXPECT_EQ(nullptr, ioctlHelper.createVmControlExtRegion(regionInstanceClass));
}

TEST_F(IoctlPrelimHelperTests, givenValidRegionInstanceClassWhenCreatingVmControlRegionExtThenProperStructIsReturned) {
    std::optional<MemoryClassInstance> regionInstanceClass = MemoryClassInstance{prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE, 2};

    EXPECT_TRUE(regionInstanceClass.has_value());

    auto retVal = ioctlHelper.createVmControlExtRegion(regionInstanceClass);

    EXPECT_NE(nullptr, retVal);

    auto regionExt = reinterpret_cast<prelim_drm_i915_gem_vm_region_ext *>(retVal.get());

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_GEM_VM_CONTROL_EXT_REGION), regionExt->base.name);
    EXPECT_EQ(static_cast<uint32_t>(prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE), regionExt->region.memory_class);
    EXPECT_EQ(2u, regionExt->region.memory_instance);
}

TEST_F(IoctlPrelimHelperTests, whenGettingFlagsForVmCreateThenProperValueIsReturned) {
    for (auto &disableScratch : ::testing::Bool()) {
        for (auto &enablePageFault : ::testing::Bool()) {
            for (auto &useVmBind : ::testing::Bool()) {
                auto flags = ioctlHelper.getFlagsForVmCreate(disableScratch, enablePageFault, useVmBind);
                if (disableScratch) {
                    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH), (flags & PRELIM_I915_VM_CREATE_FLAGS_DISABLE_SCRATCH));
                }
                if (enablePageFault) {
                    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT), (flags & PRELIM_I915_VM_CREATE_FLAGS_ENABLE_PAGE_FAULT));
                }
                if (useVmBind) {
                    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND), (flags & PRELIM_I915_VM_CREATE_FLAGS_USE_VM_BIND));
                }
                if (disableScratch || enablePageFault || useVmBind) {
                    EXPECT_NE(0u, flags);
                } else {
                    EXPECT_EQ(0u, flags);
                }
            }
        }
    }
}

TEST_F(IoctlPrelimHelperTests, whenGettingFlagsForVmBindThenProperValuesAreReturned) {
    for (auto &bindCapture : ::testing::Bool()) {
        for (auto &bindImmediate : ::testing::Bool()) {
            for (auto &bindMakeResident : ::testing::Bool()) {
                for (auto &bindLockedMemory : ::testing::Bool()) {
                    for (auto &readOnlyResource : ::testing::Bool()) {
                        auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident, bindLockedMemory, readOnlyResource);
                        if (bindCapture) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_CAPTURE, (flags & PRELIM_I915_GEM_VM_BIND_CAPTURE));
                        }
                        if (bindImmediate) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_IMMEDIATE, (flags & PRELIM_I915_GEM_VM_BIND_IMMEDIATE));
                        }
                        if (bindMakeResident || bindLockedMemory) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT, (flags & PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT));
                        }
                        if (readOnlyResource) {
                            EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_READONLY, (flags & PRELIM_I915_GEM_VM_BIND_READONLY));
                        }
                        if (flags == 0) {
                            EXPECT_FALSE(bindCapture);
                            EXPECT_FALSE(bindImmediate);
                            EXPECT_FALSE(bindMakeResident);
                            EXPECT_FALSE(bindLockedMemory);
                            EXPECT_FALSE(readOnlyResource);
                        }
                    }
                }
            }
        }
    }
}

TEST_F(IoctlPrelimHelperTests, givenIoctlHelperisVmBindPatIndexExtSupportedReturnsTrue) {
    ASSERT_EQ(true, ioctlHelper.isVmBindPatIndexExtSupported());
}

TEST_F(IoctlPrelimHelperTests, givenIoctlHelperSetVmSharedSystemMemAdviseReturnsTrue) {
    ASSERT_EQ(true, ioctlHelper.setVmSharedSystemMemAdvise(0u, 0u, 0u, 0u, {0u}));
}

TEST_F(IoctlPrelimHelperTests, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned) {
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles, 0);
    auto vmBindExt = reinterpret_cast<prelim_drm_i915_vm_bind_ext_uuid *>(retVal.get());

    for (size_t i = 0; i < bindExtHandles.size(); i++) {
        EXPECT_EQ(bindExtHandles[i], vmBindExt[i].uuid_handle);
        EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_UUID), vmBindExt[i].base.name);
    }

    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[1]), vmBindExt[0].base.next_extension);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[2]), vmBindExt[1].base.next_extension);
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetDirectSubmissionFlagThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_I915_CONTEXT_CREATE_FLAGS_LONG_RUNNING, ioctlHelper.getDirectSubmissionFlag());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToEngineCapsThenReturnSameData) {
    std::vector<EngineCapabilities> expectedEngines(2);
    expectedEngines[0] = {{static_cast<uint16_t>(ioctlHelper.getDrmParamValue(DrmParam::engineClassRender)), 0}, {true, false}};
    expectedEngines[1] = {{static_cast<uint16_t>(ioctlHelper.getDrmParamValue(DrmParam::engineClassCopy)), 1}, {false, true}};

    auto engineInfo = getEngineInfo(expectedEngines);

    auto engines = ioctlHelper.translateToEngineCaps(engineInfo);
    EXPECT_EQ(2u, engines.size());
    for (uint32_t i = 0; i < engines.size(); i++) {
        EXPECT_EQ(expectedEngines[i].engine.engineClass, engines[i].engine.engineClass);
        EXPECT_EQ(expectedEngines[i].engine.engineInstance, engines[i].engine.engineInstance);
        EXPECT_EQ(expectedEngines[i].capabilities.copyClassSaturateLink, engines[i].capabilities.copyClassSaturateLink);
        EXPECT_EQ(expectedEngines[i].capabilities.copyClassSaturatePCIE, engines[i].capabilities.copyClassSaturatePCIE);
    }
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGettingFlagForWaitUserFenceSoftThenProperFlagIsReturned) {
    EXPECT_EQ(PRELIM_I915_UFENCE_WAIT_SOFT, ioctlHelper.getWaitUserFenceSoftFlag());
}

TEST_F(IoctlPrelimHelperTests, givenValidInputWhenFillVmBindSetPatThenProperValuesAreSet) {
    VmBindExtSetPatT vmBindExtSetPat{};
    prelim_drm_i915_vm_bind_ext_set_pat prelimVmBindExtSetPat{};

    uint64_t expectedPatIndex = 2;
    uint64_t expectedNextExtension = 3;
    ioctlHelper.fillVmBindExtSetPat(vmBindExtSetPat, expectedPatIndex, expectedNextExtension);

    memcpy_s(&prelimVmBindExtSetPat, sizeof(prelimVmBindExtSetPat), vmBindExtSetPat, sizeof(vmBindExtSetPat));

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_SET_PAT), prelimVmBindExtSetPat.base.name);
    EXPECT_EQ(expectedPatIndex, prelimVmBindExtSetPat.pat_index);
    EXPECT_EQ(expectedNextExtension, prelimVmBindExtSetPat.base.next_extension);
}

TEST_F(IoctlPrelimHelperTests, givenValidInputWhenFillVmBindUserFenceThenProperValuesAreSet) {
    VmBindExtUserFenceT vmBindExtUserFence{};
    prelim_drm_i915_vm_bind_ext_user_fence prelimVmBindExtUserFence{};

    uint64_t expectedAddress = 0xdead;
    uint64_t expectedValue = 0xc0de;
    uint64_t expectedNextExtension = 1234;
    uint64_t expectedSize = sizeof(prelimVmBindExtUserFence.base) + sizeof(uint64_t) * 3;
    ioctlHelper.fillVmBindExtUserFence(vmBindExtUserFence, expectedAddress, expectedValue, expectedNextExtension);

    memcpy_s(&prelimVmBindExtUserFence, sizeof(prelimVmBindExtUserFence), vmBindExtUserFence, sizeof(vmBindExtUserFence));

    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_VM_BIND_EXT_USER_FENCE), prelimVmBindExtUserFence.base.name);
    EXPECT_EQ(expectedAddress, prelimVmBindExtUserFence.addr);
    EXPECT_EQ(expectedValue, prelimVmBindExtUserFence.val);
    EXPECT_EQ(expectedNextExtension, prelimVmBindExtUserFence.base.next_extension);
    EXPECT_EQ(expectedSize, sizeof(prelimVmBindExtUserFence));
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenCallingIsEuStallSupportedThenTrueIsReturned) {
    EXPECT_TRUE(ioctlHelper.isEuStallSupported());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenCallingPerfOpenEuStallStreamWithInvalidArgumentsThenFailureReturned) {
    int32_t invalidStream = -1;
    DrmMock *mockDrm = reinterpret_cast<DrmMock *>(drm.get());
    mockDrm->failPerfOpen = true;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(ioctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidStream));
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenGettingEuStallFdParameterThenCorrectIoctlValueIsReturned) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_PERF_FLAG_FD_EU_STALL), ioctlHelper.getEuStallFdParameter());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenQueryDeviceParamsIsCalledThenFalseIsReturned) {
    uint32_t moduleId = 0;
    uint16_t serverType = 0;
    EXPECT_FALSE(ioctlHelper.queryDeviceParams(&moduleId, &serverType));
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenQueryDeviceCapsIsCalledThenNullptrIsReturned) {
    EXPECT_EQ(ioctlHelper.queryDeviceCaps(), nullptr);
}

struct MockIoctlHelperPrelim20 : IoctlHelperPrelim20 {
    using IoctlHelperPrelim20::createGemExt;
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    int ioctl(DrmIoctl request, void *arg) override {
        ioctlCallCount++;
        if (request == DrmIoctl::gemCreateExt) {
            lastGemCreateContainedMemPolicy = checkWhetherGemCreateExtContainsMemPolicy(arg);
            if (overrideGemCreateExtReturnValue.has_value()) {
                return *overrideGemCreateExtReturnValue;
            }
        }
        if (request == DrmIoctl::getResetStatsPrelim) {
            resetStatsPrelimCalled++;
            if (overrideResetStatsPrelim.has_value()) {
                auto resetStats = reinterpret_cast<ResetStats *>(arg);
                *resetStats = std::get<0>(overrideResetStatsPrelim.value());
                auto resetStatsPrelim = reinterpret_cast<prelim_drm_i915_reset_stats *>(arg);
                resetStatsPrelim->status = std::get<1>(overrideResetStatsPrelim.value());
                auto fault = std::get<2>(overrideResetStatsPrelim.value());
                resetStatsPrelim->fault = {fault.addr, fault.type, fault.level, fault.access, fault.flags};
                return overrideResetStatsPrelimReturnValue;
            }
        }
        if (request == DrmIoctl::getResetStats) {
            resetStatsCalled++;
            if (overrideResetStats.has_value()) {
                auto resetStats = reinterpret_cast<ResetStats *>(arg);
                *resetStats = overrideResetStats.value();
                return overrideResetStatsReturnValue;
            }
        }

        return IoctlHelperPrelim20::ioctl(request, arg);
    }
    int ioctl(int fd, DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::perfDisable) {
            if (failPerfDisable) {
                return -1;
            }
        }
        if (request == DrmIoctl::perfEnable) {
            if (failPerfEnable) {
                return -1;
            }
        }
        return IoctlHelperPrelim20::ioctl(fd, request, arg);
    }
    bool checkWhetherGemCreateExtContainsMemPolicy(void *arg) {
        auto &gemCreateExt = *reinterpret_cast<prelim_drm_i915_gem_create_ext *>(arg);
        auto pExtensionBase = reinterpret_cast<i915_user_extension *>(gemCreateExt.extensions);
        while (pExtensionBase != nullptr) {
            if (pExtensionBase->name == PRELIM_I915_GEM_CREATE_EXT_MEMORY_POLICY) {
                auto lastPolicy = reinterpret_cast<prelim_drm_i915_gem_create_ext_memory_policy *>(pExtensionBase);
                lastPolicyMode = lastPolicy->mode;
                lastPolicyFlags = lastPolicy->flags;
                lastPolicyNodeMask.clear();
                auto nodeMaskPtr = reinterpret_cast<unsigned long *>(lastPolicy->nodemask_ptr);
                for (auto i = 0u; i < lastPolicy->nodemask_max; i++) {
                    lastPolicyNodeMask.push_back(nodeMaskPtr[i]);
                }
                return true;
            }
            pExtensionBase = reinterpret_cast<i915_user_extension *>(pExtensionBase->next_extension);
        }
        return false;
    }
    size_t ioctlCallCount = 0;
    bool lastGemCreateContainedMemPolicy = false;
    bool failPerfDisable = false;
    bool failPerfEnable = false;
    bool failPerfOpen = false;
    std::optional<int> overrideGemCreateExtReturnValue{};
    uint32_t lastPolicyMode = 0;
    uint32_t lastPolicyFlags = 0;
    std::vector<unsigned long> lastPolicyNodeMask{};
    std::optional<std::tuple<ResetStats, uint32_t, ResetStatsFault>> overrideResetStatsPrelim{};
    int overrideResetStatsPrelimReturnValue = 0;
    std::optional<ResetStats> overrideResetStats{};
    int overrideResetStatsReturnValue = 0;
    size_t resetStatsPrelimCalled = 0;
    size_t resetStatsCalled = 0;
};

TEST(IoctlPrelimHelperCreateGemExtTests, givenPrelimWhenCreateGemExtWithMemPolicyThenMemPolicyExtensionsIsAdded) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}};
    uint32_t numOfChunks = 0;
    std::vector<unsigned long> memPolicy;
    memPolicy.push_back(1);
    uint32_t memPolicyMode = 0;
    mockIoctlHelper.overrideGemCreateExtReturnValue = 0;
    mockIoctlHelper.initialize();
    auto ret = mockIoctlHelper.createGemExt(memClassInstance, 1024, handle, 0, {}, -1, false, numOfChunks, memPolicyMode, memPolicy, false);

    std::string output = capture.getCapturedStdout();
    std::string expectedSubstring("memory policy:");
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedMemPolicy);
    EXPECT_EQ(0u, mockIoctlHelper.lastPolicyFlags);
    EXPECT_EQ(memPolicyMode, mockIoctlHelper.lastPolicyMode);
    EXPECT_EQ(memPolicy, mockIoctlHelper.lastPolicyNodeMask);
    EXPECT_TRUE(output.find(expectedSubstring) != std::string::npos);
}

TEST(IoctlPrelimHelperCreateGemExtTests, givenPrelimWhenCreateGemExtWithMemPolicyAndChunkingThenMemPolicyExtensionsIsAdded) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}};
    uint32_t numOfChunks = 2;
    size_t size = 4 * MemoryConstants::pageSize64k;
    std::vector<unsigned long> memPolicy;
    memPolicy.push_back(1);
    uint32_t memPolicyMode = 0;
    mockIoctlHelper.overrideGemCreateExtReturnValue = 0;
    mockIoctlHelper.initialize();
    auto ret = mockIoctlHelper.createGemExt(memClassInstance, size, handle, 0, {}, -1, true, numOfChunks, memPolicyMode, memPolicy, false);

    EXPECT_EQ(0, ret);
    EXPECT_TRUE(mockIoctlHelper.lastGemCreateContainedMemPolicy);
    EXPECT_EQ(0u, mockIoctlHelper.lastPolicyFlags);
    EXPECT_EQ(memPolicyMode, mockIoctlHelper.lastPolicyMode);
    EXPECT_EQ(memPolicy, mockIoctlHelper.lastPolicyNodeMask);
}

TEST(IoctlPrelimHelperPerfTests, givenCalltoPerfDisableEuStallStreamWithValidStreamButCloseFailsThenFailureReturned) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, [](int fileDescriptor) -> int {
        return -1;
    });

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncRetVal = -1;
    int32_t invalidFd = 10;
    EXPECT_FALSE(mockIoctlHelper.perfDisableEuStallStream(&invalidFd));
    EXPECT_EQ(1u, NEO::SysCalls::closeFuncCalled);
    EXPECT_EQ(10, NEO::SysCalls::closeFuncArgPassed);
    NEO::SysCalls::closeFuncCalled = 0;
    NEO::SysCalls::closeFuncArgPassed = 0;
    NEO::SysCalls::closeFuncRetVal = 0;
}

TEST(IoctlPrelimHelperPerfTests, givenCalltoPerfDisableEuStallStreamWithInvalidStreamThenFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    int32_t invalidFd = -1;
    mockIoctlHelper.failPerfDisable = true;
    EXPECT_FALSE(mockIoctlHelper.perfDisableEuStallStream(&invalidFd));
}

TEST(IoctlPrelimHelperPerfTests, givenCalltoPerfOpenEuStallStreamWithInvalidStreamWithEnableSetToFailThenFailureReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    int32_t invalidFd = -1;
    mockIoctlHelper.failPerfEnable = true;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_FALSE(mockIoctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &invalidFd));
}

TEST(IoctlPrelimHelperPerfTests, givenCalltoPerfDisableEuStallStreamWithValidStreamThenSuccessIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    mockIoctlHelper.initialize();
    int32_t validFd = -1;
    uint32_t samplingPeridNs = 10000u;
    EXPECT_TRUE(mockIoctlHelper.perfOpenEuStallStream(0u, samplingPeridNs, 1, 20u, 10000u, &validFd));
    EXPECT_TRUE(mockIoctlHelper.perfDisableEuStallStream(&validFd));
}

class DrmMockIoctl : public DrmMock {
  public:
    DrmMockIoctl(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());
    }
    int handleRemainingRequests(DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::query) {

            Query *query = static_cast<Query *>(arg);
            QueryItem *queryItem = reinterpret_cast<QueryItem *>(query->itemsPtr);
            PrelimI915::prelim_drm_i915_query_fabric_info *info =
                reinterpret_cast<PrelimI915::prelim_drm_i915_query_fabric_info *>(queryItem->dataPtr);

            info->latency = mockLatency;
            info->bandwidth = mockBandwidth;
            return mockIoctlReturn;
        }
        return 0;
    }
    int mockIoctlReturn = 0;
    uint32_t mockLatency = 10;
    uint32_t mockBandwidth = 100;
};

TEST(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyThenSuccessIsReturned) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = std::numeric_limits<uint32_t>::max(), fabricId = 0, bandwidth = 0;
    EXPECT_TRUE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
    EXPECT_NE(latency, std::numeric_limits<uint32_t>::max());
    EXPECT_NE(bandwidth, 0u);
}

TEST(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyAndIoctlFailsThenErrorIsReturned) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = 0, fabricId = 0, bandwidth = 0;
    drm->mockIoctlReturn = 1;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

TEST(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyAndIoctlSetsZeroForLatencyThenErrorIsReturned) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = 0, fabricId = 0, bandwidth = 0;
    drm->mockIoctlReturn = 0;
    drm->mockLatency = 0;
    drm->mockBandwidth = 10;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}

TEST(IoctlPrelimHelperFabricLatencyTest, givenPrelimWhenGettingFabricLatencyAndIoctlSetsZeroForBandwidthThenErrorIsReturned) {

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMockIoctl> drm = std::make_unique<DrmMockIoctl>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t latency = 0, fabricId = 0, bandwidth = 0;
    drm->mockIoctlReturn = 0;
    drm->mockLatency = 10;
    drm->mockBandwidth = 0;
    EXPECT_FALSE(ioctlHelper.getFabricLatency(fabricId, latency, bandwidth));
}
HWTEST2_F(IoctlPrelimHelperTests, givenXeHpcWhenCallingIoctlWithGemExecbufferThenShouldNotBreakOnWouldBlock, IsXeHpcCore) {
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemExecbuffer2));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemVmBind));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::gemExecbuffer2));
}

HWTEST2_F(IoctlPrelimHelperTests, givenXeHpcWhenCallingIoctlWithGemExecbufferAndForceNonblockingExecbufferCallsThenShouldBreakOnWouldBlock, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceNonblockingExecbufferCalls.set(1);

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_FALSE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemExecbuffer2));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemVmBind));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::gemExecbuffer2));
}

HWTEST2_F(IoctlPrelimHelperTests, givenNonXeHpcWhenCallingIoctlWithGemExecbufferThenShouldBreakOnWouldBlock, IsNotXeHpcCore) {
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemExecbuffer2));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemVmBind));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::gemExecbuffer2));
}

HWTEST2_F(IoctlPrelimHelperTests, givenXeHpcWhenCreatingIoctlHelperThenProperFlagsAreSetToFileDescriptor, IsXeHpcCore) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    VariableBackup<decltype(SysCalls::getFileDescriptorFlagsCalled)> backupGetFlags(&SysCalls::getFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::setFileDescriptorFlagsCalled)> backupSetFlags(&SysCalls::setFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::passedFileDescriptorFlagsToSet)> backupPassedFlags(&SysCalls::passedFileDescriptorFlagsToSet, 0);

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(0, SysCalls::getFileDescriptorFlagsCalled);
    EXPECT_EQ(0, SysCalls::setFileDescriptorFlagsCalled);
    EXPECT_EQ(0, SysCalls::passedFileDescriptorFlagsToSet);
}

HWTEST2_F(IoctlPrelimHelperTests, givenXeHpcWhenCreatingIoctlHelperWithForceNonblockingExecbufferCallsThenProperFlagsAreSetToFileDescriptor, IsXeHpcCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceNonblockingExecbufferCalls.set(0);

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    VariableBackup<decltype(SysCalls::getFileDescriptorFlagsCalled)> backupGetFlags(&SysCalls::getFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::setFileDescriptorFlagsCalled)> backupSetFlags(&SysCalls::setFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::passedFileDescriptorFlagsToSet)> backupPassedFlags(&SysCalls::passedFileDescriptorFlagsToSet, 0);

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(0, SysCalls::getFileDescriptorFlagsCalled);
    EXPECT_EQ(0, SysCalls::setFileDescriptorFlagsCalled);
    EXPECT_EQ(0, SysCalls::passedFileDescriptorFlagsToSet);
}

HWTEST2_F(IoctlPrelimHelperTests, givenNonXeHpcWhenCreatingIoctlHelperThenProperFlagsAreSetToFileDescriptor, IsNotXeHpcCore) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    VariableBackup<decltype(SysCalls::getFileDescriptorFlagsCalled)> backupGetFlags(&SysCalls::getFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::setFileDescriptorFlagsCalled)> backupSetFlags(&SysCalls::setFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::passedFileDescriptorFlagsToSet)> backupPassedFlags(&SysCalls::passedFileDescriptorFlagsToSet, 0);

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(0, SysCalls::getFileDescriptorFlagsCalled);
    EXPECT_EQ(0, SysCalls::setFileDescriptorFlagsCalled);
}

TEST_F(IoctlPrelimHelperTests, givenDisabledForceNonblockingExecbufferCallsFlagWhenCreatingIoctlHelperThenExecBufferIsHandledBlocking) {
    DebugManagerStateRestore restorer;

    debugManager.flags.ForceNonblockingExecbufferCalls.set(0);
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    VariableBackup<decltype(SysCalls::getFileDescriptorFlagsCalled)> backupGetFlags(&SysCalls::getFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::setFileDescriptorFlagsCalled)> backupSetFlags(&SysCalls::setFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::passedFileDescriptorFlagsToSet)> backupPassedFlags(&SysCalls::passedFileDescriptorFlagsToSet, 0);

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(0, SysCalls::getFileDescriptorFlagsCalled);
    EXPECT_EQ(0, SysCalls::setFileDescriptorFlagsCalled);

    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemExecbuffer2));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemVmBind));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::gemExecbuffer2));
}

TEST_F(IoctlPrelimHelperTests, givenEnabledForceNonblockingExecbufferCallsFlagWhenCreatingIoctlHelperThenExecBufferIsHandledNonBlocking) {
    DebugManagerStateRestore restorer;

    debugManager.flags.ForceNonblockingExecbufferCalls.set(1);
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    VariableBackup<decltype(SysCalls::getFileDescriptorFlagsCalled)> backupGetFlags(&SysCalls::getFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::setFileDescriptorFlagsCalled)> backupSetFlags(&SysCalls::setFileDescriptorFlagsCalled, 0);
    VariableBackup<decltype(SysCalls::passedFileDescriptorFlagsToSet)> backupPassedFlags(&SysCalls::passedFileDescriptorFlagsToSet, 0);

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(1, SysCalls::getFileDescriptorFlagsCalled);
    EXPECT_EQ(1, SysCalls::setFileDescriptorFlagsCalled);
    EXPECT_EQ((O_RDWR | O_NONBLOCK), SysCalls::passedFileDescriptorFlagsToSet);

    EXPECT_FALSE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemExecbuffer2));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::gemVmBind));
    EXPECT_TRUE(ioctlHelper.checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::gemExecbuffer2));
}

TEST_F(IoctlPrelimHelperTests, whenChangingBufferBindingThenWaitIsNeededOnlyBeforeBind) {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_FALSE(ioctlHelper.isWaitBeforeBindRequired(false));
}

TEST_F(IoctlPrelimHelperTests, whenChangingBufferBindingAndForcingFenceWaitThenCallReturnsTrueForBindAndUnbind) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    debugManager.flags.EnableUserFenceUponUnbind.set(1);
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(false));
}

TEST_F(IoctlPrelimHelperTests, whenChangingBufferBindingAndNotForcingFenceWaitThenCallReturnsTrueForBindOnly) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    debugManager.flags.EnableUserFenceUponUnbind.set(0);
    EXPECT_TRUE(ioctlHelper.isWaitBeforeBindRequired(true));
    EXPECT_FALSE(ioctlHelper.isWaitBeforeBindRequired(false));
}

TEST_F(IoctlPrelimHelperTests, whenGettingPreferredLocationRegionThenReturnCorrectMemoryClassAndInstance) {
    DebugManagerStateRestore restorer;

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};

    IoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(std::nullopt, ioctlHelper.getPreferredLocationRegion(PreferredLocation::none, 0));

    auto region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::system, 0);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassSystem), region->memoryClass);
    EXPECT_EQ(0u, region->memoryInstance);

    region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::device, 1);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassDevice), region->memoryClass);
    EXPECT_EQ(1u, region->memoryInstance);

    region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::clear, 1);
    EXPECT_EQ(static_cast<uint16_t>(-1), region->memoryClass);
    EXPECT_EQ(0u, region->memoryInstance);

    debugManager.flags.SetVmAdvisePreferredLocation.set(3);
    region = ioctlHelper.getPreferredLocationRegion(PreferredLocation::none, 1);
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::memoryClassDevice), region->memoryClass);
    EXPECT_EQ(1u, region->memoryInstance);
}

TEST_F(IoctlPrelimHelperTests, WhenSetupIpVersionIsCalledThenIpVersionIsCorrect) {
    auto &hwInfo = *drm->getRootDeviceEnvironment().getHardwareInfo();
    auto &compilerProductHelper = drm->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    ioctlHelper.setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

TEST_F(IoctlPrelimHelperTests, whenGettingGpuTimeThenSucceeds) {
    MockExecutionEnvironment executionEnvironment{};
    auto drm = std::make_unique<DrmMockTime>(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperPrelim20 ioctlHelper{*drm};
    ASSERT_EQ(true, ioctlHelper.initialize());

    uint64_t time = 0;
    auto success = getGpuTime32(*drm.get(), &time);
    EXPECT_TRUE(success);
    EXPECT_NE(0ULL, time);
    success = getGpuTime36(*drm.get(), &time);
    EXPECT_TRUE(success);
    EXPECT_NE(0ULL, time);
    success = getGpuTimeSplitted(*drm.get(), &time);
    EXPECT_TRUE(success);
    EXPECT_NE(0ULL, time);
}

TEST_F(IoctlPrelimHelperTests, givenInvalidDrmWhenGettingGpuTimeThenFails) {
    MockExecutionEnvironment executionEnvironment{};
    auto drm = std::make_unique<DrmMockFail>(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    IoctlHelperPrelim20 ioctlHelper{*drm};
    ASSERT_EQ(true, ioctlHelper.initialize());

    uint64_t time = 0;
    auto success = getGpuTime32(*drm.get(), &time);
    EXPECT_FALSE(success);
    success = getGpuTime36(*drm.get(), &time);
    EXPECT_FALSE(success);
    success = getGpuTimeSplitted(*drm.get(), &time);
    EXPECT_FALSE(success);
}

TEST_F(IoctlPrelimHelperTests, whenGettingTimeThenTimeIsCorrect) {
    MockExecutionEnvironment executionEnvironment{};
    auto drm = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    MockIoctlHelperPrelim ioctlHelper{*drm};
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
        EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTimeSplitted);
        drm->ioctlResExt = &drm->none;
    }
}

TEST_F(IoctlPrelimHelperTests, givenInitializeGetGpuTimeFunctionNotCalledWhenSetGpuCpuTimesIsCalledThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockCustom::create(rootDeviceEnvironment);
    IoctlHelperPrelim20 ioctlHelper{*drm};

    drm->ioctlRes = -1;
    TimeStampData pGpuCpuTime{};
    std::unique_ptr<MockOSTimeLinux> osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    auto ret = ioctlHelper.setGpuCpuTimes(&pGpuCpuTime, osTime.get());
    EXPECT_EQ(false, ret);
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenGetFdFromVmExportIsCalledThenFalseIsReturned) {
    uint32_t vmId = 0, flags = 0;
    int32_t fd = 0;
    EXPECT_FALSE(ioctlHelper.getFdFromVmExport(vmId, flags, &fd));
}

TEST_F(IoctlPrelimHelperTests, whenGetResetStatsIsCalledThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    uint32_t status = 1;
    ResetStatsFault fault{};
    fault.flags = 1;

    ResetStats resetStats{};
    resetStats.contextId = 0;
    mockIoctlHelper.overrideResetStatsPrelim = {resetStats, status, fault};

    status = 0;
    fault.flags = 0;

    EXPECT_EQ(0, mockIoctlHelper.getResetStats(resetStats, nullptr, nullptr));
    EXPECT_EQ(1u, mockIoctlHelper.resetStatsPrelimCalled);
    EXPECT_EQ(0u, mockIoctlHelper.resetStatsCalled);

    EXPECT_EQ(0, mockIoctlHelper.getResetStats(resetStats, &status, nullptr));
    EXPECT_EQ(1u, status);
    EXPECT_EQ(2u, mockIoctlHelper.resetStatsPrelimCalled);
    EXPECT_EQ(0u, mockIoctlHelper.resetStatsCalled);

    status = 0;
    EXPECT_EQ(0, mockIoctlHelper.getResetStats(resetStats, &status, &fault));
    EXPECT_EQ(1u, status);
    EXPECT_EQ(1, fault.flags);
    EXPECT_EQ(3u, mockIoctlHelper.resetStatsPrelimCalled);
    EXPECT_EQ(0u, mockIoctlHelper.resetStatsCalled);
}

TEST_F(IoctlPrelimHelperTests, givenNonZeroReturnValuewhenGetResetStatsIsCalledThenReturnsValueFromRegularResetStatsIoctl) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 mockIoctlHelper{*drm};

    uint32_t status = 1;
    ResetStatsFault fault{};
    fault.flags = 1;

    ResetStats resetStats{};
    resetStats.contextId = 0;
    mockIoctlHelper.overrideResetStatsPrelim = {resetStats, status, fault};
    mockIoctlHelper.overrideResetStats = resetStats;

    status = 0;
    fault.flags = 0;
    mockIoctlHelper.overrideResetStatsPrelimReturnValue = -1;
    EXPECT_EQ(0, mockIoctlHelper.getResetStats(resetStats, &status, &fault));
    EXPECT_EQ(1u, mockIoctlHelper.resetStatsPrelimCalled);
    EXPECT_EQ(1u, mockIoctlHelper.resetStatsCalled);
}
TEST_F(IoctlPrelimHelperTests, whenCallingGetStatusAndFlagsForResetStatsThenExpectedValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 ioctlHelper{*drm};

    EXPECT_EQ(static_cast<uint32_t>(I915_RESET_STATS_BANNED), ioctlHelper.getStatusForResetStats(true));
    EXPECT_EQ(0u, ioctlHelper.getStatusForResetStats(false));

    EXPECT_TRUE(ioctlHelper.validPageFault(static_cast<uint16_t>(I915_RESET_STATS_FAULT_VALID)));
    EXPECT_FALSE(ioctlHelper.validPageFault(0u));
}

TEST_F(IoctlPrelimHelperTests, GivenIoctlHelperWhenCallingGetTileIdFromGtIdThenExpectedValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t gtId = 0;
    EXPECT_EQ(gtId, ioctlHelper.getTileIdFromGtId(gtId));
    gtId = 1;
    EXPECT_EQ(gtId, ioctlHelper.getTileIdFromGtId(gtId));
}

TEST_F(IoctlPrelimHelperTests, GivenIoctlHelperWhenCallingGetGtIdFromTileIdThenExpectedValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    MockIoctlHelperPrelim20 ioctlHelper{*drm};

    uint32_t tileId = 0u;
    EXPECT_EQ(tileId, ioctlHelper.getGtIdFromTileId(tileId, I915_ENGINE_CLASS_RENDER));
    tileId = 1u;
    EXPECT_EQ(tileId, ioctlHelper.getGtIdFromTileId(tileId, I915_ENGINE_CLASS_VIDEO));
}

TEST(DrmTest, GivenDrmWhenAskedForPreemptionThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = 0;
    pDrm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_TRUE(pDrm->isPreemptionSupported());

    pDrm->storedPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->storedRetVal = -1;
    pDrm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->storedPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    delete pDrm;
}

TEST(DrmTest, givenDrmPreemptionEnabledAndLowPriorityEngineWhenCreatingOsContextThenCallSetContextPriorityIoctl) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.preemptionSupported = false;

    OsContextLinux osContext1(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized(false);
    OsContextLinux osContext2(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    osContext2.ensureContextInitialized(false);

    EXPECT_EQ(4u, drmMock.receivedContextParamRequestCount);

    drmMock.preemptionSupported = true;

    OsContextLinux osContext3(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext3.ensureContextInitialized(false);
    EXPECT_EQ(6u, drmMock.receivedContextParamRequestCount);

    OsContextLinux osContext4(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    osContext4.ensureContextInitialized(false);
    EXPECT_EQ(9u, drmMock.receivedContextParamRequestCount);
    EXPECT_EQ(drmMock.storedDrmContextId, drmMock.receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_PRIORITY), drmMock.receivedContextParamRequest.param);
    EXPECT_EQ(static_cast<uint64_t>(-1023), drmMock.receivedContextParamRequest.value);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequest.size);
}
