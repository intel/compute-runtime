/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/timestamp_packet_constants.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/timestamp_pool_allocator.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <atomic>
#include <bitset>
#include <chrono>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

struct _ze_event_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_EVENT> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_event_handle_t>);

struct _ze_event_pool_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_event_pool_handle_t>);

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class MultiGraphicsAllocation;
struct RootDeviceEnvironment;
class InOrderExecInfo;
} // namespace NEO

namespace L0 {
typedef uint64_t FlushStamp;
struct EventPool;
struct MetricCollectorEventNotify;
struct ContextImp;
struct Context;
struct CommandQueue;
struct DriverHandle;
struct DriverHandleImp;
struct Device;
struct Kernel;

#pragma pack(1)
struct IpcEventPoolData {
    uint64_t handle = 0;
    size_t numEvents = 0;
    uint32_t rootDeviceIndex = 0;
    uint32_t maxEventPackets = 0;
    uint32_t numDevices = 0;
    bool isDeviceEventPoolAllocation = false;
    bool isHostVisibleEventPoolAllocation = false;
    bool isImplicitScalingCapable = false;
    bool isEventPoolKernelMappedTsFlagSet = false;
    bool isEventPoolTsFlagSet = false;
};

struct IpcCounterBasedEventData {
    uint64_t deviceHandle = 0;
    uint64_t hostHandle = 0;
    uint64_t counterValue = 0;
    uint32_t rootDeviceIndex = 0;
    uint32_t counterOffset = 0;
    uint32_t devicePartitions = 0;
    uint32_t hostPartitions = 0;
    uint32_t counterBasedFlags = 0;
    uint32_t signalScopeFlags = 0;
    uint32_t waitScopeFlags = 0;
};
#pragma pack()
static_assert(sizeof(IpcEventPoolData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcEventPoolData is bigger than ZE_MAX_IPC_HANDLE_SIZE");
static_assert(sizeof(IpcCounterBasedEventData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcCounterBasedEventData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

namespace EventPacketsCount {
inline constexpr uint32_t maxKernelSplit = 3;
inline constexpr uint32_t eventPackets = maxKernelSplit * NEO ::TimestampPacketConstants::preferredPacketCount;
} // namespace EventPacketsCount

struct EventDescriptor {
    NEO::MultiGraphicsAllocation *eventPoolAllocation = nullptr;
    const void *extensions = nullptr;
    size_t offsetInSharedAlloc = 0;
    uint32_t totalEventSize = 0;
    uint32_t maxKernelCount = 0;
    uint32_t maxPacketsCount = 0;
    uint32_t counterBasedFlags = 0;
    uint32_t index = 0;
    uint32_t signalScope = 0;
    uint32_t waitScope = 0;
    bool timestampPool = false;
    bool kernelMappedTsPoolFlag = false;
    bool importedIpcPool = false;
    bool ipcPool = false;
};

struct Event : _ze_event_handle_t {
    virtual ~Event() = default;
    virtual ze_result_t destroy();
    virtual ze_result_t hostSignal(bool allowCounterBased) = 0;
    virtual ze_result_t hostSynchronize(uint64_t timeout) = 0;
    virtual ze_result_t queryStatus() = 0;
    virtual ze_result_t reset() = 0;
    virtual ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) = 0;
    virtual ze_result_t queryTimestampsExp(Device *device, uint32_t *count, ze_kernel_timestamp_result_t *timestamps) = 0;
    virtual ze_result_t queryKernelTimestampsExt(Device *device, uint32_t *pCount, ze_event_query_kernel_timestamps_results_ext_properties_t *pResults) = 0;
    virtual ze_result_t getEventPool(ze_event_pool_handle_t *phEventPool) = 0;
    virtual ze_result_t getSignalScope(ze_event_scope_flags_t *pSignalScope) = 0;
    virtual ze_result_t getWaitScope(ze_event_scope_flags_t *pWaitScope) = 0;

    enum State : uint32_t {
        STATE_SIGNALED = 2u,
        HOST_CACHING_DISABLED_PERMANENT = std::numeric_limits<uint32_t>::max() - 2,
        HOST_CACHING_DISABLED = std::numeric_limits<uint32_t>::max() - 1,
        STATE_CLEARED = std::numeric_limits<uint32_t>::max(),
        STATE_INITIAL = STATE_CLEARED
    };

    enum class CounterBasedMode : uint32_t {
        // For default flow (API)
        initiallyDisabled,
        explicitlyEnabled,
        // For internal convertion (Immediate CL)
        implicitlyEnabled,
        implicitlyDisabled
    };

    template <typename TagSizeT>
    static Event *create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device);

    template <typename TagSizeT>
    static Event *create(const EventDescriptor &eventDescriptor, Device *device, ze_result_t &result);

    static Event *fromHandle(ze_event_handle_t handle) { return static_cast<Event *>(handle); }

    static ze_result_t openCounterBasedIpcHandle(const IpcCounterBasedEventData &ipcData, ze_event_handle_t *eventHandle,
                                                 DriverHandleImp *driver, ContextImp *context, uint32_t numDevices, ze_device_handle_t *deviceHandles);

    ze_result_t getCounterBasedIpcHandle(IpcCounterBasedEventData &ipcData);

    inline ze_event_handle_t toHandle() { return this; }

    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *getAllocation(Device *device) const;

    void setEventPool(EventPool *eventPool) { this->eventPool = eventPool; }
    EventPool *peekEventPool() { return this->eventPool; }

    MOCKABLE_VIRTUAL uint64_t getGpuAddress(Device *device) const;
    virtual uint32_t getPacketsInUse() const = 0;
    virtual uint32_t getPacketsUsedInLastKernel() = 0;
    virtual uint64_t getPacketAddress(Device *device) = 0;
    MOCKABLE_VIRTUAL void resetPackets(bool resetAllPackets);
    virtual void resetKernelCountAndPacketUsedCount() = 0;
    void *getHostAddress() const;
    virtual void setPacketsInUse(uint32_t value) = 0;
    uint32_t getCurrKernelDataIndex() const { return kernelCount - 1; }
    MOCKABLE_VIRTUAL void setGpuStartTimestamp();
    MOCKABLE_VIRTUAL void setGpuEndTimestamp();
    size_t getCompletionFieldOffset() const {
        return this->isUsingContextEndOffset() ? this->getContextEndOffset() : 0;
    }
    uint64_t getCompletionFieldGpuAddress(Device *device) const {
        return this->getGpuAddress(device) + getCompletionFieldOffset();
    }
    void *getCompletionFieldHostAddress() const;
    size_t getContextStartOffset() const {
        return contextStartOffset;
    }
    size_t getContextEndOffset() const {
        return contextEndOffset;
    }
    size_t getGlobalStartOffset() const {
        return globalStartOffset;
    }
    size_t getGlobalEndOffset() const {
        return globalEndOffset;
    }
    size_t getSinglePacketSize() const {
        return singlePacketSize;
    }
    void setSinglePacketSize(size_t size) {
        singlePacketSize = size;
    }
    size_t getTimestampSizeInDw() const {
        return timestampSizeInDw;
    }
    void setEventTimestampFlag(bool timestampFlag) {
        isTimestampEvent = timestampFlag;
    }
    bool isEventTimestampFlagSet() const {
        return isTimestampEvent;
    }
    void setUsingContextEndOffset(bool usingContextEndOffset) {
        this->usingContextEndOffset = usingContextEndOffset;
    }
    bool isUsingContextEndOffset() const {
        return isTimestampEvent || usingContextEndOffset;
    }
    void setCsr(NEO::CommandStreamReceiver *csr, bool clearPreviousCsrs) {
        if (clearPreviousCsrs) {
            this->csrs.clear();
            this->csrs.resize(1);
        }
        this->csrs[0] = csr;
    }
    void appendAdditionalCsr(NEO::CommandStreamReceiver *additonalCsr) {
        for (const auto &csr : csrs) {
            if (csr == additonalCsr) {
                return;
            }
        }
        csrs.push_back(additonalCsr);
    }

    void increaseKernelCount();
    uint32_t getKernelCount() const {
        return kernelCount;
    }
    void zeroKernelCount() {
        kernelCount = 0;
    }
    void setKernelCount(uint32_t newKernelCount) {
        kernelCount = newKernelCount;
    }
    bool getL3FlushForCurrenKernel() {
        return l3FlushAppliedOnKernel.test(kernelCount - 1);
    }
    void setL3FlushForCurrentKernel() {
        l3FlushAppliedOnKernel.set(kernelCount - 1);
    }

    void resetCompletionStatus() {
        if (this->isCompleted.load() != HOST_CACHING_DISABLED_PERMANENT) {
            this->isCompleted.store(STATE_CLEARED);
        }
    }

    void disableHostCaching(bool disableFromRegularList) {
        this->isCompleted.store(disableFromRegularList ? HOST_CACHING_DISABLED_PERMANENT : HOST_CACHING_DISABLED);
    }

    void setIsCompleted();

    bool isAlreadyCompleted() {
        return this->isCompleted == STATE_SIGNALED;
    }

    uint32_t getMaxPacketsCount() const {
        return maxPacketCount;
    }
    void setMaxKernelCount(uint32_t value) {
        maxKernelCount = value;
    }
    uint32_t getMaxKernelCount() const {
        return maxKernelCount;
    }
    void setKernelForPrintf(std::weak_ptr<Kernel> inputKernelWeakPtr) {
        kernelWithPrintf = inputKernelWeakPtr;
    }
    std::weak_ptr<Kernel> getKernelForPrintf() {
        return kernelWithPrintf;
    }
    void resetKernelForPrintf() {
        kernelWithPrintf.reset();
    }
    void setKernelWithPrintfDeviceMutex(std::mutex *mutexPtr) {
        kernelWithPrintfDeviceMutex = mutexPtr;
    }
    std::mutex *getKernelWithPrintfDeviceMutex() {
        return kernelWithPrintfDeviceMutex;
    }
    void resetKernelWithPrintfDeviceMutex() {
        kernelWithPrintfDeviceMutex = nullptr;
    }

    bool isSignalScope() const {
        return !!signalScope;
    }
    bool isSignalScope(ze_event_scope_flags_t flag) const {
        return !!(signalScope & flag);
    }
    bool isWaitScope() const {
        return !!waitScope;
    }
    bool isWaitScope(ze_event_scope_flags_t flag) const {
        return !!(waitScope & flag);
    }
    void setMetricNotification(MetricCollectorEventNotify *metricNotification) {
        this->metricNotification = metricNotification;
    }
    void updateInOrderExecState(std::shared_ptr<NEO::InOrderExecInfo> &newInOrderExecInfo, uint64_t signalValue, uint32_t allocationOffset);
    bool isCounterBased() const { return ((counterBasedMode == CounterBasedMode::explicitlyEnabled) || (counterBasedMode == CounterBasedMode::implicitlyEnabled)); }
    bool isCounterBasedExplicitlyEnabled() const { return (counterBasedMode == CounterBasedMode::explicitlyEnabled); }
    void enableCounterBasedMode(bool apiRequest, uint32_t flags);
    void disableImplicitCounterBasedMode();
    uint64_t getInOrderExecSignalValueWithSubmissionCounter() const;
    uint64_t getInOrderExecBaseSignalValue() const { return inOrderExecSignalValue; }
    uint32_t getInOrderAllocationOffset() const { return inOrderAllocationOffset; }
    uint64_t getInOrderIncrementValue() const { return inOrderIncrementValue; }
    void setLatestUsedCmdQueue(CommandQueue *newCmdQ);
    NEO::TimeStampData *peekReferenceTs() {
        return static_cast<NEO::TimeStampData *>(ptrOffset(getHostAddress(), getMaxPacketsCount() * getSinglePacketSize()));
    }
    void setReferenceTs(uint64_t currentCpuTimeStamp);
    const CommandQueue *getLatestUsedCmdQueue() const { return latestUsedCmdQueue; }
    bool hasKernelMappedTsCapability = false;
    std::shared_ptr<NEO::InOrderExecInfo> &getInOrderExecInfo() { return inOrderExecInfo; }
    void enableKmdWaitMode() { kmdWaitMode = true; }
    void enableInterruptMode() { interruptMode = true; }
    bool isKmdWaitModeEnabled() const { return kmdWaitMode; }
    bool isInterruptModeEnabled() const { return interruptMode; }
    void unsetInOrderExecInfo();
    uint32_t getCounterBasedFlags() const { return counterBasedFlags; }

    uint32_t getPacketsToWait() const {
        return this->signalAllEventPackets ? getMaxPacketsCount() : getPacketsInUse();
    }

    void setExternalInterruptId(uint32_t interruptId) { externalInterruptId = interruptId; }

    void resetInOrderTimestampNode(NEO::TagNodeBase *newNode, uint32_t partitionCount);
    void resetAdditionalTimestampNode(NEO::TagNodeBase *newNode, uint32_t partitionCount, bool resetAggregatedEvent);

    bool hasInOrderTimestampNode() const { return !inOrderTimestampNode.empty(); }

    bool isIpcImported() const { return isFromIpcPool; }

    void setMitigateHostVisibleSignal() {
        this->mitigateHostVisibleSignal = true;
    }

    virtual ze_result_t hostEventSetValue(State eventState) = 0;

    size_t getOffsetInSharedAlloc() const { return offsetInSharedAlloc; }
    void setReportEmptyCbEventAsReady(bool reportEmptyCbEventAsReady) { this->reportEmptyCbEventAsReady = reportEmptyCbEventAsReady; }

  protected:
    Event(int index, Device *device) : device(device), index(index) {}

    ze_result_t enableExtensions(const EventDescriptor &eventDescriptor);
    NEO::GraphicsAllocation *getExternalCounterAllocationFromAddress(uint64_t *address) const;

    void unsetCmdQueue();
    void releaseTempInOrderTimestampNodes();
    virtual void clearTimestampTagData(uint32_t partitionCount, NEO::TagNodeBase *newNode) = 0;

    EventPool *eventPool = nullptr;

    uint64_t timestampRefreshIntervalInNanoSec = 0;

    uint64_t globalStartTS = 1;
    uint64_t globalEndTS = 1;
    uint64_t contextStartTS = 1;
    uint64_t contextEndTS = 1;

    uint64_t inOrderExecSignalValue = 0;
    uint64_t inOrderIncrementValue = 0;
    uint32_t inOrderAllocationOffset = 0;

    std::chrono::microseconds gpuHangCheckPeriod{CommonConstants::gpuHangCheckTimeInUS};
    std::bitset<EventPacketsCount::maxKernelSplit> l3FlushAppliedOnKernel;

    size_t contextStartOffset = 0u;
    size_t contextEndOffset = 0u;
    size_t globalStartOffset = 0u;
    size_t globalEndOffset = 0u;
    size_t timestampSizeInDw = 0u;
    size_t singlePacketSize = 0u;
    size_t eventPoolOffset = 0u;
    size_t offsetInSharedAlloc = 0u;

    size_t cpuStartTimestamp = 0u;
    size_t gpuStartTimestamp = 0u;
    size_t gpuEndTimestamp = 0u;

    // Metric instance associated with the event.
    MetricCollectorEventNotify *metricNotification = nullptr;
    NEO::MultiGraphicsAllocation *eventPoolAllocation = nullptr;
    StackVec<NEO::CommandStreamReceiver *, 1> csrs;
    void *hostAddressFromPool = nullptr;
    Device *device = nullptr;
    std::weak_ptr<Kernel> kernelWithPrintf = std::weak_ptr<Kernel>{};
    std::mutex *kernelWithPrintfDeviceMutex = nullptr;
    std::shared_ptr<NEO::InOrderExecInfo> inOrderExecInfo;
    CommandQueue *latestUsedCmdQueue = nullptr;
    std::vector<NEO::TagNodeBase *> inOrderTimestampNode;
    std::vector<NEO::TagNodeBase *> additionalTimestampNode;

    uint32_t maxKernelCount = 0;
    uint32_t kernelCount = 1u;
    uint32_t maxPacketCount = 0;
    uint32_t totalEventSize = 0;
    uint32_t counterBasedFlags = 0;
    uint32_t externalInterruptId = NEO::InterruptId::notUsed;

    CounterBasedMode counterBasedMode = CounterBasedMode::initiallyDisabled;

    ze_event_scope_flags_t signalScope = 0u;
    ze_event_scope_flags_t waitScope = 0u;

    int index = 0;

    std::atomic<State> isCompleted{STATE_INITIAL};

    bool isTimestampEvent = false;
    bool usingContextEndOffset = false;
    bool signalAllEventPackets = false;
    bool isFromIpcPool = false;
    bool kmdWaitMode = false;
    bool interruptMode = false;
    bool isSharableCounterBased = false;
    bool mitigateHostVisibleSignal = false;
    bool reportEmptyCbEventAsReady = true;

    static const uint64_t completionTimeoutMs;
};

struct EventPool : _ze_event_pool_handle_t {
    static EventPool *create(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *deviceHandles, const ze_event_pool_desc_t *desc, ze_result_t &result);
    static ze_result_t openEventPoolIpcHandle(const ze_ipc_event_pool_handle_t &ipcEventPoolHandle, ze_event_pool_handle_t *eventPoolHandle,
                                              DriverHandleImp *driver, ContextImp *context, uint32_t numDevices, ze_device_handle_t *deviceHandles);
    EventPool(const ze_event_pool_desc_t *desc) : EventPool(desc->count) {
        setupDescriptorFlags(desc);
    }
    virtual ~EventPool();
    MOCKABLE_VIRTUAL ze_result_t destroy();
    MOCKABLE_VIRTUAL ze_result_t getIpcHandle(ze_ipc_event_pool_handle_t *ipcHandle);
    MOCKABLE_VIRTUAL ze_result_t closeIpcHandle();
    MOCKABLE_VIRTUAL ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *eventHandle);
    ze_result_t getContextHandle(ze_context_handle_t *phContext);
    ze_result_t getFlags(ze_event_pool_flags_t *pFlags);

    static EventPool *fromHandle(ze_event_pool_handle_t handle) {
        return static_cast<EventPool *>(handle);
    }

    inline ze_event_pool_handle_t toHandle() { return this; }

    MOCKABLE_VIRTUAL NEO::MultiGraphicsAllocation &getAllocation() { return *eventPoolAllocations; }
    std::unique_ptr<NEO::SharedTimestampAllocation> &getSharedTimestampAllocation() {
        return sharedTimestampAllocation;
    }

    uint32_t getEventSize() const { return eventSize; }
    void setEventSize(uint32_t size) { eventSize = size; }
    void setEventAlignment(uint32_t alignment) { eventAlignment = alignment; }
    size_t getNumEvents() const { return numEvents; }
    uint32_t getEventMaxPackets() const { return eventPackets; }
    size_t getEventPoolSize() const { return eventPoolSize; }

    bool isEventPoolTimestampFlagSet() const;

    bool isEventPoolDeviceAllocationFlagSet() const {
        if (!(eventPoolFlags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE)) {
            return true;
        }
        return false;
    }

    bool isEventPoolKernelMappedTsFlagSet() const {
        if (eventPoolFlags & ZE_EVENT_POOL_FLAG_KERNEL_MAPPED_TIMESTAMP) {
            return true;
        }
        return false;
    }

    uint32_t getMaxKernelCount() const {
        return maxKernelCount;
    }

    ze_result_t initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *deviceHandles);

    void initializeSizeParameters(uint32_t numDevices, ze_device_handle_t *deviceHandles, DriverHandleImp &driver, const NEO::RootDeviceEnvironment &rootDeviceEnvironment);

    Device *getDevice() const { return devices[0]; }

    bool getImportedIpcPool() const {
        return isImportedIpcPool;
    }

    bool isImplicitScalingCapableFlagSet() const {
        return isImplicitScalingCapable;
    }

    uint32_t getCounterBasedFlags() const { return counterBasedFlags; }
    bool isIpcPoolFlagSet() const { return isIpcPoolFlag; }

  protected:
    EventPool() = default;
    EventPool(size_t numEvents) : numEvents(numEvents) {}
    void setupDescriptorFlags(const ze_event_pool_desc_t *desc);

    std::vector<Device *> devices;

    std::unique_ptr<NEO::MultiGraphicsAllocation> eventPoolAllocations;
    std::unique_ptr<NEO::SharedTimestampAllocation> sharedTimestampAllocation;

    void *eventPoolPtr = nullptr;
    ContextImp *context = nullptr;

    size_t numEvents = 1;
    size_t eventPoolSize = 0;

    uint32_t eventAlignment = 0;
    uint32_t eventSize = 0;
    uint32_t eventPackets = 0;
    uint32_t maxKernelCount = 0;

    uint32_t counterBasedFlags = 0;

    ze_event_pool_flags_t eventPoolFlags{};

    bool isDeviceEventPoolAllocation = false;
    bool isHostVisibleEventPoolAllocation = false;
    bool isImportedIpcPool = false;
    bool isIpcPoolFlag = false;
    bool isShareableEventMemory = false;
    bool isImplicitScalingCapable = false;
};

} // namespace L0
