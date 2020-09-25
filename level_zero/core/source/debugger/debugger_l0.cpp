/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/debugger/debugger_l0.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include <cstring>

namespace L0 {

DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE] = {};

DebuggerL0::DebuggerL0(NEO::Device *device) : device(device) {
    isLegacyMode = false;

    auto &engines = device->getEngines();

    sbaTrackingGpuVa = device->getMemoryManager()->reserveGpuAddress(MemoryConstants::pageSize, device->getRootDeviceIndex());

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false,
                                         CommonConstants::allDevicesBitfield};

    properties.gpuAddress = sbaTrackingGpuVa.address;
    SbaTrackedAddresses sbaHeader;

    for (auto &engine : engines) {
        properties.osContext = engine.osContext;
        auto sbaAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        memset(sbaAllocation->getUnderlyingBuffer(), 0, sbaAllocation->getUnderlyingBufferSize());

        auto sbaHeaderPtr = reinterpret_cast<SbaTrackedAddresses *>(sbaAllocation->getUnderlyingBuffer());
        *sbaHeaderPtr = sbaHeader;

        perContextSbaAllocations[engine.osContext->getContextId()] = sbaAllocation;
    }
}

void DebuggerL0::printTrackedAddresses(uint32_t contextId) {
    auto memory = perContextSbaAllocations[contextId]->getUnderlyingBuffer();
    auto sba = reinterpret_cast<SbaTrackedAddresses *>(memory);

    PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stdout,
                       "Debugger: SBA ssh = %" SCNx64
                       " gsba = %" SCNx64
                       " bsurfsba =  %" SCNx64 "\n",
                       sba->SurfaceStateBaseAddress, sba->GeneralStateBaseAddress,
                       sba->BindlessSurfaceStateBaseAddress);
}

DebuggerL0 ::~DebuggerL0() {
    for (auto &alloc : perContextSbaAllocations) {
        device->getMemoryManager()->freeGraphicsMemory(alloc.second);
    }
    device->getMemoryManager()->freeGpuAddress(sbaTrackingGpuVa, device->getRootDeviceIndex());
}

bool DebuggerL0::isDebuggerActive() {
    return true;
}

void DebuggerL0::captureStateBaseAddress(NEO::CommandContainer &container, SbaAddresses sba) {
    if (DebuggerL0::isAnyTrackedAddressChanged(sba)) {
        programSbaTrackingCommands(*container.getCommandStream(), sba);
    }
}

} // namespace L0
