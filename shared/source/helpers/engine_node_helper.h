/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/utilities/stackvec.h"

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
    Cooperative,

    EngineUsageCount,
};

using EngineTypeUsage = std::pair<aub_stream::EngineType, EngineUsage>;
using EngineInstancesContainer = StackVec<EngineTypeUsage, 32>;

struct EngineDescriptor {
    EngineDescriptor() = delete;
    constexpr EngineDescriptor(EngineTypeUsage engineTypeUsage, DeviceBitfield deviceBitfield, PreemptionMode preemptionMode, bool isRootDevice, bool isEngineInstanced)
        : engineTypeUsage(engineTypeUsage), deviceBitfield(deviceBitfield), preemptionMode(preemptionMode), isRootDevice(isRootDevice), isEngineInstanced(isEngineInstanced) {}

    EngineTypeUsage engineTypeUsage;
    DeviceBitfield deviceBitfield;
    PreemptionMode preemptionMode;
    bool isRootDevice;
    bool isEngineInstanced;
};

namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType);
bool isBcs(aub_stream::EngineType engineType);
bool isBcsVirtualEngineEnabled();
aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, SelectorCopyEngine &selectorCopyEngine, bool internalUsage);
void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine);
aub_stream::EngineType remapEngineTypeToHwSpecific(aub_stream::EngineType inputType, const HardwareInfo &hwInfo);
uint32_t getBcsIndex(aub_stream::EngineType engineType);
aub_stream::EngineType mapBcsIndexToEngineType(uint32_t index, bool includeMainCopyEngine);
aub_stream::EngineType mapCcsIndexToEngineType(uint32_t index);

std::string engineTypeToString(aub_stream::EngineType engineType);
std::string engineTypeToStringAdditional(aub_stream::EngineType engineType);
std::string engineUsageToString(EngineUsage usage);

}; // namespace EngineHelpers
} // namespace NEO
