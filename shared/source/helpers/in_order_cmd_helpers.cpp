/*
 * Copyright (C) 2023-2026 Intel Corporation
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

    setSimulationUploadCsr(device.getDefaultEngine().commandStreamReceiver);

    reset();
}

void InOrderExecInfo::setSimulationUploadCsr(NEO::CommandStreamReceiver *csr) {
    simulationUploadCsr = csr;

    auto selectedCsr = simulationUploadCsr ? simulationUploadCsr : device.getDefaultEngine().commandStreamReceiver;
    immWritePostSyncWriteOffset = std::max(selectedCsr->getImmWritePostSyncWriteOffset(), static_cast<uint32_t>(sizeof(uint64_t)));
    isSimulationMode = selectedCsr->isTbxMode() || selectedCsr->isAubMode();
}

void InOrderExecInfo::uploadCounterNodeToSimulation(TagNodeBase &node, size_t size) {
    if (!simulationUploadCsr) {
        return;
    }
    simulationUploadCsr->writeTagAllocationChunkToSimulation(node, this->allocationOffset, size);
}

void InOrderExecInfo::uploadAllocationsToSimulation() {
    if (!isSimulationMode || !simulationUploadCsr) {
        return;
    }

    const bool csrChanged = (lastSimulationUploadCsr != simulationUploadCsr);
    if (!simulationUploadDirty && !csrChanged) {
        return;
    }

    const auto getUploadSize = [&](uint32_t partitionCount) -> size_t {
        return alignUp(sizeof(uint64_t), immWritePostSyncWriteOffset) * partitionCount;
    };

    if (deviceCounterNode) {
        // In atomic + duplicated mode device counter is GPU-owned, host upload would overwrite it.
        const bool skipDeviceCounterUploadToSimulation = atomicDeviceSignalling && duplicatedHostStorage;
        if (!skipDeviceCounterUploadToSimulation) {
            uploadCounterNodeToSimulation(*deviceCounterNode, getUploadSize(numDevicePartitionsToWait));
        }
    }

    if (hostCounterNode) {
        uploadCounterNodeToSimulation(*hostCounterNode, getUploadSize(numHostPartitionsToWait));
    }

    lastSimulationUploadCsr = simulationUploadCsr;
    simulationUploadDirty = false;
}

void InOrderExecInfo::initializeAllocationsFromHost() {
    initializeAllocationsFromHost(true);
}

void InOrderExecInfo::initializeAllocationsFromHost(bool shouldUploadToSimulation) {
    const uint64_t initialValue = getInitialCounterValue();

    const auto initializeNodeFromHost = [&](TagNodeBase &node, uint32_t partitionCount) {
        for (uint32_t i = 0; i < partitionCount; i++) {
            uint64_t *ptr = reinterpret_cast<uint64_t *>(ptrOffset(node.getCpuBase(), allocationOffset + (i * immWritePostSyncWriteOffset)));
            *ptr = initialValue;
        }
    };

    if (deviceCounterNode) {
        initializeNodeFromHost(*deviceCounterNode, numDevicePartitionsToWait);
    }

    if (hostCounterNode) {
        initializeNodeFromHost(*hostCounterNode, numHostPartitionsToWait);
    }

    simulationUploadDirty = true;

    if (shouldUploadToSimulation) {
        uploadAllocationsToSimulation();
    }
}

void InOrderExecInfo::reset() {
    resetCounterValue();
    regularCmdListSubmissionCounter = 0;
    allocationOffset = 0;

    initializeAllocationsFromHost();
}

void InOrderExecInfo::resetCounterValue() {
    counterValue = getInitialCounterValue();
    lastWaitedCounterValue[allocationOffset != 0].store(getInitialCounterValue());
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

void InOrderExecInfo::pushTempTimestampNode(TagNodeBase *node, uint64_t value, uint32_t allocationOffset) {
    std::unique_lock<std::mutex> lock(mutex);

    tempTimestampNodes.emplace_back(node, std::make_pair(value, allocationOffset));
}

void InOrderExecInfo::releaseNotUsedTempTimestampNodes(bool forceReturn) {
    std::unique_lock<std::mutex> lock(mutex);

    std::vector<std::pair<TagNodeBase *, CounterAndOffsetPairT>> tempVector;

    for (auto &node : tempTimestampNodes) {
        const auto &counterAndOffsetPair = node.second;
        if (forceReturn || isCounterAlreadyDone(counterAndOffsetPair.first, counterAndOffsetPair.second)) {
            node.first->returnTag();
        } else {
            tempVector.push_back(node);
        }
    }

    tempTimestampNodes.swap(tempVector);
}

uint64_t InOrderExecInfo::getHostNodeGpuAddress() const {
    if (hostCounterNode) {
        return hostCounterNode->getGpuAddress() + allocationOffset;
    }
    return 0;
}

uint64_t InOrderExecInfo::getDeviceNodeGpuAddress() const {
    if (deviceCounterNode) {
        return deviceCounterNode->getGpuAddress() + allocationOffset;
    }
    return 0;
}

uint64_t InOrderExecInfo::getInitialCounterValue() const {
    return debugManager.flags.InitialCounterBasedEventValue.getIfNotDefault<uint64_t>(0);
}

void InOrderExecEventHelper::updateInOrderExecState(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo, uint64_t newSignalValue, uint32_t newAllocationOffset) {
    if (this->inOrderExecInfo.get() != newInOrderExecInfo.get()) {
        inOrderExecInfo = newInOrderExecInfo;
    }

    if (!eventData) {
        eventData = std::make_unique<InOrderExecEventData>();
    }

    eventData->counterValue = newSignalValue;
    eventData->counterOffset = newAllocationOffset;
    eventData->devicePartitions = inOrderExecInfo->getNumDevicePartitionsToWait();
    eventData->hostPartitions = inOrderExecInfo->getNumHostPartitionsToWait();
    baseHostAddress = inOrderExecInfo->getBaseHostAddress();
    baseDeviceAddress = inOrderExecInfo->getBaseDeviceAddress();
    hostStorageDuplicated = inOrderExecInfo->isHostStorageDuplicated();
    if (inOrderExecInfo->isExternalMemoryExecInfo()) {
        deviceCounterAllocation = inOrderExecInfo->getExternalDeviceAllocation();
        hostCounterAllocation = inOrderExecInfo->getExternalHostAllocation();
    } else {
        deviceCounterAllocation = inOrderExecInfo->getDeviceCounterAllocation();
        hostCounterAllocation = inOrderExecInfo->getHostCounterAllocation();
    }

    dataAssigned = true;
}

void InOrderExecEventHelper::unsetInOrderExecInfo() {
    dataAssigned = false;
    inOrderExecInfo.reset();
    baseHostAddress = nullptr;
    deviceCounterAllocation = nullptr;
    hostCounterAllocation = nullptr;
    hostStorageDuplicated = false;
    baseDeviceAddress = 0;
    aggregatedEventUsageCounter = 0;

    if (eventData) {
        eventData->counterValue = 0;
        eventData->counterOffset = 0;
        eventData->devicePartitions = 0;
        eventData->hostPartitions = 0;
        eventData->incrementValue = 0;
    }
}

void InOrderExecEventHelper::releaseNotUsedTempTimestampNodes() {
    if (inOrderExecInfo) {
        inOrderExecInfo->releaseNotUsedTempTimestampNodes(false);
    }
}

void InOrderExecEventHelper::moveTimestampNodeToReleaseList() {
    for (auto &node : timestampNodes) {
        inOrderExecInfo->pushTempTimestampNode(node, eventData->counterValue, eventData->counterOffset);
    }
    timestampNodes.clear();
}

void InOrderExecEventHelper::moveAdditionalTimestampNodesToReleaseList() {
    if (inOrderExecInfo) {
        std::for_each(additionalTimestampNodes.cbegin(), additionalTimestampNodes.cend(), [&](TagNodeBase *node) {
            inOrderExecInfo->pushTempTimestampNode(node, eventData->counterValue, eventData->counterOffset);
        });
    } else {
        std::for_each(additionalTimestampNodes.cbegin(), additionalTimestampNodes.cend(), [&](TagNodeBase *node) {
            node->returnTag();
        });
    }

    additionalTimestampNodes.clear();
}

uint64_t InOrderExecEventHelper::getExecSignalValueWithSubmissionCounter() const {
    uint64_t appendCounter = inOrderExecInfo ? NEO::InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo) : 0;
    return (eventData->counterValue + appendCounter);
}

} // namespace NEO
