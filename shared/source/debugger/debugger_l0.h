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

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace NEO {
class Device;
class GraphicsAllocation;
class LinearStream;
class OSInterface;

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

    void printTrackedAddresses(uint32_t contextId);
    MOCKABLE_VIRTUAL void registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation);
    MOCKABLE_VIRTUAL void notifyCommandQueueCreated(NEO::Device *device);
    MOCKABLE_VIRTUAL void notifyCommandQueueDestroyed(NEO::Device *device);
    MOCKABLE_VIRTUAL void notifyModuleLoadAllocations(Device *device, const StackVec<NEO::GraphicsAllocation *, 32> &allocs);
    MOCKABLE_VIRTUAL void notifyModuleCreate(void *module, uint32_t moduleSize, uint64_t moduleLoadAddress);
    MOCKABLE_VIRTUAL void notifyModuleDestroy(uint64_t moduleLoadAddress);
    MOCKABLE_VIRTUAL void registerAllocationType(GraphicsAllocation *allocation);
    void initSbaTrackingMode();

    virtual size_t getSbaAddressLoadCommandsSize() = 0;
    virtual void programSbaAddressLoad(NEO::LinearStream &cmdStream, uint64_t sbaGpuVa) = 0;

    MOCKABLE_VIRTUAL bool attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &kernelAlloc, uint32_t &moduleHandle);
    MOCKABLE_VIRTUAL bool removeZebinModule(uint32_t moduleHandle);

    void setSingleAddressSpaceSbaTracking(bool value) {
        singleAddressSpaceSbaTracking = value;
    }
    bool getSingleAddressSpaceSbaTracking() { return singleAddressSpaceSbaTracking; }

    struct CommandQueueNotification {
        uint32_t subDeviceIndex = 0;
        uint32_t subDeviceCount = 0;
    };

  protected:
    static bool initDebuggingInOs(NEO::OSInterface *osInterface);

    void initialize();

    NEO::Device *device = nullptr;
    NEO::GraphicsAllocation *sbaAllocation = nullptr;
    std::unordered_map<uint32_t, NEO::GraphicsAllocation *> perContextSbaAllocations;
    NEO::AddressRange sbaTrackingGpuVa{};
    NEO::GraphicsAllocation *moduleDebugArea = nullptr;
    std::vector<uint32_t> commandQueueCount;
    std::vector<uint32_t> uuidL0CommandQueueHandle;
    bool singleAddressSpaceSbaTracking = false;
    std::mutex debuggerL0Mutex;
};

static_assert(std::is_standard_layout<DebuggerL0::CommandQueueNotification>::value, "DebuggerL0::CommandQueueNotification issue");

using DebugerL0CreateFn = DebuggerL0 *(*)(NEO::Device *device);
extern DebugerL0CreateFn debuggerL0Factory[];

template <typename GfxFamily>
class DebuggerL0Hw : public DebuggerL0 {
  public:
    static DebuggerL0 *allocate(NEO::Device *device);

    void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba) override;
    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override;
    size_t getSbaAddressLoadCommandsSize() override;
    void programSbaAddressLoad(NEO::LinearStream &cmdStream, uint64_t sbaGpuVa) override;

    void programSbaTrackingCommandsSingleAddressSpace(NEO::LinearStream &cmdStream, const SbaAddresses &sba);

  protected:
    DebuggerL0Hw(NEO::Device *device) : DebuggerL0(device){};
};

template <uint32_t coreFamily, typename GfxFamily>
struct DebuggerL0PopulateFactory {
    DebuggerL0PopulateFactory() {
        debuggerL0Factory[coreFamily] = DebuggerL0Hw<GfxFamily>::allocate;
    }
};

} // namespace NEO
