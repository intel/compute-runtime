/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/in_order_cmd_helpers.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/utilities/tag_allocator.h"

#include <cstdint>
#include <string.h>
#include <vector>

namespace NEO {

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::create(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool regularCmdList) {
    bool atomicDeviceSignalling = device.getGfxCoreHelper().inOrderAtomicSignallingEnabled(device.getRootDeviceEnvironment());

    UNRECOVERABLE_IF(!deviceCounterNode);

    return std::make_shared<NEO::InOrderExecInfo>(deviceCounterNode, hostCounterNode, device, partitionCount, regularCmdList, atomicDeviceSignalling);
}

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::createFromExternalAllocation(NEO::Device &device, NEO::GraphicsAllocation *deviceAllocation, uint64_t deviceAddress, NEO::GraphicsAllocation *hostAllocation,
                                                                               uint64_t *hostAddress, uint64_t counterValue, uint32_t devicePartitions, uint32_t hostPartitions) {
    auto inOrderExecInfo = std::make_shared<NEO::InOrderExecInfo>(nullptr, nullptr, device, 1, false, true);

    inOrderExecInfo->counterValue = counterValue;
    inOrderExecInfo->externalHostAllocation = hostAllocation;
    inOrderExecInfo->externalDeviceAllocation = deviceAllocation;
    inOrderExecInfo->hostAddress = hostAddress;
    inOrderExecInfo->deviceAddress = deviceAddress;
    inOrderExecInfo->duplicatedHostStorage = (deviceAllocation != hostAllocation);
    inOrderExecInfo->numDevicePartitionsToWait = devicePartitions;
    inOrderExecInfo->numHostPartitionsToWait = hostPartitions;

    return inOrderExecInfo;
}

InOrderExecInfo::~InOrderExecInfo() {
    if (deviceCounterNode) {
        deviceCounterNode->returnTag();
    }
    if (hostCounterNode) {
        hostCounterNode->returnTag();
    }

    // forced return - All related objects (CmdList and Events) already destroyed
    releaseNotUsedTempTimestampNodes(true);
}

InOrderExecInfo::InOrderExecInfo(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling)
    : device(device), deviceCounterNode(deviceCounterNode), hostCounterNode(hostCounterNode), rootDeviceIndex(device.getRootDeviceIndex()),
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

    isTbx = device.getDefaultEngine().commandStreamReceiver->isTbxMode();

    reset();
}

void InOrderExecInfo::uploadToTbx(TagNodeBase &node, size_t size) {
    constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();

    auto csr = device.getDefaultEngine().commandStreamReceiver;

    auto allocation = node.getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex);
    auto offset = ptrDiff(node.getGpuAddress(), allocation->getGpuAddress()) + this->allocationOffset;

    if (allocation->isTbxWritable(allBanks)) {
        // initialize full page tables for the first time
        csr->writeMemory(*allocation, false, 0, 0);
    } else {
        // chunk write if allocation already initialized
        allocation->setTbxWritable(true, allBanks);
        csr->writeMemory(*allocation, true, offset, size);
    }
    allocation->setTbxWritable(false, allBanks);
}

void InOrderExecInfo::initializeAllocationsFromHost() {
    if (deviceCounterNode) {
        const size_t deviceAllocationWriteSize = sizeof(uint64_t) * numDevicePartitionsToWait;
        memset(ptrOffset(deviceCounterNode->getCpuBase(), allocationOffset), 0, deviceAllocationWriteSize);

        if (isTbx) {
            uploadToTbx(*deviceCounterNode, deviceAllocationWriteSize);
        }
    }

    if (hostCounterNode) {
        const size_t hostAllocationWriteSize = sizeof(uint64_t) * numHostPartitionsToWait;
        memset(ptrOffset(hostCounterNode->getCpuBase(), allocationOffset), 0, hostAllocationWriteSize);

        if (isTbx) {
            uploadToTbx(*hostCounterNode, hostAllocationWriteSize);
        }
    }
}

void InOrderExecInfo::reset() {
    resetCounterValue();
    regularCmdListSubmissionCounter = 0;
    allocationOffset = 0;

    initializeAllocationsFromHost();
}

NEO::GraphicsAllocation *InOrderExecInfo::getDeviceCounterAllocation() const {
    if (externalDeviceAllocation) {
        return externalDeviceAllocation;
    }
    return deviceCounterNode ? deviceCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex) : nullptr;
}

NEO::GraphicsAllocation *InOrderExecInfo::getHostCounterAllocation() const {
    if (externalHostAllocation) {
        return externalHostAllocation;
    }
    return hostCounterNode ? hostCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex) : nullptr;
}

uint64_t InOrderExecInfo::getBaseHostGpuAddress() const {
    return hostCounterNode->getGpuAddress();
}

void InOrderExecInfo::pushTempTimestampNode(TagNodeBase *node, uint64_t value) {
    std::unique_lock<std::mutex> lock(mutex);

    tempTimestampNodes.emplace_back(node, value);
}

void InOrderExecInfo::releaseNotUsedTempTimestampNodes(bool forceReturn) {
    std::unique_lock<std::mutex> lock(mutex);

    std::vector<std::pair<TagNodeBase *, uint64_t>> tempVector;

    for (auto &node : tempTimestampNodes) {
        if (forceReturn || lastWaitedCounterValue >= node.second) {
            node.first->returnTag();
        } else {
            tempVector.push_back(node);
        }
    }

    tempTimestampNodes.swap(tempVector);
}

} // namespace NEO
