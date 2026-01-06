/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/device_bitfield.h"

#include "aubstream/engine_node.h"

#include <atomic>
#include <bitset>

namespace NEO {
enum PreemptionMode : uint32_t;
struct HardwareInfo;
struct SelectorCopyEngine;
struct RootDeviceEnvironment;

enum class EngineUsage : uint32_t {
    regular,
    lowPriority,
    highPriority,
    internal,
    cooperative,

    engineUsageCount,
};

using EngineTypeUsage = std::pair<aub_stream::EngineType, EngineUsage>;

struct EngineDescriptor {
    EngineDescriptor() = delete;
    constexpr EngineDescriptor(EngineTypeUsage engineTypeUsage, DeviceBitfield deviceBitfield, PreemptionMode preemptionMode, bool isRootDevice)
        : engineTypeUsage(engineTypeUsage), deviceBitfield(deviceBitfield), preemptionMode(preemptionMode), isRootDevice(isRootDevice) {}

    EngineTypeUsage engineTypeUsage;
    DeviceBitfield deviceBitfield;
    PreemptionMode preemptionMode;
    bool isRootDevice;
};

namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType);
bool isComputeEngine(aub_stream::EngineType engineType);
bool isBcs(aub_stream::EngineType engineType);
bool isBcsVirtualEngineEnabled(aub_stream::EngineType engineType);
aub_stream::EngineType getBcsEngineType(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, SelectorCopyEngine &selectorCopyEngine, bool internalUsage);
void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine, const RootDeviceEnvironment &rootDeviceEnvironment);
aub_stream::EngineType remapEngineTypeToHwSpecific(aub_stream::EngineType inputType, const RootDeviceEnvironment &rootDeviceEnvironment);
uint32_t getCcsIndex(aub_stream::EngineType engineType);
uint32_t getBcsIndex(aub_stream::EngineType engineType);
aub_stream::EngineType getBcsEngineAtIdx(uint32_t idx);
aub_stream::EngineType mapBcsIndexToEngineType(uint32_t index, bool includeMainCopyEngine);
aub_stream::EngineType mapCcsIndexToEngineType(uint32_t index);
std::string engineTypeToString(aub_stream::EngineType engineType);
std::string engineUsageToString(EngineUsage usage);
EngineGroupType engineTypeToEngineGroupType(aub_stream::EngineType engineType);

bool isBcsEnabled(const HardwareInfo &hwInfo, aub_stream::EngineType engineType);

constexpr bool isLinkBcs(aub_stream::EngineType engineType) {
    return engineType >= aub_stream::ENGINE_BCS1 && engineType <= aub_stream::ENGINE_BCS8;
}

inline constexpr uint32_t numLinkedCopyEngines = 8u;
inline constexpr size_t oddLinkedCopyEnginesMask = 0b010101010;
inline constexpr size_t h2dCopyEngineMask = 0b000001010;
inline constexpr size_t d2hCopyEngineMask = 0b010100000;

bool linkCopyEnginesSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield);

aub_stream::EngineType selectLinkCopyEngine(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, std::atomic<uint32_t> &selectorCopyEngine);

}; // namespace EngineHelpers
} // namespace NEO
