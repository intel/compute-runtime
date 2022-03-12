/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <level_zero/ze_api.h>

#include <memory>
#include <unordered_map>

namespace NEO {
class Device;
class GraphicsAllocation;
class LinearStream;
class OSInterface;
} // namespace NEO

namespace L0 {
#pragma pack(1)
struct SbaTrackedAddresses {
    char magic[8] = "sbaarea";
    uint64_t Reserved1 = 0;
    uint8_t Version = 0;
    uint8_t Reserved2[7];
    uint64_t GeneralStateBaseAddress = 0;
    uint64_t SurfaceStateBaseAddress = 0;
    uint64_t DynamicStateBaseAddress = 0;
    uint64_t IndirectObjectBaseAddress = 0;
    uint64_t InstructionBaseAddress = 0;
    uint64_t BindlessSurfaceStateBaseAddress = 0;
    uint64_t BindlessSamplerStateBaseAddress = 0;
};

struct DebugAreaHeader {
    char magic[8] = "dbgarea";
    uint64_t reserved1 = 0;
    uint8_t version = 0;
    uint8_t pgsize = 0;
    uint8_t size = 0;
    uint8_t reserved2 = 0;
    uint16_t scratchBegin = 0;
    uint16_t scratchEnd = 0;
    union {
        uint64_t isSharedBitfield = 0;
        struct {
            uint64_t isShared : 1;
            uint64_t reserved3 : 63;
        };
    };
};
static_assert(sizeof(DebugAreaHeader) == 32u * sizeof(uint8_t));

struct alignas(4) DebuggerVersion {
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
};
struct alignas(8) StateSaveAreaHeader {
    char magic[8] = "tssarea";
    uint64_t reserved1;
    struct DebuggerVersion version;
    uint8_t size;
    uint8_t reserved2[3];
};

#pragma pack()

class DebuggerL0 : public NEO::Debugger, NEO::NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<Debugger> create(NEO::Device *device);

    DebuggerL0(NEO::Device *device);
    ~DebuggerL0() override;

    NEO::GraphicsAllocation *getSbaTrackingBuffer(uint32_t contextId) {
        return perContextSbaAllocations[contextId];
    }

    NEO::GraphicsAllocation *getModuleDebugArea() {
        return moduleDebugArea;
    }

    uint64_t getSbaTrackingGpuVa() {
        return sbaTrackingGpuVa.address;
    }

    void captureStateBaseAddress(NEO::CommandContainer &container, SbaAddresses sba) override;
    void printTrackedAddresses(uint32_t contextId);
    MOCKABLE_VIRTUAL void registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation);
    MOCKABLE_VIRTUAL void notifyCommandQueueCreated();
    MOCKABLE_VIRTUAL void notifyCommandQueueDestroyed();

    virtual size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) = 0;
    virtual void programSbaTrackingCommands(NEO::LinearStream &cmdStream, const SbaAddresses &sba) = 0;

    MOCKABLE_VIRTUAL bool attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &kernelAlloc, uint32_t &moduleHandle);
    MOCKABLE_VIRTUAL bool removeZebinModule(uint32_t moduleHandle);

  protected:
    static bool isAnyTrackedAddressChanged(SbaAddresses sba) {
        return sba.GeneralStateBaseAddress != 0 ||
               sba.SurfaceStateBaseAddress != 0 ||
               sba.BindlessSurfaceStateBaseAddress != 0;
    }
    static bool initDebuggingInOs(NEO::OSInterface *osInterface);

    void initialize();

    NEO::Device *device = nullptr;
    NEO::GraphicsAllocation *sbaAllocation = nullptr;
    std::unordered_map<uint32_t, NEO::GraphicsAllocation *> perContextSbaAllocations;
    NEO::AddressRange sbaTrackingGpuVa;
    NEO::GraphicsAllocation *moduleDebugArea = nullptr;
    std::atomic<uint32_t> commandQueueCount = 0u;
    uint32_t uuidL0CommandQueueHandle = 0;
};

using DebugerL0CreateFn = DebuggerL0 *(*)(NEO::Device *device);
extern DebugerL0CreateFn debuggerL0Factory[];

template <typename GfxFamily>
class DebuggerL0Hw : public DebuggerL0 {
  public:
    static DebuggerL0 *allocate(NEO::Device *device);

    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override;
    void programSbaTrackingCommands(NEO::LinearStream &cmdStream, const SbaAddresses &sba) override;

  protected:
    DebuggerL0Hw(NEO::Device *device) : DebuggerL0(device){};
};

template <uint32_t coreFamily, typename GfxFamily>
struct DebuggerL0PopulateFactory {
    DebuggerL0PopulateFactory() {
        debuggerL0Factory[coreFamily] = DebuggerL0Hw<GfxFamily>::allocate;
    }
};

} // namespace L0
