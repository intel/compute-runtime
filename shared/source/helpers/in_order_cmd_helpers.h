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
    uint64_t counterValue = 0;
    uint64_t incrementValue = 0;
    uint32_t counterOffset = 0;
    uint32_t devicePartitions = 0;
    uint32_t hostPartitions = 0;
};
#pragma pack()

class InOrderExecEventHelper : public NonCopyableClass {
  public:
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

    const InOrderExecEventData *getEventData() const { return eventData.get(); }

    uint64_t *getBaseHostAddress() const { return baseHostAddress; }
    uint64_t getBaseDeviceAddress() const { return baseDeviceAddress; }

    NEO::GraphicsAllocation *getDeviceCounterAllocation() const { return deviceCounterAllocation; }
    NEO::GraphicsAllocation *getHostCounterAllocation() const { return hostCounterAllocation; }
    bool isHostStorageDuplicated() const { return hostStorageDuplicated; }
    bool isFromExternalMemory() const { return fromExternalMemory; }

    uint64_t getIncrementValue() const { return eventData ? eventData->incrementValue : 0; }

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
                    NEO::GraphicsAllocation *hostCounterAllocation, uint64_t baseDeviceAddress, uint64_t *baseHostAddress, uint64_t incrementValue, uint64_t aggregatedEventUsageCounter,
                    bool hostStorageDuplicated, bool fromExternalMemory);

  protected:
    void assignInOrderExecInfo(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo);
    void moveTimestampNodesToReleaseList(std::vector<NEO::TagNodeBase *> &nodes);

    std::unique_ptr<InOrderExecEventData> eventData;

    std::shared_ptr<InOrderExecInfo> inOrderExecInfo;
    std::vector<NEO::TagNodeBase *> timestampNodes;
    std::vector<NEO::TagNodeBase *> additionalTimestampNodes;

    NEO::GraphicsAllocation *deviceCounterAllocation = nullptr;
    NEO::GraphicsAllocation *hostCounterAllocation = nullptr;
    uint64_t *baseHostAddress = nullptr;
    uint64_t baseDeviceAddress = 0;
    uint64_t aggregatedEventUsageCounter = 0;
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
