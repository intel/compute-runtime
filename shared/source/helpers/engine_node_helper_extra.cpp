/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"

namespace NEO {
namespace EngineHelpers {
bool isBcs(aub_stream::EngineType engineType) {
    return engineType == aub_stream::ENGINE_BCS;
}

aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, SelectorCopyEngine &selectorCopyEngine, bool internalUsage) {
    return aub_stream::EngineType::ENGINE_BCS;
}

void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine) {}

std::string engineTypeToStringAdditional(aub_stream::EngineType engineType) {
    return "Unknown";
}

aub_stream::EngineType remapEngineTypeToHwSpecific(aub_stream::EngineType inputType, const HardwareInfo &hwInfo) {
    return inputType;
}

uint32_t getBcsIndex(aub_stream::EngineType engineType) {
    return 0;
}

} // namespace EngineHelpers
} // namespace NEO
