/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/timestamp_packet_constants.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/os_interface/os_time.h"

#include <level_zero/ze_api.h>

#include <atomic>
#include <bitset>
#include <chrono>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

struct _ze_event_handle_t {};

struct _ze_event_pool_handle_t {};

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class MultiGraphicsAllocation;
struct RootDeviceEnvironment;
} // namespace NEO

namespace L0 {
typedef uint64_t FlushStamp;
struct EventPool;
struct MetricStreamer;
struct ContextImp;
struct Context;
struct CommandQueue;
struct DriverHandle;
struct DriverHandleImp;
struct Device;
struct Kernel;
struct InOrderExecInfo;

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
};
#pragma pack()
static_assert(sizeof(IpcEventPoolData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcEventPoolData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

namespace EventPacketsCount {
inline constexpr uint32_t maxKernelSplit = 3;
inline constexpr uint32_t eventPackets = maxKernelSplit * NEO ::TimestampPacketConstants::preferredPacketCount;
} // namespace EventPacketsCount

struct Event : _ze_event_handle_t {
    virtual ~Event() = default;
    virtual ze_result_t destroy();
    virtual ze_result_t hostSignal() = 0;
    virtual ze_result_t hostSynchronize(uint64_t timeout) = 0;
    virtual ze_result_t queryStatus() = 0;
    virtual ze_result_t reset() = 0;
    virtual ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) = 0;
    virtual ze_result_t queryTimestampsExp(Device *device, uint32_t *count, ze_kernel_timestamp_result_t *timestamps) = 0;
    virtual ze_result_t queryKernelTimestampsExt(Device *device, uint32_t *pCount, ze_event_query_kernel_timestamps_results_ext_properties_t *pResults) = 0;

    enum State : uint32_t {
        STATE_SIGNALED = 0u,
        HOST_CACHING_DISABLED_PERMANENT = std::numeric_limits<uint32_t>::max() - 2,
        HOST_CACHING_DISABLED = std::numeric_limits<uint32_t>::max() - 1,
        STATE_CLEARED = std::numeric_limits<uint32_t>::max(),
        STATE_INITIAL = STATE_CLEARED
    };

    enum class CounterBasedMode : uint32_t {
        // For default flow (API)
        InitiallyDisabled,
        ExplicitlyEnabled,
        // For internal convertion (Immediate CL)
        ImplicitlyEnabled,
        ImplicitlyDisabled
    };

    template <typename TagSizeT>
    static Event *create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device);

    static Event *fromHandle(ze_event_handle_t handle) { return static_cast<Event *>(handle); }

    inline ze_event_handle_t toHandle() { return this; }

    MOCKABLE_VIRTUAL NEO::GraphicsAllocation &getAllocation(Device *device) const;

    MOCKABLE_VIRTUAL uint64_t getGpuAddress(Device *device) const;
    virtual uint32_t getPacketsInUse() const = 0;
    virtual uint32_t getPacketsUsedInLastKernel() = 0;
    virtual uint64_t getPacketAddress(Device *device) = 0;
    MOCKABLE_VIRTUAL void resetPackets(bool resetAllPackets);
    virtual void resetKernelCountAndPacketUsedCount() = 0;
    void *getHostAddress() const { return hostAddress; }
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
    void setMetricStreamer(MetricStreamer *metricStreamer) {
        this->metricStreamer = metricStreamer;
    }
    void updateInOrderExecState(std::shared_ptr<InOrderExecInfo> &newInOrderExecInfo, uint64_t signalValue, uint32_t allocationOffset);
    bool isCounterBased() const { return ((counterBasedMode == CounterBasedMode::ExplicitlyEnabled) || (counterBasedMode == CounterBasedMode::ImplicitlyEnabled)); }
    bool isCounterBasedExplicitlyEnabled() const { return (counterBasedMode == CounterBasedMode::ExplicitlyEnabled); }
    void enableCounterBasedMode(bool apiRequest);
    void disableImplicitCounterBasedMode();
    NEO::GraphicsAllocation *getInOrderExecDataAllocation() const;
    uint64_t getInOrderExecSignalValueWithSubmissionCounter() const;
    uint64_t getInOrderExecBaseSignalValue() const { return inOrderExecSignalValue; }
    uint32_t getInOrderAllocationOffset() const { return inOrderAllocationOffset; }
    void setLatestUsedCmdQueue(CommandQueue *newCmdQ);
    NEO::TimeStampData *peekReferenceTs() {
        return &referenceTs;
    }
    void setReferenceTs(uint64_t currentCpuTimeStamp);
    const CommandQueue *getLatestUsedCmdQueue() const { return latestUsedCmdQueue; }
    bool hasKerneMappedTsCapability = false;
    std::shared_ptr<InOrderExecInfo> &getInOrderExecInfo() { return inOrderExecInfo; }
    void enableKmdWaitMode() { kmdWaitMode = true; }
    bool isKmdWaitModeEnabled() const { return kmdWaitMode; }

  protected:
    Event(EventPool *eventPool, int index, Device *device) : device(device), eventPool(eventPool), index(index) {}

    void unsetCmdQueue();

    uint64_t globalStartTS = 1;
    uint64_t globalEndTS = 1;
    uint64_t contextStartTS = 1;
    uint64_t contextEndTS = 1;
    NEO::TimeStampData referenceTs{};

    uint64_t inOrderExecSignalValue = 0;
    uint32_t inOrderAllocationOffset = 0;

    std::chrono::microseconds gpuHangCheckPeriod{500'000};
    std::bitset<EventPacketsCount::maxKernelSplit> l3FlushAppliedOnKernel;

    size_t contextStartOffset = 0u;
    size_t contextEndOffset = 0u;
    size_t globalStartOffset = 0u;
    size_t globalEndOffset = 0u;
    size_t timestampSizeInDw = 0u;
    size_t singlePacketSize = 0u;
    size_t eventPoolOffset = 0u;

    size_t cpuStartTimestamp = 0u;
    size_t gpuStartTimestamp = 0u;
    size_t gpuEndTimestamp = 0u;

    // Metric streamer instance associated with the event.
    MetricStreamer *metricStreamer = nullptr;
    StackVec<NEO::CommandStreamReceiver *, 1> csrs;
    void *hostAddress = nullptr;
    Device *device = nullptr;
    EventPool *eventPool = nullptr;
    std::weak_ptr<Kernel> kernelWithPrintf = std::weak_ptr<Kernel>{};
    std::mutex *kernelWithPrintfDeviceMutex = nullptr;
    std::shared_ptr<InOrderExecInfo> inOrderExecInfo;
    CommandQueue *latestUsedCmdQueue = nullptr;

    uint32_t maxKernelCount = 0;
    uint32_t kernelCount = 1u;
    uint32_t maxPacketCount = 0;
    uint32_t totalEventSize = 0;
    CounterBasedMode counterBasedMode = CounterBasedMode::InitiallyDisabled;

    ze_event_scope_flags_t signalScope = 0u;
    ze_event_scope_flags_t waitScope = 0u;

    int index = 0;

    std::atomic<State> isCompleted{STATE_INITIAL};

    bool isTimestampEvent = false;
    bool usingContextEndOffset = false;
    bool signalAllEventPackets = false;
    bool isFromIpcPool = false;
    bool kmdWaitMode = false;
    uint64_t timestampRefreshIntervalInNanoSec = 0;
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

    static EventPool *fromHandle(ze_event_pool_handle_t handle) {
        return static_cast<EventPool *>(handle);
    }

    inline ze_event_pool_handle_t toHandle() { return this; }

    MOCKABLE_VIRTUAL NEO::MultiGraphicsAllocation &getAllocation() { return *eventPoolAllocations; }

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

    bool isEventPoolKerneMappedTsFlagSet() const {
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

    bool isCounterBased() const { return counterBased; }
    bool isIpcPoolFlagSet() const { return isIpcPoolFlag; }

  protected:
    EventPool() = default;
    EventPool(size_t numEvents) : numEvents(numEvents) {}
    void setupDescriptorFlags(const ze_event_pool_desc_t *desc);

    std::vector<Device *> devices;

    std::unique_ptr<NEO::MultiGraphicsAllocation> eventPoolAllocations;
    void *eventPoolPtr = nullptr;
    ContextImp *context = nullptr;

    size_t numEvents = 1;
    size_t eventPoolSize = 0;

    uint32_t eventAlignment = 0;
    uint32_t eventSize = 0;
    uint32_t eventPackets = 0;
    uint32_t maxKernelCount = 0;

    ze_event_pool_flags_t eventPoolFlags{};

    bool isDeviceEventPoolAllocation = false;
    bool isHostVisibleEventPoolAllocation = false;
    bool isImportedIpcPool = false;
    bool isIpcPoolFlag = false;
    bool isShareableEventMemory = false;
    bool isImplicitScalingCapable = false;
    bool counterBased = false;
};

} // namespace L0
