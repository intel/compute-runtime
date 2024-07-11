/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockNeoDrm : public NEO::Drm {

  public:
    using NEO::Drm::getEngineInfo;
    using NEO::Drm::ioctlHelper;
    const int mockFd = 0;

    MockNeoDrm(NEO::RootDeviceEnvironment &rootDeviceEnvironment) : NEO::Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    bool mockSysmanQueryEngineInfoReturnFalse = true;
    bool sysmanQueryEngineInfo() override {
        if (mockSysmanQueryEngineInfoReturnFalse != true) {
            return mockSysmanQueryEngineInfoReturnFalse;
        }

        std::vector<NEO::EngineCapabilities> mockEngineInfo(6);
        mockEngineInfo[0].engine.engineClass = EngineClass::ENGINE_CLASS_RENDER;
        mockEngineInfo[0].engine.engineInstance = 0;
        mockEngineInfo[1].engine.engineClass = EngineClass::ENGINE_CLASS_RENDER;
        mockEngineInfo[1].engine.engineInstance = 1;
        mockEngineInfo[2].engine.engineClass = EngineClass::ENGINE_CLASS_VIDEO;
        mockEngineInfo[2].engine.engineInstance = 1;
        mockEngineInfo[3].engine.engineClass = EngineClass::ENGINE_CLASS_COPY;
        mockEngineInfo[3].engine.engineInstance = 0;
        mockEngineInfo[4].engine.engineClass = EngineClass::ENGINE_CLASS_VIDEO_ENHANCE;
        mockEngineInfo[4].engine.engineInstance = 0;
        mockEngineInfo[5].engine.engineClass = UINT16_MAX;
        mockEngineInfo[5].engine.engineInstance = 0;

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{mockEngineInfo};
        this->engineInfo.reset(new NEO::EngineInfo(this, engineInfosPerTile));
        return true;
    }
};

class MockPmuInterfaceImp : public L0::Sysman::PmuInterfaceImp {
  public:
    int64_t mockPmuFd = -1;
    uint64_t mockTimestamp = 0;
    uint64_t mockActiveTime = 0;
    using PmuInterfaceImp::perfEventOpen;
    using PmuInterfaceImp::pSysmanKmdInterface;
    MockPmuInterfaceImp(L0::Sysman::LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

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

} // namespace ult
} // namespace Sysman
} // namespace L0
