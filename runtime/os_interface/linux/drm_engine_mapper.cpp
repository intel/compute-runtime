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

unsigned int DrmEngineMapper::engineNodeMap(EngineType engineType) {
    if (EngineType::ENGINE_RCS == engineType) {
        return I915_EXEC_RENDER;
    }
    UNRECOVERABLE_IF(true);
}
} // namespace OCLRT
