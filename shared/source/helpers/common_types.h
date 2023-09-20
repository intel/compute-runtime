/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace NEO {
class GraphicsAllocation;
struct EngineControl;
using EngineControlContainer = std::vector<EngineControl>;
using MultiDeviceEngineControlContainer = StackVec<EngineControlContainer, 6u>;
class Device;
using DeviceVector = std::vector<std::unique_ptr<Device>>;
using PrivateAllocsToReuseContainer = StackVec<std::pair<uint32_t, GraphicsAllocation *>, 8>;

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

enum class PreferredLocation : int16_t {
    Clear = -1,
    System = 0,
    Device = 1,
    None = 2,
    Default = Device
};

enum class PostSyncMode : uint32_t {
    NoWrite = 0,
    Timestamp = 1,
    ImmediateData = 2,
};

enum class AtomicAccessMode : uint32_t {
    None = 1,
    Host = 2,
    Device = 3,
    System = 4
};

} // namespace NEO
