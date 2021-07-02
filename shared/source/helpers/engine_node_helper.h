/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

#include <atomic>
#include <string>
#include <utility>

namespace NEO {
struct HardwareInfo;
struct SelectorCopyEngine;

enum class EngineUsage : uint32_t {
    Regular,
    LowPriority,
    Internal,

    EngineUsageCount,
};

using EngineTypeUsage = std::pair<aub_stream::EngineType, EngineUsage>;

namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType);
bool isBcs(aub_stream::EngineType engineType);
aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo, SelectorCopyEngine &selectorCopyEngine, bool internalUsage = false);
void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine);

std::string engineTypeToString(aub_stream::EngineType engineType);
std::string engineTypeToStringAdditional(aub_stream::EngineType engineType);
std::string engineUsageToString(EngineUsage usage);

}; // namespace EngineHelpers
} // namespace NEO
