/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_engine_mapper.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"

namespace NEO {

GPUNODE_ORDINAL WddmEngineMapper::engineNodeMap(aub_stream::EngineType engineType) {
    if (EngineHelpers::isCcs(engineType)) {
        return GPUNODE_CCS0;
    } else if (aub_stream::ENGINE_BCS == engineType || EngineHelpers::isLinkBcs(engineType)) {
        return GPUNODE_BLT;
    }
    UNRECOVERABLE_IF(engineType != aub_stream::ENGINE_RCS && engineType != aub_stream::ENGINE_CCCS);
    return GPUNODE_3D;
}

} // namespace NEO
