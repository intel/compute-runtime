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
    deviceCountBased,
    chunkSizeBased,
    mappingBased
};

class TagTypeBase {
};

enum class TagNodeType {
    timestampPacket,
    hwTimeStamps,
    hwPerfCounter
};

enum class CacheRegion : uint16_t {
    defaultRegion = 0,
    region1,
    region2,
    count,
    none = 0xFFFF
};

enum class CacheLevel : uint16_t {
    defaultLevel = 0,
    level3 = 3
};

enum class CachePolicy : uint32_t {
    uncached = 0,
    writeCombined = 1,
    writeThrough = 2,
    writeBack = 3,
};

enum class PreferredLocation : int16_t {
    clear = -1,
    system = 0,
    device = 1,
    none = 2,
    defaultLocation = device
};

enum class PostSyncMode : uint32_t {
    noWrite = 0,
    timestamp = 1,
    immediateData = 2,
};

enum class AtomicAccessMode : uint32_t {
    none = 1,
    host = 2,
    device = 3,
    system = 4
};

} // namespace NEO
