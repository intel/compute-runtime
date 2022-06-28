/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"

namespace NEO {

DrmParam DrmEngineMapper::engineNodeMap(aub_stream::EngineType engineType) {
    if (EngineHelpers::isCcs(engineType)) {
        return DrmParam::ExecDefault;
    } else if (aub_stream::ENGINE_BCS == engineType || EngineHelpers::isLinkBcs(engineType)) {
        return DrmParam::ExecBlt;
    }
    UNRECOVERABLE_IF(engineType != aub_stream::ENGINE_RCS && engineType != aub_stream::ENGINE_CCCS);
    return DrmParam::ExecRender;
}
} // namespace NEO
