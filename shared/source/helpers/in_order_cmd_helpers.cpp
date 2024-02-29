/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/in_order_cmd_helpers.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/tag_allocator.h"

#include <cstdint>
#include <string.h>
#include <vector>

namespace NEO {

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::create(TagNodeBase *deviceCounterNode, NEO::Device &device, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling) {
    NEO::GraphicsAllocation *hostCounterAllocation = nullptr;

    auto &gfxCoreHelper = device.getGfxCoreHelper();

    if (gfxCoreHelper.duplicatedInOrderCounterStorageEnabled(device.getRootDeviceEnvironment())) {
        NEO::AllocationProperties hostAllocationProperties{device.getRootDeviceIndex(), MemoryConstants::pageSize64k, NEO::AllocationType::bufferHostMemory, device.getDeviceBitfield()};
        hostCounterAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(hostAllocationProperties);

        UNRECOVERABLE_IF(!hostCounterAllocation);
    }

    UNRECOVERABLE_IF(!deviceCounterNode);

    return std::make_shared<NEO::InOrderExecInfo>(deviceCounterNode, hostCounterAllocation, *device.getMemoryManager(), partitionCount, device.getRootDeviceIndex(), regularCmdList, atomicDeviceSignalling);
}

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::createFromExternalAllocation(NEO::Device &device, uint64_t deviceAddress, uint64_t *hostAddress, uint64_t counterValue) {
    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(nullptr, nullptr, *device.getMemoryManager(), 1, device.getRootDeviceIndex(), false, true);

    inOrderExecInfo->counterValue = counterValue;
    inOrderExecInfo->hostAddress = hostAddress;
    inOrderExecInfo->deviceAddress = deviceAddress;
    inOrderExecInfo->duplicatedHostStorage = true;

    return inOrderExecInfo;
}

InOrderExecInfo::~InOrderExecInfo() {
    if (deviceCounterNode) {
        deviceCounterNode->returnTag();
    }
    memoryManager.freeGraphicsMemory(hostCounterAllocation);
}

InOrderExecInfo::InOrderExecInfo(TagNodeBase *deviceCounterNode, NEO::GraphicsAllocation *hostCounterAllocation, NEO::MemoryManager &memoryManager, uint32_t partitionCount, uint32_t rootDeviceIndex,
                                 bool regularCmdList, bool atomicDeviceSignalling)
    : memoryManager(memoryManager), deviceCounterNode(deviceCounterNode), hostCounterAllocation(hostCounterAllocation), rootDeviceIndex(rootDeviceIndex),
      regularCmdList(regularCmdList), atomicDeviceSignalling(atomicDeviceSignalling) {

    numDevicePartitionsToWait = atomicDeviceSignalling ? 1 : partitionCount;
    numHostPartitionsToWait = partitionCount;

    if (hostCounterAllocation) {
        hostAddress = reinterpret_cast<uint64_t *>(hostCounterAllocation->getUnderlyingBuffer());
        duplicatedHostStorage = true;
    } else if (deviceCounterNode) {
        hostAddress = reinterpret_cast<uint64_t *>(deviceCounterNode->getCpuBase());
    }

    if (deviceCounterNode) {
        deviceAddress = deviceCounterNode->getGpuAddress();
    }

    reset();
}

void InOrderExecInfo::initializeAllocationsFromHost() {
    if (deviceCounterNode) {
        const size_t deviceAllocationWriteSize = sizeof(uint64_t) * numDevicePartitionsToWait;
        memset(ptrOffset(deviceCounterNode->getCpuBase(), allocationOffset), 0, deviceAllocationWriteSize);
    }

    if (hostCounterAllocation) {
        const size_t hostAllocationWriteSize = sizeof(uint64_t) * numHostPartitionsToWait;
        memset(ptrOffset(hostCounterAllocation->getUnderlyingBuffer(), allocationOffset), 0, hostAllocationWriteSize);
    }
}

void InOrderExecInfo::reset() {
    resetCounterValue();
    regularCmdListSubmissionCounter = 0;
    allocationOffset = 0;

    initializeAllocationsFromHost();
}

NEO::GraphicsAllocation *InOrderExecInfo::getDeviceCounterAllocation() const {
    return deviceCounterNode ? deviceCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex) : nullptr;
}

} // namespace NEO
