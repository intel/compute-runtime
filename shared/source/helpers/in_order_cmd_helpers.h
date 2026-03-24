/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/mt_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/defs.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class GraphicsAllocation;
class MemoryManager;
class Device;
class TagNodeBase;
class CommandStreamReceiver;

template <typename TagType>
class TagNode;

template <bool deviceAlloc>
class DeviceAllocNodeType {
  public:
    using ValueT = uint64_t;

    static constexpr uint32_t defaultAllocatorTagCount = 128;

    static constexpr AllocationType getAllocationType() { return deviceAlloc ? NEO::AllocationType::gpuTimestampDeviceBuffer : NEO::AllocationType::timestampPacketTagBuffer; }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::counter64b; }

    static constexpr size_t getSinglePacketSize() { return sizeof(uint64_t); }

    void initialize(uint64_t initValue) { data = initValue; }

  protected:
    uint64_t data = {};
};

static_assert(sizeof(uint64_t) == sizeof(DeviceAllocNodeType<true>), "This structure is consumed by GPU and has to follow specific restrictions for padding and size");
static_assert(sizeof(uint64_t) == sizeof(DeviceAllocNodeType<false>), "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

class InOrderExecInfo : public NEO::NonCopyableClass {
  public:
    ~InOrderExecInfo();

    InOrderExecInfo() = delete;

    static std::shared_ptr<InOrderExecInfo> create(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount);

    InOrderExecInfo(TagNodeBase *deviceCounterNode, TagNodeBase *hostCounterNode, NEO::Device &device, uint32_t partitionCount, bool atomicDeviceSignalling);

    NEO::GraphicsAllocation *getDeviceCounterAllocation() const;
    NEO::GraphicsAllocation *getHostCounterAllocation() const;
    uint64_t *getBaseHostAddress() const { return hostAddress; }
    uint64_t getBaseDeviceAddress() const { return deviceAddress; }
    uint64_t getBaseHostGpuAddress() const;

    uint64_t getDeviceNodeGpuAddress() const;
    uint64_t getHostNodeGpuAddress() const;
    size_t getDeviceNodeWriteSize() const {
        if (deviceCounterNode) {
            const size_t deviceAllocationWriteSize = sizeof(uint64_t) * numDevicePartitionsToWait;
            return deviceAllocationWriteSize;
        }
        return 0;
    }
    size_t getHostNodeWriteSize() const {
        if (hostCounterNode) {
            const size_t hostAllocationWriteSize = sizeof(uint64_t) * numHostPartitionsToWait;
            return hostAllocationWriteSize;
        }
        return 0;
    }

    uint64_t getCounterValue() const { return counterValue; }
    void addCounterValue(uint64_t addValue) { counterValue += addValue; }
    void resetCounterValue();

    bool isHostStorageDuplicated() const { return duplicatedHostStorage; }
    bool isAtomicDeviceSignalling() const { return atomicDeviceSignalling; }

    uint32_t getNumDevicePartitionsToWait() const { return numDevicePartitionsToWait; }
    uint32_t getNumHostPartitionsToWait() const { return numHostPartitionsToWait; }

    void setAllocationOffset(uint32_t newOffset) { allocationOffset = newOffset; }
    void initializeAllocationsFromHost();
    void initializeAllocationsFromHost(bool shouldUploadToSimulation);
    void uploadAllocationsToSimulation();
    uint32_t getAllocationOffset() const { return allocationOffset; }
    void setSimulationUploadCsr(NEO::CommandStreamReceiver *csr);

    void reset();
    bool isExternalMemoryExecInfo() const { return deviceCounterNode == nullptr; }
    void setLastWaitedCounterValue(uint64_t value, uint32_t allocationOffset) {
        if (!isExternalMemoryExecInfo()) {
            NEO::MultiThreadHelpers::interlockedMax(lastWaitedCounterValue[allocationOffset != 0], value);
        }
    }

    bool isCounterAlreadyDone(uint64_t waitValue, uint32_t allocationOffset) const {
        return lastWaitedCounterValue[allocationOffset != 0] >= waitValue;
    }

    void pushTempTimestampNode(TagNodeBase *node, uint64_t value, uint32_t allocationOffset);
    void releaseNotUsedTempTimestampNodes(bool forceReturn);
    void setupInterruptFence();
    SyncFence *getInterruptFence() { return interruptFence ? interruptFence->get() : nullptr; }

    uint64_t getInitialCounterValue() const;

  protected:
    using CounterAndOffsetPairT = std::pair<uint64_t, uint32_t>;

    void uploadCounterNodeToSimulation(TagNodeBase &node, size_t size);

    NEO::Device &device;
    NEO::TagNodeBase *deviceCounterNode = nullptr;
    NEO::TagNodeBase *hostCounterNode = nullptr;
    std::vector<std::pair<NEO::TagNodeBase *, CounterAndOffsetPairT>> tempTimestampNodes;

    std::mutex mutex;
    std::atomic<uint64_t> lastWaitedCounterValue[2] = {0, 0}; // [0] for offset == 0, [1] for offset != 0

    uint64_t counterValue = 0;
    uint64_t deviceAddress = 0;
    uint64_t *hostAddress = nullptr;
    uint32_t numDevicePartitionsToWait = 0;
    uint32_t numHostPartitionsToWait = 0;
    uint32_t allocationOffset = 0;
    uint32_t rootDeviceIndex = 0;
    uint32_t immWritePostSyncWriteOffset = 0;
    std::optional<std::unique_ptr<SyncFence>> interruptFence;
    bool duplicatedHostStorage = false;
    bool atomicDeviceSignalling = false;
    bool isSimulationMode = false;
    bool simulationUploadDirty = false;
    NEO::CommandStreamReceiver *simulationUploadCsr = nullptr;
    NEO::CommandStreamReceiver *lastSimulationUploadCsr = nullptr;
};

static_assert(NEO::NonCopyable<InOrderExecInfo>);

// Used for IPC exchange - keep minimal set of data to share
#pragma pack(1)
struct InOrderExecEventData {
    uint64_t deviceAllocIpcHandle = 0;
    uint64_t hostAllocIpcHandle = 0;
    uint64_t counterValue = 0;
    size_t deviceIpcAllocOffset = 0;
    size_t hostIpcAllocOffset = 0;
    uint32_t counterOffset = 0;
    uint32_t devicePartitions = 0;
    uint32_t hostPartitions = 0;
    unsigned int exporterProcessId = 0;
};
#pragma pack()

class InOrderExecEventDataNodeType {
  public:
    using ValueT = uint8_t;

    static constexpr AllocationType getAllocationType() { return NEO::AllocationType::internalHostMemory; }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::inOrderIpcData; }

    static constexpr size_t getSinglePacketSize() { return sizeof(InOrderExecEventData); }

    void initialize(uint8_t initValue) { memset(data, 0, sizeof(data)); }

  protected:
    uint8_t data[sizeof(InOrderExecEventData)] = {};
};

