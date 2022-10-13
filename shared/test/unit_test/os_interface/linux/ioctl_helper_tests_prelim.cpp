/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

extern std::vector<uint8_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions);
extern std::vector<uint8_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines);

struct IoctlPrelimHelperTests : ::testing::Test {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<Drm> drm{Drm::create(std::make_unique<HwDeviceIdDrm>(0, ""), *executionEnvironment.rootDeviceEnvironments[0])};
    IoctlHelperPrelim20 ioctlHelper{*drm};
};

TEST_F(IoctlPrelimHelperTests, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
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
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmBind), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_BIND));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmUnbind), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemWaitUserFence), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemCreateExt), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmAdvise), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmPrefetch), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::UuidRegister), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_UUID_REGISTER));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::UuidUnregister), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::DebuggerOpen), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemClosReserve), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemClosFree), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemCacheReserve), static_cast<unsigned int>(PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemMmapOffset), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_MMAP_OFFSET));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_DESTROY));

    EXPECT_THROW(ioctlHelper.getIoctlRequestValue(DrmIoctl::DG1GemCreateExt), std::runtime_error);
}

TEST_F(IoctlPrelimHelperTests, whenGettingDrmParamStringThenProperStringIsReturned) {
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamChipsetId).c_str(), "I915_PARAM_CHIPSET_ID");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamRevision).c_str(), "I915_PARAM_REVISION");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasExecSoftpin).c_str(), "I915_PARAM_HAS_EXEC_SOFTPIN");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasPooledEu).c_str(), "I915_PARAM_HAS_POOLED_EU");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasScheduler).c_str(), "I915_PARAM_HAS_SCHEDULER");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamEuTotal).c_str(), "I915_PARAM_EU_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamSubsliceTotal).c_str(), "I915_PARAM_SUBSLICE_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamMinEuInPool).c_str(), "I915_PARAM_MIN_EU_IN_POOL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamCsTimestampFrequency).c_str(), "I915_PARAM_CS_TIMESTAMP_FREQUENCY");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasVmBind).c_str(), "PRELIM_I915_PARAM_HAS_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasPageFault).c_str(), "PRELIM_I915_PARAM_HAS_PAGE_FAULT");
}

TEST_F(IoctlPrelimHelperTests, whenGettingIoctlRequestStringThenProperStringIsReturned) {
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::Getparam).c_str(), "DRM_IOCTL_I915_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemExecbuffer2).c_str(), "DRM_IOCTL_I915_GEM_EXECBUFFER2");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemWait).c_str(), "DRM_IOCTL_I915_GEM_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemClose).c_str(), "DRM_IOCTL_GEM_CLOSE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemUserptr).c_str(), "DRM_IOCTL_I915_GEM_USERPTR");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemCreate).c_str(), "DRM_IOCTL_I915_GEM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemSetDomain).c_str(), "DRM_IOCTL_I915_GEM_SET_DOMAIN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemSetTiling).c_str(), "DRM_IOCTL_I915_GEM_SET_TILING");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemGetTiling).c_str(), "DRM_IOCTL_I915_GEM_GET_TILING");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextCreateExt).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextDestroy).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::RegRead).c_str(), "DRM_IOCTL_I915_REG_READ");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GetResetStats).c_str(), "DRM_IOCTL_I915_GET_RESET_STATS");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextGetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextSetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::Query).c_str(), "DRM_IOCTL_I915_QUERY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::PrimeFdToHandle).c_str(), "DRM_IOCTL_PRIME_FD_TO_HANDLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::PrimeHandleToFd).c_str(), "DRM_IOCTL_PRIME_HANDLE_TO_FD");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmBind).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmUnbind).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemWaitUserFence).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemCreateExt).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmAdvise).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmPrefetch).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::UuidRegister).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_REGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::UuidUnregister).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::DebuggerOpen).c_str(), "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemClosReserve).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemClosFree).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemCacheReserve).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemMmapOffset).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmCreate).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmDestroy).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");

    EXPECT_THROW(ioctlHelper.getIoctlString(DrmIoctl::DG1GemCreateExt), std::runtime_error);
}

