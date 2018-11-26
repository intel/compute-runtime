/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

#include <cstdint>

namespace OCLRT {

class DrmEngineMapper {
  public:
    static unsigned int engineNodeMap(EngineType engineType);
};

} // namespace OCLRT
