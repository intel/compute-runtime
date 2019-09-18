/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_engine_mapper.h"

#include "hw_cmds.h"

namespace NEO {

GPUNODE_ORDINAL WddmEngineMapper::engineNodeMap(aub_stream::EngineType engineType) {
    if (aub_stream::ENGINE_RCS == engineType) {
        return GPUNODE_3D;
    } else if (aub_stream::ENGINE_BCS == engineType) {
        return GPUNODE_BLT;
    } else if (aub_stream::ENGINE_CCS == engineType) {
        return GPUNODE_CCS0;
    }
    UNRECOVERABLE_IF(true);
}

} // namespace NEO
