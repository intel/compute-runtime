/*
 * Copyright (C) 2020-2021 Intel Corporation
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
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"

#include <cstring>

namespace L0 {

DebugerL0CreateFn debuggerL0Factory[IGFX_MAX_CORE] = {};

DebuggerL0::DebuggerL0(NEO::Device *device) : device(device) {
    isLegacyMode = false;
    initialize();
}

void DebuggerL0::initialize() {
    auto &engines = device->getMemoryManager()->getRegisteredEngines();

    sbaTrackingGpuVa = device->getMemoryManager()->reserveGpuAddress(MemoryConstants::pageSize, device->getRootDeviceIndex());

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::GraphicsAllocation::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false,
                                         device->getDeviceBitfield()};

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

    {
        auto &hwInfo = device->getHardwareInfo();
        auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                             NEO::GraphicsAllocation::AllocationType::DEBUG_MODULE_AREA,
                                             false,
                                             device->getDeviceBitfield()};
        moduleDebugArea = device->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        bool bindlessSip = NEO::DebugManager.flags.UseBindlessDebugSip.get();

        DebugAreaHeader debugArea = {};
        debugArea.reserved1 = bindlessSip ? 1 : 0;
        debugArea.size = sizeof(DebugAreaHeader);
        debugArea.pgsize = 1;
        debugArea.isShared = moduleDebugArea->storageInfo.getNumBanks() == 1;
        debugArea.scratchBegin = sizeof(DebugAreaHeader);
        debugArea.scratchEnd = MemoryConstants::pageSize64k - sizeof(DebugAreaHeader);

        NEO::MemoryOperationsHandler *memoryOperationsIface = device->getRootDeviceEnvironment().memoryOperationsInterface.get();
        if (memoryOperationsIface) {
            memoryOperationsIface->makeResident(device, ArrayRef<NEO::GraphicsAllocation *>(&moduleDebugArea, 1));
        }

        NEO::MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *moduleDebugArea),
                                                              *device, moduleDebugArea, 0, &debugArea,
                                                              sizeof(DebugAreaHeader));
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
    device->getMemoryManager()->freeGpuAddress(sbaTrackingGpuVa, device->getRootDeviceIndex());
    device->getMemoryManager()->freeGraphicsMemory(moduleDebugArea);
}

void DebuggerL0::captureStateBaseAddress(NEO::CommandContainer &container, SbaAddresses sba) {
    if (DebuggerL0::isAnyTrackedAddressChanged(sba)) {
        programSbaTrackingCommands(*container.getCommandStream(), sba);
    }
}

void DebuggerL0::getAttentionBitmaskForThread(uint32_t slice, uint32_t subslice, uint32_t eu, uint32_t thread, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;

    bitmaskSize = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice * bytesPerEu;
    bitmask = std::make_unique<uint8_t[]>(bitmaskSize);

    int threadValue = 0xff;
    if (numThreadsPerEu == 7) {
        threadValue = 0x7f;
    }

    if (slice == UINT32_MAX && subslice == UINT32_MAX && eu == UINT32_MAX && thread == UINT32_MAX) {
        memset(bitmask.get(), threadValue, bitmaskSize);
        return;
    }

    for (uint32_t sliceID = 0; sliceID < hwInfo.gtSystemInfo.MaxSlicesSupported; sliceID++) {
        if (slice != UINT32_MAX) {
            sliceID = slice;
        }
        uint8_t *sliceData = ptrOffset(bitmask.get(), threadsSizePerSlice * sliceID);

        for (uint32_t subsliceID = 0; subsliceID < numSubslicesPerSlice; subsliceID++) {
            if (subslice != UINT32_MAX) {
                subsliceID = subslice;
            }
            uint8_t *subsliceData = ptrOffset(sliceData, numEuPerSubslice * bytesPerEu * subsliceID);

            for (uint32_t euID = 0; euID < numEuPerSubslice; euID++) {
                if (eu != UINT32_MAX) {
                    euID = eu;
                }
                uint8_t *euData = ptrOffset(subsliceData, bytesPerEu * euID);

                if (thread == UINT32_MAX) {
                    *euData = threadValue;
                } else {
                    UNRECOVERABLE_IF(thread > 7);
                    *euData = (1 << thread);
                }

                UNRECOVERABLE_IF(numThreadsPerEu > 8);

                if (eu != UINT32_MAX) {
                    break;
                }
            }
            if (subslice != UINT32_MAX) {
                break;
            }
        }
        if (slice != UINT32_MAX) {
            break;
        }
    }
}

void DebuggerL0::getAttentionBitmaskForSingleThreads(std::vector<ze_device_thread_t> &threads, const NEO::HardwareInfo &hwInfo, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;

    bitmaskSize = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice * bytesPerEu;
    bitmask = std::make_unique<uint8_t[]>(bitmaskSize);

    memset(bitmask.get(), 0, bitmaskSize);

    for (auto &thread : threads) {
        uint8_t *sliceData = ptrOffset(bitmask.get(), threadsSizePerSlice * thread.slice);
        uint8_t *subsliceData = ptrOffset(sliceData, numEuPerSubslice * bytesPerEu * thread.subslice);
        uint8_t *euData = ptrOffset(subsliceData, bytesPerEu * thread.eu);
        UNRECOVERABLE_IF(thread.thread > 7);
        *euData |= (1 << thread.thread);
    }
}

std::vector<ze_device_thread_t> DebuggerL0::getThreadsFromAttentionBitmask(const NEO::HardwareInfo &hwInfo, const uint8_t *bitmask, const size_t bitmaskSize) {
    const uint32_t numSubslicesPerSlice = hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported;
    const uint32_t numEuPerSubslice = hwInfo.gtSystemInfo.MaxEuPerSubSlice;
    const uint32_t numThreadsPerEu = (hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount);
    const uint32_t bytesPerEu = alignUp(numThreadsPerEu, 8) / 8;
    const uint32_t threadsSizePerSlice = numSubslicesPerSlice * numEuPerSubslice * bytesPerEu;
    const uint32_t threadsSizePerSubSlice = numEuPerSubslice * bytesPerEu;

    UNRECOVERABLE_IF(bytesPerEu != 1);
    std::vector<ze_device_thread_t> threads;

    for (uint32_t slice = 0; slice < hwInfo.gtSystemInfo.MaxSlicesSupported; slice++) {
        for (uint32_t subslice = 0; subslice < numSubslicesPerSlice; subslice++) {
            for (uint32_t eu = 0; eu < hwInfo.gtSystemInfo.MaxEuPerSubSlice; eu++) {
                size_t offset = slice * threadsSizePerSlice + subslice * threadsSizePerSubSlice + eu * bytesPerEu;

                if (offset >= bitmaskSize) {
                    return threads;
                }

                std::bitset<8> bits(bitmask[offset]);
                for (uint32_t i = 0; i < 8; i++) {
                    if (bits.test(i)) {
                        threads.emplace_back(ze_device_thread_t{slice, subslice, eu, i});
                    }
                }
            }
        }
    }

    return threads;
}

} // namespace L0
