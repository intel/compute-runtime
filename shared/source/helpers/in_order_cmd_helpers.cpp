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

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::create(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool regularCmdList) {
    bool atomicDeviceSignalling = device.getGfxCoreHelper().inOrderAtomicSignallingEnabled(device.getRootDeviceEnvironment());

    UNRECOVERABLE_IF(!deviceCounterNode);

    return std::make_shared<NEO::InOrderExecInfo>(deviceCounterNode, hostCounterNode, *device.getMemoryManager(), partitionCount, device.getRootDeviceIndex(), regularCmdList, atomicDeviceSignalling);
}

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::createFromExternalAllocation(NEO::Device &device, uint64_t deviceAddress, NEO::GraphicsAllocation *hostAllocation, uint64_t *hostAddress, uint64_t counterValue) {
    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(nullptr, nullptr, *device.getMemoryManager(), 1, device.getRootDeviceIndex(), false, true);

    inOrderExecInfo->counterValue = counterValue;
    inOrderExecInfo->externalHostAllocation = hostAllocation;
    inOrderExecInfo->hostAddress = hostAddress;
    inOrderExecInfo->deviceAddress = deviceAddress;
    inOrderExecInfo->duplicatedHostStorage = true;

    return inOrderExecInfo;
}

InOrderExecInfo::~InOrderExecInfo() {
    if (deviceCounterNode) {
        deviceCounterNode->returnTag();
    }
    if (hostCounterNode) {
        hostCounterNode->returnTag();
    }
}

InOrderExecInfo::InOrderExecInfo(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::MemoryManager &memoryManager, uint32_t partitionCount, uint32_t rootDeviceIndex,
                                 bool regularCmdList, bool atomicDeviceSignalling)
    : memoryManager(memoryManager), deviceCounterNode(deviceCounterNode), hostCounterNode(hostCounterNode), rootDeviceIndex(rootDeviceIndex),
      regularCmdList(regularCmdList), atomicDeviceSignalling(atomicDeviceSignalling) {

    numDevicePartitionsToWait = atomicDeviceSignalling ? 1 : partitionCount;
    numHostPartitionsToWait = partitionCount;

    if (hostCounterNode) {
        hostAddress = reinterpret_cast<uint64_t *>(hostCounterNode->getCpuBase());
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

    if (hostCounterNode) {
        const size_t hostAllocationWriteSize = sizeof(uint64_t) * numHostPartitionsToWait;
        memset(ptrOffset(hostCounterNode->getCpuBase(), allocationOffset), 0, hostAllocationWriteSize);
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

NEO::GraphicsAllocation *InOrderExecInfo::getHostCounterAllocation() const {
    return hostCounterNode ? hostCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex) : nullptr;
}

uint64_t InOrderExecInfo::getBaseHostGpuAddress() const {
    return hostCounterNode->getGpuAddress();
}

} // namespace NEO
