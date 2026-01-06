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

    bool sysmanQueryEngineInfo() override {

        std::vector<NEO::EngineCapabilities> queryEngineInfo(4);
        queryEngineInfo[0].engine.engineClass = EngineClass::ENGINE_CLASS_RENDER;
        queryEngineInfo[0].engine.engineInstance = 0;
        queryEngineInfo[1].engine.engineClass = EngineClass::ENGINE_CLASS_COPY;
        queryEngineInfo[1].engine.engineInstance = 0;
        queryEngineInfo[2].engine.engineClass = EngineClass::ENGINE_CLASS_COMPUTE;
        queryEngineInfo[2].engine.engineInstance = 0;
        queryEngineInfo[3].engine.engineClass = EngineClass::ENGINE_CLASS_VIDEO_ENHANCE;
        queryEngineInfo[3].engine.engineInstance = 0;

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{queryEngineInfo};

        this->engineInfo.reset(new NEO::EngineInfo(this, engineInfosPerTile));
        return true;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
