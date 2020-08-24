/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/engine_info_impl.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "sysman/engine/engine_imp.h"
#include "sysman/linux/os_sysman_imp.h"

using namespace NEO;
namespace L0 {
namespace ult {

class EngineNeoDrm : public Drm {
  public:
    using Drm::engineInfo;
    const int mockFd = 0;
    EngineNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceId>(mockFd, ""), rootDeviceEnvironment) {}
};
template <>
struct Mock<EngineNeoDrm> : public EngineNeoDrm {
    Mock<EngineNeoDrm>(RootDeviceEnvironment &rootDeviceEnvironment) : EngineNeoDrm(rootDeviceEnvironment) {}

    bool queryEngineInfoMockPositiveTest() {
        drm_i915_engine_info i915engineInfo[4] = {};
        i915engineInfo[0].engine.engine_class = I915_ENGINE_CLASS_RENDER;
        i915engineInfo[0].engine.engine_instance = 0;
        i915engineInfo[1].engine.engine_class = I915_ENGINE_CLASS_VIDEO;
        i915engineInfo[1].engine.engine_instance = 0;
        i915engineInfo[2].engine.engine_class = I915_ENGINE_CLASS_VIDEO;
        i915engineInfo[2].engine.engine_instance = 1;
        i915engineInfo[3].engine.engine_class = I915_ENGINE_CLASS_COPY;
        i915engineInfo[3].engine.engine_instance = 0;

        this->engineInfo.reset(new EngineInfoImpl(i915engineInfo, 4));
        return true;
    }

    bool queryEngineInfoMockReturnFalse() {
        return false;
    }

    MOCK_METHOD(bool, queryEngineInfo, (), (override));
};

} // namespace ult
} // namespace L0
