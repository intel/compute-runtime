/*
 * Copyright (C) 2021 Intel Corporation
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

aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo, std::atomic<uint32_t> &selectorCopyEngine) {
    return aub_stream::EngineType::ENGINE_BCS;
}

std::string engineTypeToStringAdditional(aub_stream::EngineType engineType) {
    return "Unknown";
}

} // namespace EngineHelpers
} // namespace NEO
