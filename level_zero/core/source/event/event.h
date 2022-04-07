/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/timestamp_packet.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include <chrono>
#include <limits>

struct _ze_event_handle_t {};

struct _ze_event_pool_handle_t {};

namespace L0 {
typedef uint64_t FlushStamp;
struct EventPool;
struct MetricStreamer;

namespace EventPacketsCount {
constexpr uint32_t maxKernelSplit = 3;
constexpr uint32_t eventPackets = maxKernelSplit * NEO ::TimestampPacketSizeControl::preferredPacketCount;
} // namespace EventPacketsCount

struct Event : _ze_event_handle_t {
    virtual ~Event() = default;
    virtual ze_result_t destroy();
    virtual ze_result_t hostSignal() = 0;
    virtual ze_result_t hostSynchronize(uint64_t timeout) = 0;
    virtual ze_result_t queryStatus() = 0;
    virtual ze_result_t reset() = 0;
    virtual ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) = 0;
    virtual ze_result_t queryTimestampsExp(Device *device, uint32_t *pCount, ze_kernel_timestamp_result_t *pTimestamps) = 0;
    enum State : uint32_t {
        STATE_SIGNALED = 0u,
        STATE_CLEARED = std::numeric_limits<uint32_t>::max(),
        STATE_INITIAL = STATE_CLEARED
    };

    template <typename TagSizeT>
    static Event *create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device);

    static Event *fromHandle(ze_event_handle_t handle) { return static_cast<Event *>(handle); }

    inline ze_event_handle_t toHandle() { return this; }

    virtual NEO::GraphicsAllocation &getAllocation(Device *device) = 0;

    virtual uint64_t getGpuAddress(Device *device) = 0;
    virtual uint32_t getPacketsInUse() = 0;
    virtual uint64_t getPacketAddress(Device *device) = 0;
    virtual void resetPackets() = 0;
    void *getHostAddress() { return hostAddress; }
    virtual void setPacketsInUse(uint32_t value) = 0;
    uint32_t getCurrKernelDataIndex() const { return kernelCount - 1; }

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
    void setPartitionedEvent(bool partitionedEvent) {
        this->partitionedEvent = partitionedEvent;
    }
    bool isPartitionedEvent() const {
        return partitionedEvent;
    }
    bool useContextEndOffset() const {
        return isTimestampEvent || partitionedEvent;
    }

    uint64_t globalStartTS;
    uint64_t globalEndTS;
    uint64_t contextStartTS;
    uint64_t contextEndTS;
    std::chrono::microseconds gpuHangCheckPeriod{500'000};

    // Metric streamer instance associated with the event.
    MetricStreamer *metricStreamer = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    void *hostAddress = nullptr;

    ze_event_scope_flags_t signalScope = 0u;
    ze_event_scope_flags_t waitScope = 0u;

    uint32_t kernelCount = 1u;

    bool l3FlushWaApplied = false;

  protected:
    size_t contextStartOffset = 0u;
    size_t contextEndOffset = 0u;
    size_t globalStartOffset = 0u;
    size_t globalEndOffset = 0u;
    size_t timestampSizeInDw = 0u;
    size_t singlePacketSize = 0u;
    bool isTimestampEvent = false;
    bool partitionedEvent = false;
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

    EventImp(EventPool *eventPool, int index, Device *device)
        : device(device), index(index), eventPool(eventPool) {
        contextStartOffset = NEO::TimestampPackets<TagSizeT>::getContextStartOffset();
        contextEndOffset = NEO::TimestampPackets<TagSizeT>::getContextEndOffset();
        globalStartOffset = NEO::TimestampPackets<TagSizeT>::getGlobalStartOffset();
        globalEndOffset = NEO::TimestampPackets<TagSizeT>::getGlobalEndOffset();
        timestampSizeInDw = (sizeof(TagSizeT) / 4);
        singlePacketSize = NEO::TimestampPackets<TagSizeT>::getSinglePacketSize();
    }

    ~EventImp() override {}

    ze_result_t hostSignal() override;

    ze_result_t hostSynchronize(uint64_t timeout) override;

    ze_result_t queryStatus() override;

    ze_result_t reset() override;

    ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) override;
    ze_result_t queryTimestampsExp(Device *device, uint32_t *pCount, ze_kernel_timestamp_result_t *pTimestamps) override;

    NEO::GraphicsAllocation &getAllocation(Device *device) override;

    uint64_t getGpuAddress(Device *device) override;

    void resetPackets() override;
    uint64_t getPacketAddress(Device *device) override;
    uint32_t getPacketsInUse() override;
    void setPacketsInUse(uint32_t value) override;

    std::unique_ptr<KernelEventCompletionData<TagSizeT>[]> kernelEventCompletionData;

    Device *device;
    int index;
    EventPool *eventPool;

  protected:
    ze_result_t calculateProfilingData();
    ze_result_t queryStatusKernelTimestamp();
    ze_result_t queryStatusNonTimestamp();
    ze_result_t hostEventSetValue(TagSizeT eventValue);
    ze_result_t hostEventSetValueTimestamps(TagSizeT eventVal);
    void assignKernelEventCompletionData(void *address);
};

struct EventPool : _ze_event_pool_handle_t {
    static EventPool *create(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *phDevices, const ze_event_pool_desc_t *desc, ze_result_t &result);
    virtual ~EventPool() = default;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) = 0;
    virtual ze_result_t closeIpcHandle() = 0;
    virtual ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) = 0;
    virtual Device *getDevice() = 0;

    static EventPool *fromHandle(ze_event_pool_handle_t handle) {
        return static_cast<EventPool *>(handle);
    }

    inline ze_event_pool_handle_t toHandle() { return this; }

    virtual NEO::MultiGraphicsAllocation &getAllocation() { return *eventPoolAllocations; }

    virtual uint32_t getEventSize() = 0;
    virtual void setEventSize(uint32_t) = 0;
    virtual void setEventAlignment(uint32_t) = 0;

    bool isEventPoolTimestampFlagSet() {
        if (NEO::DebugManager.flags.OverrideTimestampEvents.get() != -1) {
            auto timestampOverride = !!NEO::DebugManager.flags.OverrideTimestampEvents.get();
            return timestampOverride;
        }
        if (eventPoolFlags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
            return true;
        }
        return false;
    }

    bool isEventPoolDeviceAllocationFlagSet() {
        if (!(eventPoolFlags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE)) {
            return true;
        }
        return false;
    }

    std::unique_ptr<NEO::MultiGraphicsAllocation> eventPoolAllocations;
    ze_event_pool_flags_t eventPoolFlags;
};

struct EventPoolImp : public EventPool {
    EventPoolImp(const ze_event_pool_desc_t *desc) : numEvents(desc->count) {
        eventPoolFlags = desc->flags;
    }

    ze_result_t initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *phDevices);

    ~EventPoolImp();

    ze_result_t destroy() override;

    ze_result_t getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) override;

    ze_result_t closeIpcHandle() override;

    ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) override;

    uint32_t getEventSize() override { return eventSize; }
    void setEventSize(uint32_t size) override { eventSize = size; }
    void setEventAlignment(uint32_t alignment) override { eventAlignment = alignment; }
    size_t getNumEvents() { return numEvents; }

    Device *getDevice() override { return devices[0]; }

    void *eventPoolPtr = nullptr;
    std::vector<Device *> devices;
    ContextImp *context = nullptr;
    size_t numEvents;
    bool isImportedIpcPool = false;

  protected:
    uint32_t eventAlignment = 0;
    uint32_t eventSize = 0;
};

} // namespace L0
