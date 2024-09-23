/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE] = {};

DebuggerL0::DebuggerL0(NEO::Device *device) : device(device) {
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

    if (NEO::debugManager.flags.DebuggerForceSbaTrackingMode.get() != -1) {
        setSingleAddressSpaceSbaTracking(NEO::debugManager.flags.DebuggerForceSbaTrackingMode.get());
    }

    auto &engines = device->getMemoryManager()->getRegisteredEngines(device->getRootDeviceIndex());

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false,
                                         device->getDeviceBitfield()};

    if (!singleAddressSpaceSbaTracking) {
        RootDeviceIndicesContainer rootDeviceIndices;
        rootDeviceIndices.pushUnique(device->getRootDeviceIndex());
        uint32_t rootDeviceIndexReserved = 0;
        sbaTrackingGpuVa = device->getMemoryManager()->reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);
        UNRECOVERABLE_IF(sbaTrackingGpuVa.address == 0u);
        properties.gpuAddress = sbaTrackingGpuVa.address;
    }

    SbaTrackedAddresses sbaHeader{};

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
        auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();

        NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                             NEO::AllocationType::debugModuleArea,
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

        NEO::MemoryOperationsHandler *memoryOperationsIface = rootDeviceEnvironment.memoryOperationsInterface.get();
        if (memoryOperationsIface) {
            memoryOperationsIface->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&moduleDebugArea, 1), false);
            auto numSubDevices = device->getNumSubDevices();
            for (uint32_t i = 0; i < numSubDevices; i++) {
                auto subDevice = device->getSubDevice(i);
                memoryOperationsIface->makeResident(subDevice, ArrayRef<NEO::GraphicsAllocation *>(&moduleDebugArea, 1), false);
            }
        }

        const auto &productHelper = device->getProductHelper();
        NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *moduleDebugArea),
                                                              *device, moduleDebugArea, 0, &debugArea,
                                                              sizeof(DebugAreaHeader));
        if (productHelper.disableL3CacheForDebug(hwInfo)) {
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
                            sba->surfaceStateBaseAddress, sba->generalStateBaseAddress, sba->dynamicStateBaseAddress,
                            sba->indirectObjectBaseAddress, sba->instructionBaseAddress, sba->bindlessSurfaceStateBaseAddress);
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
            memoryOperationsIface->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&gfxAlloc, 1), false);
        }
    }
}
} // namespace NEO
