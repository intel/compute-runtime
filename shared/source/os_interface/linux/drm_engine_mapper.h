/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

namespace NEO {

enum class DrmParam;
class DrmEngineMapper {
  public:
    static DrmParam engineNodeMap(aub_stream::EngineType engineType);
};

} // namespace NEO
