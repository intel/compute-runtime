/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
namespace NEO {
struct HardwareInfo;
class LinearStream;
class IndirectHeap;
struct DebugData;
class GraphicsAllocation;

class Debugger {
  public:
    struct SbaAddresses {
        constexpr static size_t trackedAddressCount = 6;
        uint64_t GeneralStateBaseAddress = 0;
        uint64_t SurfaceStateBaseAddress = 0;
        uint64_t DynamicStateBaseAddress = 0;
        uint64_t IndirectObjectBaseAddress = 0;
        uint64_t InstructionBaseAddress = 0;
        uint64_t BindlessSurfaceStateBaseAddress = 0;
        uint64_t BindlessSamplerStateBaseAddress = 0;
    };

    static std::unique_ptr<Debugger> create(HardwareInfo *hwInfo);
    virtual ~Debugger() = default;
    bool isLegacy() const { return isLegacyMode; }
    virtual void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba) = 0;
    virtual size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) = 0;

    void *getDebugSurfaceReservedSurfaceState(IndirectHeap &ssh);

    inline static bool isDebugEnabled(bool internalUsage) {
        return !internalUsage;
    }

  protected:
    bool isLegacyMode = true;
};

static_assert(std::is_standard_layout<Debugger::SbaAddresses>::value);
} // namespace NEO