/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/cache_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <array>

using namespace NEO;

class DrmMockExtended : public DrmMock {
  public:
    using Drm::cacheInfo;
    using Drm::engineInfo;
    using Drm::ioctlHelper;
    using Drm::memoryInfo;
    using Drm::pageFaultSupported;
    using Drm::rootDeviceEnvironment;
    using DrmMock::DrmMock;
    using BaseClass = DriverModel;

    DrmMockExtended(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockExtended(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmMockExtended(RootDeviceEnvironment &rootDeviceEnvironmentIn, const HardwareInfo *inputHwInfo);

    int handleRemainingRequests(DrmIoctl request, void *arg) override;

    ADDMETHOD_CONST(getDriverModelType, DriverModelType, true, DriverModelType::unknown, (), ())

    static constexpr uint32_t maxEngineCount = 9u;
    ContextEnginesLoadBalance<maxEngineCount> receivedContextEnginesLoadBalance{};
    ContextParamEngines<1 + maxEngineCount> receivedContextParamEngines{};

    int storedRetValForSetParamEngines = 0;

    uint16_t closIndex = 0;
    uint16_t maxNumWays = 32;
    uint32_t allocNumWays = 0;

    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();

    BcsInfoMask supportedCopyEnginesMask = 1;
    std::array<uint64_t, 9> copyEnginesCapsMap = {{PRELIM_I915_COPY_CLASS_CAP_SATURATE_LMEM,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_PCIE,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK,
                                                   PRELIM_I915_COPY_CLASS_CAP_SATURATE_LINK}};
    bool failQueryDistanceInfo = false;
    bool vmAdviseCalled = false;
    uint32_t vmAdviseFlags = 0u;
    uint32_t vmAdviseHandle = 0u;
    MemoryClassInstance vmAdviseRegion = {};
    int vmAdviseRetValue = 0;

    // PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT
    prelim_drm_i915_gem_create_ext createExt{};
    prelim_drm_i915_gem_create_ext_setparam setparamRegion{};
    MemoryClassInstance memRegions{};
    std::vector<MemoryClassInstance> allMemRegions;
    int gemCreateExtRetVal = 0;

    // DRM_IOCTL_I915_GEM_MMAP_OFFSET
    int mmapOffsetRetVal = 0;

    // Debugger ioctls
    prelim_drm_i915_debugger_open_param inputDebuggerOpen = {};
    int debuggerOpenRetval = 10; // debugFd

  protected:
    virtual bool handleQueryItem(QueryItem *queryItem);
};
