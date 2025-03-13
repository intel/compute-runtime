/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockNeoDrm : public NEO::Drm {

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

        std::vector<NEO::EngineCapabilities> mockEngineInfo(3);
        mockEngineInfo[0].engine.engineClass = EngineClass::ENGINE_CLASS_RENDER;
        mockEngineInfo[0].engine.engineInstance = 0;
        mockEngineInfo[1].engine.engineClass = EngineClass::ENGINE_CLASS_COPY;
        mockEngineInfo[1].engine.engineInstance = 0;
        mockEngineInfo[2].engine.engineClass = EngineClass::ENGINE_CLASS_VIDEO_ENHANCE;
        mockEngineInfo[2].engine.engineInstance = 0;
        mockEngineInfo[3].engine.engineClass = UINT16_MAX;
        mockEngineInfo[3].engine.engineInstance = 0;

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{mockEngineInfo};
        this->engineInfo.reset(new NEO::EngineInfo(this, engineInfosPerTile));
        return true;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
