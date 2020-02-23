/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"

namespace NEO {
namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType) {
    return engineType == aub_stream::ENGINE_CCS;
}

bool isBcs(aub_stream::EngineType engineType) {
    return engineType == aub_stream::ENGINE_BCS;
}

aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo) {
    return aub_stream::EngineType::ENGINE_BCS;
}
} // namespace EngineHelpers
} // namespace NEO
