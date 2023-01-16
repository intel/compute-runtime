/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/timestamp_packet.h"

#include <level_zero/ze_api.h>

#include <bitset>
#include <limits>

struct _ze_event_handle_t {};

struct _ze_event_pool_handle_t {};

namespace NEO {
struct RootDeviceEnvironment;
}

namespace L0 {
typedef uint64_t FlushStamp;
struct EventPool;
struct MetricStreamer;
struct ContextImp;
struct Context;
struct DriverHandle;
struct DriverHandleImp;
struct Device;

#pragma pack(1)
struct IpcEventPoolData {
    uint64_t handle = 0;
    size_t numEvents = 0;
    uint32_t rootDeviceIndex = 0;
    bool isDeviceEventPoolAllocation = false;
    bool isHostVisibleEventPoolAllocation = false;
};
#pragma pack()
static_assert(sizeof(IpcEventPoolData) <= ZE_MAX_IPC_HANDLE_SIZE, "IpcEventPoolData is bigger than ZE_MAX_IPC_HANDLE_SIZE");

namespace EventPacketsCount {
inline constexpr uint32_t maxKernelSplit = 3;
inline constexpr uint32_t eventPackets = maxKernelSplit * NEO ::TimestampPacketSizeControl::preferredPacketCount;
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
    enum State : uint32_t {
        STATE_SIGNALED = 0u,
        HOST_CACHING_DISABLED_PERMANENT = std::numeric_limits<uint32_t>::max() - 2,
        HOST_CACHING_DISABLED = std::numeric_limits<uint32_t>::max() - 1,
        STATE_CLEARED = std::numeric_limits<uint32_t>::max(),
        STATE_INITIAL = STATE_CLEARED
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
    void *getCompletionFieldHostAddress() const {
        return ptrOffset(getHostAddress(), getCompletionFieldOffset());
    }
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
    void setCsr(NEO::CommandStreamReceiver *csr) {
        this->csr = csr;
    }

    void increaseKernelCount() {
        kernelCount++;
        UNRECOVERABLE_IF(kernelCount > maxKernelCount);
    }
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

    void setIsCompleted() {
        if (this->isCompleted.load() == STATE_CLEARED) {
            this->isCompleted = STATE_SIGNALED;
        }
    }

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

    uint64_t globalStartTS = 1;
    uint64_t globalEndTS = 1;
    uint64_t contextStartTS = 1;
    uint64_t contextEndTS = 1;
    std::chrono::microseconds gpuHangCheckPeriod{500'000};

    // Metric streamer instance associated with the event.
    MetricStreamer *metricStreamer = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    void *hostAddress = nullptr;
    Device *device = nullptr;
    EventPool *eventPool = nullptr;

    ze_event_scope_flags_t signalScope = 0u;
    ze_event_scope_flags_t waitScope = 0u;

    int index = 0;

  protected:
    Event(EventPool *eventPool, int index, Device *device) : device(device), eventPool(eventPool), index(index) {}
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

    uint32_t maxKernelCount = 0;
    uint32_t kernelCount = 1u;
    uint32_t maxPacketCount = 0;
    uint32_t totalEventSize = 0;

    std::atomic<State> isCompleted{STATE_INITIAL};

    bool isTimestampEvent = false;
    bool usingContextEndOffset = false;
    bool signalAllEventPackets = false;
};

template <typename TagSizeT>
class KernelEventCompletionData : public NEO::TimestampPackets<TagSizeT> {
  public:
    uint32_t getPacketsUsed() const { return packetsUsed; }
    void setPacketsUsed(uint32_t value) { packetsUsed = value; }

  protected:
    uint32_t packetsUsed = 1;
};

template <typename TagSizeT>
struct EventImp : public Event {

    EventImp(EventPool *eventPool, int index, Device *device, bool downloadAllocationRequired)
        : Event(eventPool, index, device), downloadAllocationRequired(downloadAllocationRequired) {
        contextStartOffset = NEO::TimestampPackets<TagSizeT>::getContextStartOffset();
        contextEndOffset = NEO::TimestampPackets<TagSizeT>::getContextEndOffset();
        globalStartOffset = NEO::TimestampPackets<TagSizeT>::getGlobalStartOffset();
        globalEndOffset = NEO::TimestampPackets<TagSizeT>::getGlobalEndOffset();
        timestampSizeInDw = (sizeof(TagSizeT) / sizeof(uint32_t));
        singlePacketSize = NEO::TimestampPackets<TagSizeT>::getSinglePacketSize();
    }

    ~EventImp() override {}

    ze_result_t hostSignal() override;

    ze_result_t hostSynchronize(uint64_t timeout) override;

    ze_result_t queryStatus() override;

    ze_result_t reset() override;

    ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) override;
    ze_result_t queryTimestampsExp(Device *device, uint32_t *count, ze_kernel_timestamp_result_t *timestamps) override;

    void resetDeviceCompletionData(bool resetAllPackets);
    void resetKernelCountAndPacketUsedCount() override;

    uint64_t getPacketAddress(Device *device) override;
    uint32_t getPacketsInUse() const override;
    uint32_t getPacketsUsedInLastKernel() override;
    void setPacketsInUse(uint32_t value) override;

    std::unique_ptr<KernelEventCompletionData<TagSizeT>[]> kernelEventCompletionData;

    const bool downloadAllocationRequired = false;

  protected:
    ze_result_t calculateProfilingData();
    ze_result_t queryStatusEventPackets();
    MOCKABLE_VIRTUAL ze_result_t hostEventSetValue(TagSizeT eventValue);
    ze_result_t hostEventSetValueTimestamps(TagSizeT eventVal);
    MOCKABLE_VIRTUAL void assignKernelEventCompletionData(void *address);
    void setRemainingPackets(TagSizeT eventVal, void *nextPacketAddress, uint32_t packetsAlreadySet);
};

struct EventPool : _ze_event_pool_handle_t {
    static EventPool *create(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *deviceHandles, const ze_event_pool_desc_t *desc, ze_result_t &result);
    EventPool(const ze_event_pool_desc_t *desc) : EventPool(desc->count) {
        eventPoolFlags = desc->flags;
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

    uint32_t getMaxKernelCount() const {
        return maxKernelCount;
    }

    ze_result_t initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *deviceHandles);

    void initializeSizeParameters(uint32_t numDevices, ze_device_handle_t *deviceHandles, DriverHandleImp &driver, const NEO::RootDeviceEnvironment &rootDeviceEnvironment);

    Device *getDevice() const { return devices[0]; }

    std::vector<Device *> devices;
    std::unique_ptr<NEO::MultiGraphicsAllocation> eventPoolAllocations;
    void *eventPoolPtr = nullptr;
    ContextImp *context = nullptr;
    ze_event_pool_flags_t eventPoolFlags;
    bool isDeviceEventPoolAllocation = false;
    bool isHostVisibleEventPoolAllocation = false;
    bool isImportedIpcPool = false;
    bool isShareableEventMemory = false;

  protected:
    EventPool() = default;
    EventPool(size_t numEvents) : numEvents(numEvents) {}

    size_t numEvents = 1;
    size_t eventPoolSize = 0;
    uint32_t eventAlignment = 0;
    uint32_t eventSize = 0;
    uint32_t eventPackets = 0;
    uint32_t maxKernelCount = 0;
};

} // namespace L0
