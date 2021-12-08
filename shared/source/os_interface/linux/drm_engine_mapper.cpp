/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"

#include "drm/i915_drm.h"

namespace NEO {

unsigned int DrmEngineMapper::engineNodeMap(aub_stream::EngineType engineType) {
    if (EngineHelpers::isCcs(engineType)) {
        return I915_EXEC_DEFAULT;
    } else if (aub_stream::ENGINE_BCS == engineType || EngineHelpers::isLinkBcs(engineType)) {
        return I915_EXEC_BLT;
    }
    UNRECOVERABLE_IF(engineType != aub_stream::ENGINE_RCS && engineType != aub_stream::ENGINE_CCCS);
    return I915_EXEC_RENDER;
}
} // namespace NEO
