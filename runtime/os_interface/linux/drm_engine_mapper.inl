/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/linux/drm_engine_mapper.h"
#include "drm/i915_drm.h"

namespace OCLRT {

template <typename Family>
bool DrmEngineMapper<Family>::engineNodeMap(EngineType engineType, unsigned int &flag) {
    bool ret = false;
    switch (engineType) {
    case EngineType::ENGINE_RCS:
        flag = I915_EXEC_RENDER;
        ret = true;
        break;
    default:
        break;
    }
    return ret;
}
} // namespace OCLRT