TEST_F(IoctlPrelimHelperTests, whenGettingDrmParamValueThenPropertValueIsReturned) {
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextCreateExtSetparam), static_cast<int>(I915_CONTEXT_CREATE_EXT_SETPARAM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextCreateFlagsUseExtensions), static_cast<int>(I915_CONTEXT_CREATE_FLAGS_USE_EXTENSIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextEnginesExtLoadBalance), static_cast<int>(I915_CONTEXT_ENGINES_EXT_LOAD_BALANCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamEngines), static_cast<int>(I915_CONTEXT_PARAM_ENGINES));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamGttSize), static_cast<int>(I915_CONTEXT_PARAM_GTT_SIZE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamPersistence), static_cast<int>(I915_CONTEXT_PARAM_PERSISTENCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamPriority), static_cast<int>(I915_CONTEXT_PARAM_PRIORITY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamRecoverable), static_cast<int>(I915_CONTEXT_PARAM_RECOVERABLE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamSseu), static_cast<int>(I915_CONTEXT_PARAM_SSEU));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ContextParamVm), static_cast<int>(I915_CONTEXT_PARAM_VM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassCompute), static_cast<int>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassRender), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassCopy), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassVideo), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassVideoEnhance), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassInvalid), static_cast<int>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_INVALID));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::EngineClassInvalidNone), static_cast<int>(I915_ENGINE_CLASS_INVALID_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecBlt), static_cast<int>(I915_EXEC_BLT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecDefault), static_cast<int>(I915_EXEC_DEFAULT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecNoReloc), static_cast<int>(I915_EXEC_NO_RELOC));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ExecRender), static_cast<int>(I915_EXEC_RENDER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::MemoryClassDevice), static_cast<int>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::MemoryClassSystem), static_cast<int>(drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::MmapOffsetWb), static_cast<int>(I915_MMAP_OFFSET_WB));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::MmapOffsetWc), static_cast<int>(I915_MMAP_OFFSET_WC));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamChipsetId), static_cast<int>(I915_PARAM_CHIPSET_ID));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamRevision), static_cast<int>(I915_PARAM_REVISION));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasExecSoftpin), static_cast<int>(I915_PARAM_HAS_EXEC_SOFTPIN));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasPooledEu), static_cast<int>(I915_PARAM_HAS_POOLED_EU));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasScheduler), static_cast<int>(I915_PARAM_HAS_SCHEDULER));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamEuTotal), static_cast<int>(I915_PARAM_EU_TOTAL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamSubsliceTotal), static_cast<int>(I915_PARAM_SUBSLICE_TOTAL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamMinEuInPool), static_cast<int>(I915_PARAM_MIN_EU_IN_POOL));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamCsTimestampFrequency), static_cast<int>(I915_PARAM_CS_TIMESTAMP_FREQUENCY));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasVmBind), static_cast<int>(PRELIM_I915_PARAM_HAS_VM_BIND));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::ParamHasPageFault), static_cast<int>(PRELIM_I915_PARAM_HAS_PAGE_FAULT));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryEngineInfo), static_cast<int>(DRM_I915_QUERY_ENGINE_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryHwconfigTable), static_cast<int>(PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryMemoryRegions), static_cast<int>(DRM_I915_QUERY_MEMORY_REGIONS));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryComputeSlices), static_cast<int>(PRELIM_DRM_I915_QUERY_COMPUTE_SLICES));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::QueryTopologyInfo), static_cast<int>(DRM_I915_QUERY_TOPOLOGY_INFO));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::SchedulerCapPreemption), static_cast<int>(I915_SCHEDULER_CAP_PREEMPTION));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::TilingNone), static_cast<int>(I915_TILING_NONE));
    EXPECT_EQ(ioctlHelper.getDrmParamValue(DrmParam::TilingY), static_cast<int>(I915_TILING_Y));
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToMemoryRegionsThenReturnSameData) {
    std::vector<MemoryRegion> expectedMemRegions(2);
    expectedMemRegions[0].region.memoryClass = prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_SYSTEM;
    expectedMemRegions[0].region.memoryInstance = 0;
    expectedMemRegions[0].probedSize = 1024;
    expectedMemRegions[1].region.memoryClass = prelim_drm_i915_gem_memory_class::PRELIM_I915_MEMORY_CLASS_DEVICE;
    expectedMemRegions[1].region.memoryInstance = 0;
    expectedMemRegions[1].probedSize = 1024;

    auto regionInfo = getRegionInfo(expectedMemRegions);

    auto memRegions = ioctlHelper.translateToMemoryRegions(regionInfo);
    EXPECT_EQ(2u, memRegions.size());
    for (uint32_t i = 0; i < memRegions.size(); i++) {
        EXPECT_EQ(expectedMemRegions[i].region.memoryClass, memRegions[i].region.memoryClass);
        EXPECT_EQ(expectedMemRegions[i].region.memoryInstance, memRegions[i].region.memoryInstance);
        EXPECT_EQ(expectedMemRegions[i].probedSize, memRegions[i].probedSize);
        EXPECT_EQ(expectedMemRegions[i].unallocatedSize, memRegions[i].unallocatedSize);
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
                auto flags = ioctlHelper.getFlagsForVmBind(bindCapture, bindImmediate, bindMakeResident);
                if (bindCapture) {
                    EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_CAPTURE, (flags & PRELIM_I915_GEM_VM_BIND_CAPTURE));
                }
                if (bindImmediate) {
                    EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_IMMEDIATE, (flags & PRELIM_I915_GEM_VM_BIND_IMMEDIATE));
                }
                if (bindMakeResident) {
                    EXPECT_EQ(PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT, (flags & PRELIM_I915_GEM_VM_BIND_MAKE_RESIDENT));
                }
                if (flags == 0) {
                    EXPECT_FALSE(bindCapture);
                    EXPECT_FALSE(bindImmediate);
                    EXPECT_FALSE(bindMakeResident);
                }
            }
        }
    }
}