static_assert(sizeof(InOrderExecEventData) == sizeof(InOrderExecEventDataNodeType));

class SharableEventDataHelper {
  public:
    void releaseResources(MemoryManager &memoryManager);

    // TagNode is a default mode
    // External allocation is used in case of IPC import of TagNode (from another process)
    // Local temp storage is used for temporary reference copy. For example, when recording a Graph

    void initializeFromTagNode(TagNodeBase &node, uint32_t rootDeviceIndex);
    void initializeFromExternalAllocation(NEO::GraphicsAllocation &newAllocation, size_t offset);
    void initializeLocalTempStorage();

    GraphicsAllocation *getAllocation() const { return allocation; }
    size_t getAllocationOffset() const { return allocationOffset; }

    InOrderExecEventData *eventDataPtr = nullptr;

  protected:
    std::unique_ptr<InOrderExecEventData> localTempStorage;
    TagNode<InOrderExecEventDataNodeType> *eventDataNode = nullptr;
    GraphicsAllocation *allocation = nullptr;
    size_t allocationOffset = 0;
};

class InOrderExecEventHelper : public NonCopyableClass {
  public:
    void initializeFromTagNode(TagNodeBase &node, uint32_t rootDeviceIndex);
    void initializeFromExternalAllocation(NEO::GraphicsAllocation &newAllocation, size_t offset);
    void initializeLocalTempStorage();

    void releaseResources(MemoryManager &memoryManager);
    void updateInOrderExecState(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo, uint64_t newSignalValue, uint32_t newAllocationOffset);
    void copyData(InOrderExecEventHelper &output);

    bool isDataAssigned() const { return dataAssigned; }

    bool isCounterAlreadyDone(uint64_t waitValue, uint32_t offset) const { return inOrderExecInfo.get() ? inOrderExecInfo->isCounterAlreadyDone(waitValue, offset) : false; }
    void setLastWaitedCounterValue(uint64_t value, uint32_t allocationOffset) {
        if (inOrderExecInfo) {
            inOrderExecInfo->setLastWaitedCounterValue(value, allocationOffset);
        }
    }
    SyncFence *getInterruptFence() const { return inOrderExecInfo.get() ? inOrderExecInfo->getInterruptFence() : nullptr; }

    const InOrderExecEventData *getEventData() const { return sharableEventDataHelper.eventDataPtr; }

    void setDeviceAllocIpcHandle(uint64_t handle, size_t offset, unsigned int exportedPid);
    void setHostAllocIpcHandle(uint64_t handle, size_t offset);

    uint64_t *getBaseHostCpuAddress() const { return baseHostCpuAddress; }
    uint64_t getBaseHostGpuAddress() const { return baseHostGpuAddress; }
    uint64_t getBaseDeviceAddress() const { return baseDeviceAddress; }

