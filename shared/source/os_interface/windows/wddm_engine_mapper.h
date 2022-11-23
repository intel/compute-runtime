/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_lib.h"

#include "aubstream/engine_node.h"

namespace NEO {

class WddmEngineMapper {
  public:
    static GPUNODE_ORDINAL engineNodeMap(aub_stream::EngineType engineType);
};

} // namespace NEO
