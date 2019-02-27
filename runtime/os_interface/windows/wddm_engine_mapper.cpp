/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_engine_mapper.h"

#include "hw_cmds.h"

namespace OCLRT {

GPUNODE_ORDINAL WddmEngineMapper::engineNodeMap(EngineType engineType) {
    if (EngineType::ENGINE_RCS == engineType) {
        return GPUNODE_3D;
    }
    UNRECOVERABLE_IF(true);
}

} // namespace OCLRT
