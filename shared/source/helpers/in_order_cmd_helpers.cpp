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

std::shared_ptr<InOrderExecInfo> InOrderExecInfo::create(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount) {
    bool atomicDeviceSignalling = device.getGfxCoreHelper().inOrderAtomicSignallingEnabled(device.getRootDeviceEnvironment());

    UNRECOVERABLE_IF(!deviceCounterNode);

    return std::make_shared<NEO::InOrderExecInfo>(deviceCounterNode, hostCounterNode, device, partitionCount, atomicDeviceSignalling);
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

InOrderExecInfo::InOrderExecInfo(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool atomicDeviceSignalling)
    : device(device), deviceCounterNode(deviceCounterNode), hostCounterNode(hostCounterNode), rootDeviceIndex(device.getRootDeviceIndex()), atomicDeviceSignalling(atomicDeviceSignalling) {

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
        // In atomic + duplicated mode device counter is GPU-owned.
        // Skip host upload only in pure AUB mode to avoid overwrite.
        const bool gpuOwnedDeviceCounter = atomicDeviceSignalling && duplicatedHostStorage;
        const bool skipDeviceCounterUploadToSimulation = gpuOwnedDeviceCounter &&
                                                         simulationUploadCsr->isAubMode() &&
                                                         !simulationUploadCsr->isTbxMode();
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
    allocationOffset = 0;

    initializeAllocationsFromHost();
}

void InOrderExecInfo::resetCounterValue() {
    counterValue = getInitialCounterValue();
    lastWaitedCounterValue[allocationOffset != 0].store(getInitialCounterValue());

    if (getInterruptFence() != nullptr) {
        interruptFence->get()->setFenceValue(getCounterValue());
    }
}

NEO::GraphicsAllocation *InOrderExecInfo::getDeviceCounterAllocation() const {
    return deviceCounterNode ? deviceCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex) : nullptr;
}

NEO::GraphicsAllocation *InOrderExecInfo::getHostCounterAllocation() const {
    return hostCounterNode ? hostCounterNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex) : nullptr;
}

