/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/kernel/debug_data.h"

extern NEO::DebugerL0CreateFn mockDebuggerL0HwFactory[];

namespace NEO {
template <class BaseClass>
struct WhiteBox;

class MockDebuggerL0 : public NEO::DebuggerL0 {
  public:
    MockDebuggerL0(NEO::Device *device) : DebuggerL0(device) {
        isLegacyMode = false;
    }

    void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba, bool useFirstLevelBB) override{};
    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override {
        return 0;
    }

    size_t getSbaAddressLoadCommandsSize() override { return 0; };
    void programSbaAddressLoad(NEO::LinearStream &cmdStream, uint64_t sbaGpuVa) override{};
};

template <typename GfxFamily>
class MockDebuggerL0Hw : public NEO::DebuggerL0Hw<GfxFamily> {
  public:
    using NEO::DebuggerL0::perContextSbaAllocations;
    using NEO::DebuggerL0::sbaTrackingGpuVa;
    using NEO::DebuggerL0::singleAddressSpaceSbaTracking;
    using NEO::DebuggerL0::uuidL0CommandQueueHandle;

    MockDebuggerL0Hw(NEO::Device *device) : NEO::DebuggerL0Hw<GfxFamily>(device) {}
    ~MockDebuggerL0Hw() override = default;

    static NEO::DebuggerL0 *allocate(NEO::Device *device) {
        return new MockDebuggerL0Hw<GfxFamily>(device);
    }

    void captureStateBaseAddress(NEO::LinearStream &cmdStream, NEO::Debugger::SbaAddresses sba, bool useFirstLevelBB) override {
        captureStateBaseAddressCount++;
        NEO::DebuggerL0Hw<GfxFamily>::captureStateBaseAddress(cmdStream, sba, useFirstLevelBB);
    }

    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override {
        getSbaTrackingCommandsSizeCount++;
        return NEO::DebuggerL0Hw<GfxFamily>::getSbaTrackingCommandsSize(trackedAddressCount);
    }

    void registerElfAndLinkWithAllocation(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) override {
        registerElfAndLinkCount++;
        lastReceivedElf = debugData->vIsa;
        NEO::DebuggerL0Hw<GfxFamily>::registerElfAndLinkWithAllocation(debugData, isaAllocation);
    }

    uint32_t registerElf(NEO::DebugData *debugData) override {
        if (elfHandleToReturn != std::numeric_limits<uint32_t>::max()) {
            return elfHandleToReturn;
        }
        registerElfCount++;
        lastReceivedElf = debugData->vIsa;
        return NEO::DebuggerL0Hw<GfxFamily>::registerElf(debugData);
    }

    bool attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &allocs, uint32_t &moduleHandle, uint32_t elfHandle) override {
        segmentCountWithAttachedModuleHandle = static_cast<uint32_t>(allocs.size());
        if (std::numeric_limits<uint32_t>::max() != moduleHandleToReturn) {
            moduleHandle = moduleHandleToReturn;
            return true;
        }
        return NEO::DebuggerL0Hw<GfxFamily>::attachZebinModuleToSegmentAllocations(allocs, moduleHandle, elfHandle);
    }

    bool removeZebinModule(uint32_t moduleHandle) override {
        removedZebinModuleHandle = moduleHandle;
        return NEO::DebuggerL0Hw<GfxFamily>::removeZebinModule(moduleHandle);
    }

    void notifyCommandQueueCreated(NEO::Device *device) override {
        commandQueueCreatedCount++;
        NEO::DebuggerL0Hw<GfxFamily>::notifyCommandQueueCreated(device);
    }

    void notifyCommandQueueDestroyed(NEO::Device *device) override {
        commandQueueDestroyedCount++;
        NEO::DebuggerL0Hw<GfxFamily>::notifyCommandQueueDestroyed(device);
    }

    void registerAllocationType(NEO::GraphicsAllocation *allocation) override {
        registerAllocationTypeCount++;
        NEO::DebuggerL0Hw<GfxFamily>::registerAllocationType(allocation);
    }

    void notifyModuleCreate(void *module, uint32_t moduleSize, uint64_t moduleLoadAddress) override {
        notifyModuleCreateCount++;
        NEO::DebuggerL0Hw<GfxFamily>::notifyModuleCreate(module, moduleSize, moduleLoadAddress);
    }

    void notifyModuleDestroy(uint64_t moduleLoadAddress) override {
        notifyModuleDestroyCount++;
        NEO::DebuggerL0Hw<GfxFamily>::notifyModuleDestroy(moduleLoadAddress);
    }

    void notifyModuleLoadAllocations(NEO::Device *device, const StackVec<NEO::GraphicsAllocation *, 32> &allocs) override {
        notifyModuleLoadAllocationsCapturedDevice = device;
        NEO::DebuggerL0Hw<GfxFamily>::notifyModuleLoadAllocations(device, allocs);
    }

    uint32_t captureStateBaseAddressCount = 0;
    uint32_t getSbaTrackingCommandsSizeCount = 0;
    uint32_t registerElfCount = 0;
    uint32_t registerElfAndLinkCount = 0;
    uint32_t commandQueueCreatedCount = 0;
    uint32_t commandQueueDestroyedCount = 0;
    uint32_t registerAllocationTypeCount = 0;
    uint32_t notifyModuleCreateCount = 0;
    uint32_t notifyModuleDestroyCount = 0;
    const char *lastReceivedElf = nullptr;

    uint32_t segmentCountWithAttachedModuleHandle = 0;
    uint32_t removedZebinModuleHandle = 0;
    uint32_t moduleHandleToReturn = std::numeric_limits<uint32_t>::max();
    uint32_t elfHandleToReturn = std::numeric_limits<uint32_t>::max();
    NEO::Device *notifyModuleLoadAllocationsCapturedDevice = nullptr;
};

template <>
struct WhiteBox<NEO::DebuggerL0> : public NEO::DebuggerL0 {
    using BaseClass = NEO::DebuggerL0;
    using BaseClass::initDebuggingInOs;
};

template <uint32_t productFamily, typename GfxFamily>
struct MockDebuggerL0HwPopulateFactory {
    MockDebuggerL0HwPopulateFactory() {
        mockDebuggerL0HwFactory[productFamily] = MockDebuggerL0Hw<GfxFamily>::allocate;
    }
};
} // namespace NEO
