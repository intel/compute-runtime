/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/kernel/debug_data.h"

#include "level_zero/core/source/debugger/debugger_l0.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {
extern DebugerL0CreateFn mockDebuggerL0HwFactory[];

template <typename GfxFamily>
class MockDebuggerL0Hw : public L0::DebuggerL0Hw<GfxFamily> {
  public:
    using L0::DebuggerL0::perContextSbaAllocations;
    using L0::DebuggerL0::sbaTrackingGpuVa;
    using L0::DebuggerL0::singleAddressSpaceSbaTracking;

    MockDebuggerL0Hw(NEO::Device *device) : L0::DebuggerL0Hw<GfxFamily>(device) {}
    ~MockDebuggerL0Hw() override = default;

    static DebuggerL0 *allocate(NEO::Device *device) {
        return new MockDebuggerL0Hw<GfxFamily>(device);
    }

    void captureStateBaseAddress(NEO::CommandContainer &container, NEO::Debugger::SbaAddresses sba) override {
        captureStateBaseAddressCount++;
        L0::DebuggerL0Hw<GfxFamily>::captureStateBaseAddress(container, sba);
    }

    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override {
        getSbaTrackingCommandsSizeCount++;
        return L0::DebuggerL0Hw<GfxFamily>::getSbaTrackingCommandsSize(trackedAddressCount);
    }

    void programSbaTrackingCommands(NEO::LinearStream &cmdStream, const NEO::Debugger::SbaAddresses &sba) override {
        programSbaTrackingCommandsCount++;
        L0::DebuggerL0Hw<GfxFamily>::programSbaTrackingCommands(cmdStream, sba);
    }

    void registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) override {
        registerElfCount++;
        lastReceivedElf = debugData->vIsa;
        L0::DebuggerL0Hw<GfxFamily>::registerElf(debugData, isaAllocation);
    }

    bool attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &allocs, uint32_t &moduleHandle) override {
        segmentCountWithAttachedModuleHandle = static_cast<uint32_t>(allocs.size());
        if (std::numeric_limits<uint32_t>::max() != moduleHandleToReturn) {
            moduleHandle = moduleHandleToReturn;
            return true;
        }
        return L0::DebuggerL0Hw<GfxFamily>::attachZebinModuleToSegmentAllocations(allocs, moduleHandle);
    }

    bool removeZebinModule(uint32_t moduleHandle) override {
        removedZebinModuleHandle = moduleHandle;
        return L0::DebuggerL0Hw<GfxFamily>::removeZebinModule(moduleHandle);
    }

    void notifyCommandQueueCreated() override {
        commandQueueCreatedCount++;
        L0::DebuggerL0Hw<GfxFamily>::notifyCommandQueueCreated();
    }

    void notifyCommandQueueDestroyed() override {
        commandQueueDestroyedCount++;
        L0::DebuggerL0Hw<GfxFamily>::notifyCommandQueueDestroyed();
    }

    uint32_t captureStateBaseAddressCount = 0;
    uint32_t programSbaTrackingCommandsCount = 0;
    uint32_t getSbaTrackingCommandsSizeCount = 0;
    uint32_t registerElfCount = 0;
    uint32_t commandQueueCreatedCount = 0;
    uint32_t commandQueueDestroyedCount = 0;
    const char *lastReceivedElf = nullptr;

    uint32_t segmentCountWithAttachedModuleHandle = 0;
    uint32_t removedZebinModuleHandle = 0;
    uint32_t moduleHandleToReturn = std::numeric_limits<uint32_t>::max();
};

template <uint32_t productFamily, typename GfxFamily>
struct MockDebuggerL0HwPopulateFactory {
    MockDebuggerL0HwPopulateFactory() {
        mockDebuggerL0HwFactory[productFamily] = MockDebuggerL0Hw<GfxFamily>::allocate;
    }
};

template <>
struct WhiteBox<::L0::DebuggerL0> : public ::L0::DebuggerL0 {
    using BaseClass = ::L0::DebuggerL0;
    using BaseClass::initDebuggingInOs;
};

} // namespace ult
} // namespace L0
