/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <bitset>
#include <cstdint>
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

enum class CacheRegion : uint16_t {
    Default = 0,
    Region1,
    Region2,
    Count,
    None = 0xFFFF
};

enum class CacheLevel : uint16_t {
    Default = 0,
    Level3 = 3
};

enum class CachePolicy : uint32_t {
    Uncached = 0,
    WriteCombined = 1,
    WriteThrough = 2,
    WriteBack = 3,
};

enum class PostSyncMode : uint32_t {
    NoWrite = 0,
    Timestamp = 1,
    ImmediateData = 2,
};

} // namespace NEO
