/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/os_interface/windows/wddm_engine_mapper.h"

namespace OCLRT {

template <typename Family>
bool WddmEngineMapper<Family>::engineNodeMap(EngineType engineType, GPUNODE_ORDINAL &gpuNode) {
    bool ret = false;
    switch (engineType) {
    case EngineType::ENGINE_RCS:
        gpuNode = GPUNODE_3D;
        ret = true;
        break;
    default:
        break;
    }
    return ret;
}

} // namespace OCLRT
