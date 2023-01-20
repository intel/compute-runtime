/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/utilities/stackvec.h"

#include "aubstream/engine_node.h"

#include <atomic>
#include <bitset>

namespace NEO {
enum PreemptionMode : uint32_t;
struct HardwareInfo;
struct SelectorCopyEngine;
struct RootDeviceEnvironment;

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
bool isBcsVirtualEngineEnabled(aub_stream::EngineType engineType);
aub_stream::EngineType getBcsEngineType(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, SelectorCopyEngine &selectorCopyEngine, bool internalUsage);
void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine);
aub_stream::EngineType remapEngineTypeToHwSpecific(aub_stream::EngineType inputType, const RootDeviceEnvironment &rootDeviceEnvironment);
uint32_t getBcsIndex(aub_stream::EngineType engineType);
aub_stream::EngineType mapBcsIndexToEngineType(uint32_t index, bool includeMainCopyEngine);
aub_stream::EngineType mapCcsIndexToEngineType(uint32_t index);
std::string engineTypeToString(aub_stream::EngineType engineType);
std::string engineUsageToString(EngineUsage usage);

bool isBcsEnabled(const HardwareInfo &hwInfo, aub_stream::EngineType engineType);

constexpr bool isLinkBcs(aub_stream::EngineType engineType) {
    return engineType >= aub_stream::ENGINE_BCS1 && engineType <= aub_stream::ENGINE_BCS8;
}

inline constexpr uint32_t numLinkedCopyEngines = 8u;
inline constexpr size_t oddLinkedCopyEnginesMask = 0b010101010;

bool linkCopyEnginesSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield);

aub_stream::EngineType selectLinkCopyEngine(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, std::atomic<uint32_t> &selectorCopyEngine);

}; // namespace EngineHelpers
} // namespace NEO
