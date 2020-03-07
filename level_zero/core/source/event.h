/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/device.h"
#include "level_zero/core/source/driver_handle.h"
#include <level_zero/ze_event.h>

struct _ze_event_handle_t {};

struct _ze_event_pool_handle_t {};

namespace L0 {
typedef uint64_t FlushStamp;
struct EventPool;
struct MetricTracer;

struct Event : _ze_event_handle_t {
    virtual ~Event() = default;
    virtual ze_result_t destroy();
    virtual ze_result_t hostSignal() = 0;
    virtual ze_result_t hostSynchronize(uint32_t timeout) = 0;
    virtual ze_result_t queryStatus() = 0;
    virtual ze_result_t reset() = 0;
    virtual ze_result_t getTimestamp(ze_event_timestamp_type_t timestampType, void *dstptr) = 0;

    enum State : uint64_t {
        STATE_SIGNALED = 0u,
        STATE_CLEARED = static_cast<uint64_t>(-1),
        STATE_INITIAL = STATE_CLEARED
    };

    enum EventTimestampRegister : uint32_t {
        GLOBAL_START_LOW = 0u,
        GLOBAL_START_HIGH,
        GLOBAL_END,
        CONTEXT_START,
        CONTEXT_END
    };

    static Event *create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device);

    static Event *fromHandle(ze_event_handle_t handle) { return static_cast<Event *>(handle); }

    inline ze_event_handle_t toHandle() { return this; }

    NEO::GraphicsAllocation &getAllocation();

    uint64_t getGpuAddress() { return gpuAddress; }
    uint64_t getOffsetOfEventTimestampRegister(uint32_t eventTimestampReg);

    void *hostAddress = nullptr;
    uint64_t gpuAddress;
    int offsetUsed = -1;

    ze_event_scope_flag_t signalScope; // Saving scope for use later
    ze_event_scope_flag_t waitScope;

    bool isTimestampEvent = false;

    // Metric tracer instance associated with the event.
    MetricTracer *metricTracer = nullptr;

  protected:
    NEO::GraphicsAllocation *allocation = nullptr;
};

struct EventPool : _ze_event_pool_handle_t {
    static EventPool *create(Device *device, const ze_event_pool_desc_t *desc);

    virtual ~EventPool() = default;
    virtual ze_result_t destroy() = 0;
    virtual size_t getPoolSize() = 0;
    virtual uint32_t getPoolUsedCount() = 0;
    virtual ze_result_t getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) = 0;
    virtual ze_result_t closeIpcHandle() = 0;
    virtual ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) = 0;
    virtual ze_result_t reserveEventFromPool(int index, Event *event) = 0;
    virtual ze_result_t releaseEventToPool(Event *event) = 0;
    virtual Device *getDevice() = 0;

    enum EventCreationState : int {
        EVENT_STATE_INITIAL = 0,
        EVENT_STATE_DESTROYED = EVENT_STATE_INITIAL,
        EVENT_STATE_CREATED = 1
    };

    static EventPool *fromHandle(ze_event_pool_handle_t handle) {
        return static_cast<EventPool *>(handle);
    }

    inline ze_event_pool_handle_t toHandle() { return this; }

    NEO::GraphicsAllocation &getAllocation() { return *eventPoolAllocation; }

    virtual uint32_t getEventSize() = 0;
    virtual uint32_t getNumEventTimestampsToRead() = 0;

    bool isEventPoolUsedForTimestamp = false;

  protected:
    NEO::GraphicsAllocation *eventPoolAllocation = nullptr;
};

} // namespace L0
