/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/in_order_cmd_helpers.h"

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include <cstdint>
#include <string.h>
#include <vector>

namespace NEO {

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::create(NEO::Device &device, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling, bool duplicatedHostStorage) {
    NEO::AllocationProperties allocationProperties{device.getRootDeviceIndex(), MemoryConstants::pageSize64k, NEO::AllocationType::timestampPacketTagBuffer, device.getDeviceBitfield()};

    auto inOrderDependencyCounterAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(allocationProperties);

    NEO::GraphicsAllocation *hostCounterAllocation = nullptr;

    if (duplicatedHostStorage) {
        NEO::AllocationProperties hostAllocationProperties{device.getRootDeviceIndex(), MemoryConstants::pageSize64k, NEO::AllocationType::bufferHostMemory, device.getDeviceBitfield()};
        hostCounterAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(hostAllocationProperties);

        UNRECOVERABLE_IF(!hostCounterAllocation);
    }

    UNRECOVERABLE_IF(!inOrderDependencyCounterAllocation);

    return std::make_shared<NEO::InOrderExecInfo>(inOrderDependencyCounterAllocation, hostCounterAllocation, *device.getMemoryManager(), partitionCount, regularCmdList, atomicDeviceSignalling);
}

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::createFromExternalAllocation(NEO::Device &device, uint64_t deviceAddress, uint64_t *hostAddress, uint64_t counterValue) {
    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(nullptr, nullptr, *device.getMemoryManager(), 1, false, true);

    inOrderExecInfo->counterValue = counterValue;
    inOrderExecInfo->hostAddress = hostAddress;
    inOrderExecInfo->deviceAddress = deviceAddress;
    inOrderExecInfo->duplicatedHostStorage = true;

    return inOrderExecInfo;
}

InOrderExecInfo::~InOrderExecInfo() {
    memoryManager.freeGraphicsMemory(deviceCounterAllocation);
    memoryManager.freeGraphicsMemory(hostCounterAllocation);
}

InOrderExecInfo::InOrderExecInfo(NEO::GraphicsAllocation *deviceCounterAllocation, NEO::GraphicsAllocation *hostCounterAllocation, NEO::MemoryManager &memoryManager, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling)
    : memoryManager(memoryManager), deviceCounterAllocation(deviceCounterAllocation), hostCounterAllocation(hostCounterAllocation), regularCmdList(regularCmdList), atomicDeviceSignalling(atomicDeviceSignalling) {

    numDevicePartitionsToWait = atomicDeviceSignalling ? 1 : partitionCount;
    numHostPartitionsToWait = partitionCount;

    if (hostCounterAllocation) {
        hostAddress = reinterpret_cast<uint64_t *>(hostCounterAllocation->getUnderlyingBuffer());
        duplicatedHostStorage = true;
    } else if (deviceCounterAllocation) {
        hostAddress = reinterpret_cast<uint64_t *>(deviceCounterAllocation->getUnderlyingBuffer());
    }

    if (deviceCounterAllocation) {
        deviceAddress = deviceCounterAllocation->getGpuAddress();
    }

    reset();
}

void InOrderExecInfo::reset() {
    resetCounterValue();
    regularCmdListSubmissionCounter = 0;
    allocationOffset = 0;

    if (deviceCounterAllocation) {
        memset(deviceCounterAllocation->getUnderlyingBuffer(), 0, deviceCounterAllocation->getUnderlyingBufferSize());
    } else {
        DEBUG_BREAK_IF(!deviceAddress);
    }

    if (hostCounterAllocation) {
        memset(hostCounterAllocation->getUnderlyingBuffer(), 0, hostCounterAllocation->getUnderlyingBufferSize());
    }
}

} // namespace NEO
