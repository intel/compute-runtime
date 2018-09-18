/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/engine_node.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

#include <cstdint>

namespace OCLRT {

template <typename Family>
class WddmEngineMapper {
  public:
    static bool engineNodeMap(EngineType engineType, GPUNODE_ORDINAL &gpuNode);
};

} // namespace OCLRT
