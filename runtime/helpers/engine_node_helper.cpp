/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "engine_node.h"

namespace NEO {

bool isCcs(aub_stream::EngineType engineType) {
    return engineType == aub_stream::ENGINE_CCS;
}

} // namespace NEO