uint64_t InOrderExecInfo::getBaseHostGpuAddress() const {
    return hostCounterNode ? hostCounterNode->getGpuAddress() : 0;
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

void InOrderExecInfo::setupInterruptFence() {
    if (!interruptFence) {
        interruptFence = nullptr;
        device.getDefaultEngine().commandStreamReceiver->allocateUserFence(interruptFence.value());
    }

    if (interruptFence->get() != nullptr) {
        interruptFence->get()->setFenceValue(getCounterValue());
    }
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

void SharableEventDataHelper::releaseResources(MemoryManager &memoryManager) {
    if (eventDataNode) {
        eventDataNode->returnTag();
    } else if (allocation) {
        memoryManager.freeGraphicsMemory(allocation);
    }
}

void SharableEventDataHelper::initializeFromTagNode(TagNodeBase &node, uint32_t rootDeviceIndex) {
    UNRECOVERABLE_IF(eventDataNode || allocation || localTempStorage.get());

    eventDataNode = static_cast<TagNode<InOrderExecEventDataNodeType> *>(&node);
    allocation = eventDataNode->getBaseGraphicsAllocation()->getGraphicsAllocation(rootDeviceIndex);

    eventDataPtr = reinterpret_cast<InOrderExecEventData *>(eventDataNode->tagForCpuAccess);
    allocationOffset = static_cast<size_t>(eventDataNode->getGpuAddress() - allocation->getGpuAddress());
}

void SharableEventDataHelper::initializeFromExternalAllocation(NEO::GraphicsAllocation &newAllocation, size_t offset) {
    UNRECOVERABLE_IF(eventDataNode || allocation || localTempStorage.get());

    allocation = &newAllocation;
    allocationOffset = offset;

    eventDataPtr = reinterpret_cast<InOrderExecEventData *>(ptrOffset(allocation->getUnderlyingBuffer(), offset));
}

void SharableEventDataHelper::initializeLocalTempStorage() {
    UNRECOVERABLE_IF(eventDataNode || allocation || localTempStorage.get());

    localTempStorage = std::make_unique<InOrderExecEventData>();
    eventDataPtr = localTempStorage.get();
}

void InOrderExecEventHelper::releaseResources(MemoryManager &memoryManager) {
    sharableEventDataHelper.releaseResources(memoryManager);
}

void InOrderExecEventHelper::assignData(uint64_t counterValue, uint32_t counterOffset, uint32_t devicePartitions, uint32_t hostPartitions, NEO::GraphicsAllocation *deviceCounterAllocation,
                                        NEO::GraphicsAllocation *hostCounterAllocation, uint64_t baseDeviceAddress, uint64_t baseHostGpuAddress, uint64_t *baseHostCpuAddress, uint64_t incrementValue, uint64_t aggregatedEventUsageCounter,
                                        bool hostStorageDuplicated, bool fromExternalMemory) {

    UNRECOVERABLE_IF(!sharableEventDataHelper.eventDataPtr);
    auto eventDataPtr = sharableEventDataHelper.eventDataPtr;

    eventDataPtr->counterValue = counterValue;
    eventDataPtr->counterOffset = counterOffset;
    eventDataPtr->devicePartitions = devicePartitions;
    eventDataPtr->hostPartitions = hostPartitions;

    this->incrementValue = incrementValue;
    this->baseHostGpuAddress = baseHostGpuAddress;
    this->baseHostCpuAddress = baseHostCpuAddress;
    this->baseDeviceAddress = baseDeviceAddress;
    this->hostStorageDuplicated = hostStorageDuplicated;
    this->fromExternalMemory = fromExternalMemory;
    this->deviceCounterAllocation = deviceCounterAllocation;
    this->hostCounterAllocation = hostCounterAllocation;
    this->aggregatedEventUsageCounter = aggregatedEventUsageCounter;

    dataAssigned = true;
}

void InOrderExecEventHelper::assignInOrderExecInfo(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo) {
    if (this->inOrderExecInfo.get() != newInOrderExecInfo.get()) {
        inOrderExecInfo = newInOrderExecInfo;
    }
}

void InOrderExecEventHelper::updateInOrderExecState(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo, uint64_t newSignalValue, uint32_t newAllocationOffset) {
    assignInOrderExecInfo(newInOrderExecInfo);

    assignData(newSignalValue, newAllocationOffset, inOrderExecInfo->getNumDevicePartitionsToWait(), inOrderExecInfo->getNumHostPartitionsToWait(), inOrderExecInfo->getDeviceCounterAllocation(),
               inOrderExecInfo->getHostCounterAllocation(), inOrderExecInfo->getBaseDeviceAddress(), inOrderExecInfo->getBaseHostGpuAddress(), inOrderExecInfo->getBaseHostAddress(), 0, 0, inOrderExecInfo->isHostStorageDuplicated(),
               inOrderExecInfo->isExternalMemoryExecInfo());
}

void InOrderExecEventHelper::unsetInOrderExecInfo() {
    inOrderExecInfo.reset();

    if (dataAssigned) {
        assignData(0, 0, 0, 0, nullptr, nullptr, 0, 0, nullptr, 0, 0, false, false);
        dataAssigned = false;
    }
}

void InOrderExecEventHelper::releaseNotUsedTempTimestampNodes() {
    if (inOrderExecInfo) {
        inOrderExecInfo->releaseNotUsedTempTimestampNodes(false);
    }
}

void InOrderExecEventHelper::moveTimestampNodesToReleaseList(std::vector<NEO::TagNodeBase *> &nodes) {
    if (inOrderExecInfo) {
        auto eventDataPtr = sharableEventDataHelper.eventDataPtr;
        std::for_each(nodes.cbegin(), nodes.cend(), [&](TagNodeBase *node) {
            inOrderExecInfo->pushTempTimestampNode(node, eventDataPtr->counterValue, eventDataPtr->counterOffset);
        });
    } else {
        std::for_each(nodes.cbegin(), nodes.cend(), [](TagNodeBase *node) {
            node->returnTag();
        });
    }

    nodes.clear();
}

void InOrderExecEventHelper::moveTimestampNodeToReleaseList() {
    moveTimestampNodesToReleaseList(timestampNodes);
}

void InOrderExecEventHelper::moveAdditionalTimestampNodesToReleaseList() {
    moveTimestampNodesToReleaseList(additionalTimestampNodes);
}

void InOrderExecEventHelper::copyData(InOrderExecEventHelper &output) {
    output.assignInOrderExecInfo(inOrderExecInfo);

    auto eventDataPtr = sharableEventDataHelper.eventDataPtr;

    output.assignData(eventDataPtr->counterValue, eventDataPtr->counterOffset, eventDataPtr->devicePartitions, eventDataPtr->hostPartitions,
                      getDeviceCounterAllocation(), getHostCounterAllocation(), getBaseDeviceAddress(), getBaseHostGpuAddress(), getBaseHostCpuAddress(),
                      incrementValue, getAggregatedEventUsageCounter(), isHostStorageDuplicated(), isFromExternalMemory());
}

void InOrderExecEventHelper::initializeFromTagNode(TagNodeBase &node, uint32_t rootDeviceIndex) {
    sharableEventDataHelper.initializeFromTagNode(node, rootDeviceIndex);
}

void InOrderExecEventHelper::initializeFromExternalAllocation(NEO::GraphicsAllocation &newAllocation, size_t offset) {
    sharableEventDataHelper.initializeFromExternalAllocation(newAllocation, offset);
}

void InOrderExecEventHelper::initializeLocalTempStorage() {
    sharableEventDataHelper.initializeLocalTempStorage();
}

void InOrderExecEventHelper::setDeviceAllocIpcHandle(uint64_t handle, size_t offset, unsigned int exportedPid) {
    sharableEventDataHelper.eventDataPtr->deviceAllocIpcHandle = handle;
    sharableEventDataHelper.eventDataPtr->deviceIpcAllocOffset = offset;
    sharableEventDataHelper.eventDataPtr->exporterProcessId = exportedPid;
}

void InOrderExecEventHelper::setHostAllocIpcHandle(uint64_t handle, size_t offset) {
    sharableEventDataHelper.eventDataPtr->hostAllocIpcHandle = handle;
    sharableEventDataHelper.eventDataPtr->hostIpcAllocOffset = offset;
}

void InOrderExecEventHelper::releaseImportedAllocations(NEO::MemoryManager &memoryManager) {
    if (!hasImportedIpcAllocs) {
        return;
    }

    if (this->deviceCounterAllocation) {
        memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(this->deviceCounterAllocation);
        this->deviceCounterAllocation = nullptr;
    }
    if (this->hostCounterAllocation && hostStorageDuplicated) {
        memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(this->hostCounterAllocation);
        this->hostCounterAllocation = nullptr;
    }
}

void InOrderExecEventHelper::assignAllocationsFromImport(NEO::MemoryManager &memoryManager, NEO::GraphicsAllocation &deviceAlloc, NEO::GraphicsAllocation &hostAlloc) {
    releaseImportedAllocations(memoryManager);

    this->deviceCounterAllocation = &deviceAlloc;
    this->baseDeviceAddress = deviceAlloc.getGpuAddress() + sharableEventDataHelper.eventDataPtr->deviceIpcAllocOffset;

    if (&deviceAlloc == &hostAlloc) {
        this->hostCounterAllocation = &hostAlloc;
        this->baseHostGpuAddress = hostAlloc.getGpuAddress() + sharableEventDataHelper.eventDataPtr->deviceIpcAllocOffset;
        this->baseHostCpuAddress = reinterpret_cast<uint64_t *>(ptrOffset(hostAlloc.getUnderlyingBuffer(), sharableEventDataHelper.eventDataPtr->deviceIpcAllocOffset));
        this->hostStorageDuplicated = false;
    } else {
        this->hostCounterAllocation = &hostAlloc;
        this->baseHostGpuAddress = hostAlloc.getGpuAddress() + sharableEventDataHelper.eventDataPtr->hostIpcAllocOffset;
        this->baseHostCpuAddress = reinterpret_cast<uint64_t *>(ptrOffset(hostAlloc.getUnderlyingBuffer(), sharableEventDataHelper.eventDataPtr->hostIpcAllocOffset));
        this->hostStorageDuplicated = true;
    }

    hasImportedIpcAllocs = true;
    dataAssigned = true;
}

bool InOrderExecEventHelper::is2WayIpcImportRefreshNeeded() const {
    if (!is2WayIpcSharingEnabled()) {
        return false;
    }

    const auto eventDataPtr = sharableEventDataHelper.eventDataPtr;

    const bool dataIsDifferent = (eventDataPtr->exporterProcessId != this->imported2WayExportedPid) ||
                                 (eventDataPtr->deviceAllocIpcHandle != this->imported2WayDeviceCounterHandle) ||
                                 (eventDataPtr->deviceIpcAllocOffset != this->imported2WayCounterOffset);

    return dataIsDifferent;
}

} // namespace NEO