TEST_F(IoctlPrelimHelperTests, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned) {
    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = ioctlHelper.prepareVmBindExt(bindExtHandles);
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
    expectedEngines[0] = {{static_cast<uint16_t>(ioctlHelper.getDrmParamValue(DrmParam::EngineClassRender)), 0}, 0};
    expectedEngines[1] = {{static_cast<uint16_t>(ioctlHelper.getDrmParamValue(DrmParam::EngineClassCopy)), 1}, 0};

    auto engineInfo = getEngineInfo(expectedEngines);

    auto engines = ioctlHelper.translateToEngineCaps(engineInfo);
    EXPECT_EQ(2u, engines.size());
    for (uint32_t i = 0; i < engines.size(); i++) {
        EXPECT_EQ(expectedEngines[i].engine.engineClass, engines[i].engine.engineClass);
        EXPECT_EQ(expectedEngines[i].engine.engineInstance, engines[i].engine.engineInstance);
        EXPECT_EQ(expectedEngines[i].capabilities, engines[i].capabilities);
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

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenGettingEuStallPropertiesThenCorrectPropertiesAreReturned) {
    std::array<uint64_t, 12u> properties = {};
    EXPECT_TRUE(ioctlHelper.getEuStallProperties(properties, 0x101, 0x102, 0x103, 1, 20u));
    EXPECT_EQ(properties[0], prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_BUF_SZ);
    EXPECT_EQ(properties[1], 0x101u);
    EXPECT_EQ(properties[2], prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_SAMPLE_RATE);
    EXPECT_EQ(properties[3], 0x102u);
    EXPECT_EQ(properties[4], prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_POLL_PERIOD);
    EXPECT_EQ(properties[5], 0x103u);
    EXPECT_EQ(properties[6], prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_CLASS);
    EXPECT_EQ(properties[7], prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE);
    EXPECT_EQ(properties[8], prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_ENGINE_INSTANCE);
    EXPECT_EQ(properties[9], 1u);
    EXPECT_EQ(properties[10], prelim_drm_i915_eu_stall_property_id::PRELIM_DRM_I915_EU_STALL_PROP_EVENT_REPORT_COUNT);
    EXPECT_EQ(properties[11], 20u);
}

TEST_F(IoctlPrelimHelperTests, givenPrelimWhenGettingEuStallFdParameterThenCorrectIoctlValueIsReturned) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_I915_PERF_FLAG_FD_EU_STALL), ioctlHelper.getEuStallFdParameter());
}
