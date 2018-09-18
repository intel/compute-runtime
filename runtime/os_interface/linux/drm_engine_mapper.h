/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/engine_node.h"

#include <cstdint>

namespace OCLRT {

template <typename Family>
class DrmEngineMapper {
  public:
    static bool engineNodeMap(EngineType engineType, unsigned int &flag);
};

} // namespace OCLRT
