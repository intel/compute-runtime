/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <bitset>
#include <memory>
#include <vector>
namespace NEO {
struct EngineControl;
using EngineControlContainer = std::vector<EngineControl>;
using DeviceBitfield = std::bitset<32>;
class Device;
using DeviceVector = std::vector<std::unique_ptr<Device>>;

enum class DebugPauseState : uint32_t {
    disabled,
    waitingForFirstSemaphore,
    waitingForUserStartConfirmation,
    hasUserStartConfirmation,
    waitingForUserEndConfirmation,
    hasUserEndConfirmation,
    terminate
};

enum class ColouringPolicy : uint32_t {
    DeviceCountBased,
    ChunkSizeBased,
    MappingBased
};

class TagTypeBase {
};

enum class TagNodeType {
    TimestampPacket,
    HwTimeStamps,
    HwPerfCounter
};
} // namespace NEO
