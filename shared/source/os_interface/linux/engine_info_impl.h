/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "drm/i915_drm.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {

struct EngineInfoImpl : public EngineInfo {
    ~EngineInfoImpl() override = default;

    EngineInfoImpl(const std::vector<EngineCapabilities> &engineInfos) : engines(engineInfos) {
    }

    std::vector<EngineCapabilities> engines;
};

} // namespace NEO
