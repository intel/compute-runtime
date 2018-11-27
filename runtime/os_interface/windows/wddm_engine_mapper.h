/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

namespace OCLRT {

class WddmEngineMapper {
  public:
    static GPUNODE_ORDINAL engineNodeMap(EngineType engineType);
};

} // namespace OCLRT
