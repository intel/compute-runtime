/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

namespace NEO {

class DrmEngineMapper {
  public:
    static unsigned int engineNodeMap(aub_stream::EngineType engineType);
};

} // namespace NEO
