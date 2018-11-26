/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"

namespace OCLRT {

GPUNODE_ORDINAL WddmEngineMapper::engineNodeMap(EngineType engineType) {
    if (EngineType::ENGINE_RCS == engineType) {
        return GPUNODE_3D;
    }
    UNRECOVERABLE_IF(true);
}

} // namespace OCLRT
