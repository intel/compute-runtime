/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"
#include "level_zero/sysman/source/api/engine/sysman_engine_imp.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

using namespace NEO;

namespace L0 {
namespace Sysman {
namespace ult {

class MockEngineSysmanHwDeviceIdDrm : public MockSysmanHwDeviceIdDrm {
  public:
    using L0::Sysman::ult::MockSysmanHwDeviceIdDrm::MockSysmanHwDeviceIdDrm;
    int returnOpenFileDescriptor = -1;
    int returnCloseFileDescriptor = 0;

    int openFileDescriptor() override {
        return returnOpenFileDescriptor;
    }

    int closeFileDescriptor() override {
        return returnCloseFileDescriptor;
    }
};

struct MockEngineNeoDrm : public Drm {
    using Drm::getEngineInfo;
    using Drm::setupIoctlHelper;
    const int mockFd = 0;
    MockEngineNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
    MockEngineNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment, int mockFileDescriptor) : Drm(std::make_unique<MockEngineSysmanHwDeviceIdDrm>(mockFileDescriptor, ""), rootDeviceEnvironment) {}

    bool mockSysmanQueryEngineInfoReturnFalse = true;
    bool sysmanQueryEngineInfo() override {
        if (mockSysmanQueryEngineInfoReturnFalse != true) {
            return mockSysmanQueryEngineInfoReturnFalse;
        }

        std::vector<NEO::EngineCapabilities> i915engineInfo(6);
        i915engineInfo[0].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
        i915engineInfo[0].engine.engineInstance = 0;
        i915engineInfo[1].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER;
        i915engineInfo[1].engine.engineInstance = 1;
        i915engineInfo[2].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO;
        i915engineInfo[2].engine.engineInstance = 1;
        i915engineInfo[3].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY;
        i915engineInfo[3].engine.engineInstance = 0;
        i915engineInfo[4].engine.engineClass = drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE;
        i915engineInfo[4].engine.engineInstance = 0;
        i915engineInfo[5].engine.engineClass = UINT16_MAX;
        i915engineInfo[5].engine.engineInstance = 0;

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{i915engineInfo};

        this->engineInfo.reset(new EngineInfo(this, engineInfosPerTile));
        return true;
    }
};

struct MockEnginePmuInterfaceImp : public L0::Sysman::PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::pSysmanKmdInterface;
    int64_t mockPmuFd = 0;
    uint64_t mockTimestamp = 0;
    uint64_t mockActiveTime = 0;
    MockEnginePmuInterfaceImp(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    int64_t mockPerfEventFailureReturnValue = 0;
    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {
        if (mockPerfEventFailureReturnValue == -1) {
            return mockPerfEventFailureReturnValue;
        }

        return mockPmuFd;
    }

    int mockPmuReadFailureReturnValue = 0;
    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {
        if (mockPmuReadFailureReturnValue == -1) {
            return mockPmuReadFailureReturnValue;
        }

        data[0] = mockActiveTime;
        data[1] = mockTimestamp;
        return 0;
    }
};

using DrmMockEngineInfoFailing = DrmMock;

} // namespace ult
} // namespace Sysman
} // namespace L0
