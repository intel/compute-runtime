/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
namespace NEO {
struct HardwareInfo;
class LinearStream;
class IndirectHeap;
struct DebugData;
class GraphicsAllocation;
struct RootDeviceEnvironment;

class Debugger {
  public:
    struct SbaAddresses {
        constexpr static size_t trackedAddressCount = 6;
        uint64_t generalStateBaseAddress = 0;
        uint64_t surfaceStateBaseAddress = 0;
        uint64_t dynamicStateBaseAddress = 0;
        uint64_t indirectObjectBaseAddress = 0;
        uint64_t instructionBaseAddress = 0;
        uint64_t bindlessSurfaceStateBaseAddress = 0;
        uint64_t bindlessSamplerStateBaseAddress = 0;
    };

    virtual ~Debugger() = default;
    virtual void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba, bool useFirstLevelBB) = 0;
    virtual size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) = 0;
    virtual bool getSingleAddressSpaceSbaTracking() const = 0;

    void *getDebugSurfaceReservedSurfaceState(IndirectHeap &ssh);

    inline static bool isDebugEnabled(bool internalUsage) {
        return !internalUsage;
    }
};

enum class DebuggingMode : uint32_t {
    disabled,
    online,
    offline
};

inline DebuggingMode getDebuggingMode(uint32_t programDebugging) {
    switch (programDebugging) {
    case 1: {
        return DebuggingMode::online;
    }
    case 2: {
        return DebuggingMode::offline;
    }
    case 0:
    default: {
        return DebuggingMode::disabled;
    }
    }
}

static_assert(std::is_standard_layout<Debugger::SbaAddresses>::value);
} // namespace NEO
