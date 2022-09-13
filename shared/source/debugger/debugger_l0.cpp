/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"

#include <cstring>

namespace NEO {

DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE] = {};

DebuggerL0::DebuggerL0(NEO::Device *device) : device(device) {
    isLegacyMode = false;

    const auto deviceCount = std::max(1u, device->getNumSubDevices());
    commandQueueCount.resize(deviceCount);
    uuidL0CommandQueueHandle.resize(deviceCount);

    for (uint32_t i = 0; i < deviceCount; i++) {
        commandQueueCount[i] = 0;
        uuidL0CommandQueueHandle[i] = 0;
    }
    initialize();
}

void DebuggerL0::initialize() {

    initSbaTrackingMode();

    if (NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.get() != -1) {
        setSingleAddressSpaceSbaTracking(NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.get());
    }

    auto &engines = device->getMemoryManager()->getRegisteredEngines();

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false,
                                         device->getDeviceBitfield()};

    if (!singleAddressSpaceSbaTracking) {
        sbaTrackingGpuVa = device->getMemoryManager()->reserveGpuAddress(MemoryConstants::pageSize, device->getRootDeviceIndex());
        properties.gpuAddress = sbaTrackingGpuVa.address;
    }

    SbaTrackedAddresses sbaHeader;

    for (auto &engine : engines) {
        if (!singleAddressSpaceSbaTracking) {
            properties.osContext = engine.osContext;
        }
        properties.subDevicesBitfield = engine.osContext->getDeviceBitfield();

        auto sbaAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        device->getMemoryManager()->copyMemoryToAllocation(sbaAllocation, 0, &sbaHeader, sizeof(sbaHeader));

        perContextSbaAllocations[engine.osContext->getContextId()] = sbaAllocation;
        registerAllocationType(sbaAllocation); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }

    {
        auto &hwInfo = device->getHardwareInfo();
        auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                             NEO::AllocationType::DEBUG_MODULE_AREA,
                                             false,
                                             device->getDeviceBitfield()};
        moduleDebugArea = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        DebugAreaHeader debugArea = {};
        debugArea.reserved1 = 1;
        debugArea.size = sizeof(DebugAreaHeader);
        debugArea.pgsize = 1;
        debugArea.isShared = moduleDebugArea->storageInfo.getNumBanks() == 1;
        debugArea.scratchBegin = sizeof(DebugAreaHeader);
        debugArea.scratchEnd = MemoryConstants::pageSize64k - sizeof(DebugAreaHeader);

        NEO::MemoryOperationsHandler *memoryOperationsIface = device->getRootDeviceEnvironment().memoryOperationsInterface.get();
        if (memoryOperationsIface) {
            memoryOperationsIface->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&moduleDebugArea, 1));
        }

        const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
        NEO::MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *moduleDebugArea),
                                                              *device, moduleDebugArea, 0, &debugArea,
                                                              sizeof(DebugAreaHeader));
        if (hwHelper.disableL3CacheForDebug(hwInfo)) {
            device->getGmmHelper()->forceAllResourcesUncached();
        }

        registerAllocationType(moduleDebugArea); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }
}

void DebuggerL0::printTrackedAddresses(uint32_t contextId) {
    auto memory = perContextSbaAllocations[contextId]->getUnderlyingBuffer();
    auto sba = reinterpret_cast<SbaTrackedAddresses *>(memory);

    PRINT_DEBUGGER_INFO_LOG("Debugger: SBA ssh = %" SCNx64
                            " gsba = %" SCNx64
                            " dsba =  %" SCNx64
                            " ioba =  %" SCNx64
                            " iba =  %" SCNx64
                            " bsurfsba =  %" SCNx64 "\n",
                            sba->SurfaceStateBaseAddress, sba->GeneralStateBaseAddress, sba->DynamicStateBaseAddress,
                            sba->IndirectObjectBaseAddress, sba->InstructionBaseAddress, sba->BindlessSurfaceStateBaseAddress);
}

DebuggerL0 ::~DebuggerL0() {
    for (auto &alloc : perContextSbaAllocations) {
        device->getMemoryManager()->freeGraphicsMemory(alloc.second);
    }
    if (sbaTrackingGpuVa.size != 0) {
        device->getMemoryManager()->freeGpuAddress(sbaTrackingGpuVa, device->getRootDeviceIndex());
    }
    device->getMemoryManager()->freeGraphicsMemory(moduleDebugArea);
}

void DebuggerL0::notifyModuleLoadAllocations(Device *device, const StackVec<NEO::GraphicsAllocation *, 32> &allocs) {
    NEO::MemoryOperationsHandler *memoryOperationsIface = device->getRootDeviceEnvironment().memoryOperationsInterface.get();
    if (memoryOperationsIface) {
        for (auto gfxAlloc : allocs) {
            memoryOperationsIface->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&gfxAlloc, 1));
        }
    }
}
} // namespace NEO
