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

struct MockIoctlHelperXe : public NEO::IoctlHelperXe {

    MockIoctlHelperXe(NEO::Drm &drmArg) : NEO::IoctlHelperXe(drmArg) {}

    int getTileIdFromGtId(int gtId) const override {
        return 0;
    }
};

struct MockXeEngineInfo : public NEO::EngineInfo {

    using NEO::EngineInfo::tileToEngineMap;
    uint32_t count = 2;

    MockXeEngineInfo(NEO::Drm *drm, StackVec<std::vector<NEO::EngineCapabilities>, 2> &engineInfosPerTile) : NEO::EngineInfo(drm, engineInfosPerTile) {

        std::vector<NEO::EngineCapabilities> mockEngineCapabilities(4);
        mockEngineCapabilities[0].engine.engineClass = EngineClass::ENGINE_CLASS_RENDER;
        mockEngineCapabilities[0].engine.engineInstance = 0;
        mockEngineCapabilities[1].engine.engineClass = EngineClass::ENGINE_CLASS_COPY;
        mockEngineCapabilities[1].engine.engineInstance = 0;
        mockEngineCapabilities[2].engine.engineClass = EngineClass::ENGINE_CLASS_COMPUTE;
        mockEngineCapabilities[2].engine.engineInstance = 0;
        mockEngineCapabilities[3].engine.engineClass = EngineClass::ENGINE_CLASS_VIDEO_ENHANCE;
        mockEngineCapabilities[3].engine.engineInstance = 0;

        tileToEngineMap.emplace(0, mockEngineCapabilities[0].engine);
        tileToEngineMap.emplace(0, mockEngineCapabilities[1].engine);
        tileToEngineMap.emplace(0, mockEngineCapabilities[2].engine);
        tileToEngineMap.emplace(1, mockEngineCapabilities[3].engine);
    }
};

struct MockNeoDrm : public NEO::Drm {

  public:
    using NEO::Drm::getEngineInfo;
    using NEO::Drm::ioctlHelper;
    const int mockFd = 0;

    MockNeoDrm(NEO::RootDeviceEnvironment &rootDeviceEnvironment) : NEO::Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    bool sysmanQueryEngineInfo() override {

        StackVec<std::vector<NEO::EngineCapabilities>, 2> engineInfosPerTile{};
        this->engineInfo.reset(new MockXeEngineInfo(this, engineInfosPerTile));
        return true;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
