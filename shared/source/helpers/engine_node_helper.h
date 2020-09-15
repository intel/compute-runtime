/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

#include <atomic>
#include <utility>

namespace NEO {
struct HardwareInfo;

enum class EngineUsage : uint32_t {
    Regular,
    LowPriority,
    Internal
};

using EngineTypeUsage = std::pair<aub_stream::EngineType, EngineUsage>;

namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType);
bool isBcs(aub_stream::EngineType engineType);
aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo, std::atomic<uint32_t> &selectorCopyEngine);
}; // namespace EngineHelpers
} // namespace NEO