    NEO::GraphicsAllocation *getDeviceCounterAllocation() const { return deviceCounterAllocation; }
    NEO::GraphicsAllocation *getHostCounterAllocation() const { return hostCounterAllocation; }
    bool isHostStorageDuplicated() const { return hostStorageDuplicated; }
    bool isFromExternalMemory() const { return fromExternalMemory; }

    uint64_t getIncrementValue() const { return incrementValue; }

    NEO::TagNodeBase *getLatestTimestampNode() const { return timestampNodes.back(); }
    NEO::TagNodeBase *getTimestampNode(size_t index) const { return timestampNodes[index]; }
    void addTimestampNode(NEO::TagNodeBase *node) { timestampNodes.push_back(node); }
    bool hasTimestampNodes() const { return !timestampNodes.empty(); }
    size_t getTimestampNodesCount() const { return timestampNodes.size(); }

    NEO::TagNodeBase *getAdditionalTimestampNode(size_t index) const { return additionalTimestampNodes[index]; }
    void addAdditionalTimestampNode(NEO::TagNodeBase *node) { additionalTimestampNodes.push_back(node); }
    bool hasAdditionalTimestampNodes() const { return !additionalTimestampNodes.empty(); }
    size_t getAdditionalTimestampNodesCount() const { return additionalTimestampNodes.size(); }

    void releaseNotUsedTempTimestampNodes();
    void moveTimestampNodeToReleaseList();
    void moveAdditionalTimestampNodesToReleaseList();
    void unsetInOrderExecInfo();

    uint64_t getAggregatedEventUsageCounter() const { return aggregatedEventUsageCounter; }
    void addAggregatedEventUsageCounter(uint64_t addValue) { aggregatedEventUsageCounter += addValue; }
    void resetAggregatedEventUsageCounter() { aggregatedEventUsageCounter = 0; }

    void assignData(uint64_t counterValue, uint32_t counterOffset, uint32_t devicePartitions, uint32_t hostPartitions, NEO::GraphicsAllocation *deviceCounterAllocation,
                    NEO::GraphicsAllocation *hostCounterAllocation, uint64_t baseDeviceAddress, uint64_t baseHostGpuAddress, uint64_t *baseHostCpuAddress, uint64_t incrementValue, uint64_t aggregatedEventUsageCounter,
                    bool hostStorageDuplicated, bool fromExternalMemory);

    const SharableEventDataHelper &getSharableEventDataHelper() const { return sharableEventDataHelper; }

    void set2WayIpcSharingEnabled(bool enabled) { twoWayIpcSharing = enabled; }
    bool is2WayIpcSharingEnabled() const { return twoWayIpcSharing; }

    void setLatestImported2WayIpcData(uint64_t deviceCounterHandle, size_t deviceCounterOffset, unsigned int exporterPid) {
        imported2WayDeviceCounterHandle = deviceCounterHandle;
        imported2WayCounterOffset = deviceCounterOffset;
        imported2WayExportedPid = exporterPid;
    }

    bool is2WayIpcImportRefreshNeeded() const;

    void assignAllocationsFromImport(NEO::MemoryManager &memoryManager, NEO::GraphicsAllocation &deviceAlloc, NEO::GraphicsAllocation &hostAlloc);

  protected:
    void assignInOrderExecInfo(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo);
    void moveTimestampNodesToReleaseList(std::vector<NEO::TagNodeBase *> &nodes);

    SharableEventDataHelper sharableEventDataHelper = {};
    std::shared_ptr<InOrderExecInfo> inOrderExecInfo;
    std::vector<NEO::TagNodeBase *> timestampNodes;
    std::vector<NEO::TagNodeBase *> additionalTimestampNodes;

    NEO::GraphicsAllocation *deviceCounterAllocation = nullptr;
    NEO::GraphicsAllocation *hostCounterAllocation = nullptr;

    uint64_t *baseHostCpuAddress = nullptr;
    uint64_t baseHostGpuAddress = 0;
    uint64_t baseDeviceAddress = 0;
    uint64_t incrementValue = 0;
    uint64_t aggregatedEventUsageCounter = 0;
    uint64_t imported2WayDeviceCounterHandle = 0;
    size_t imported2WayCounterOffset = 0;
    unsigned int imported2WayExportedPid = 0;
    bool twoWayIpcSharing = false;
    bool hostStorageDuplicated = false;
    bool fromExternalMemory = false;
    bool dataAssigned = false;
};

namespace InOrderProgrammingHelpers {
inline bool isLriFor64bDataProgrammingRequired(bool qwordInOrderCounter, bool useSemaphore64bCmd) {
    return qwordInOrderCounter && !useSemaphore64bCmd;
}
} // namespace InOrderProgrammingHelpers

} // namespace NEO
