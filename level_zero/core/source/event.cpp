/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event.h"

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <queue>
#include <unordered_map>

namespace L0 {

struct EventImp : public Event {
    EventImp(EventPool *eventPool, int index, Device *device)
        : device(device), eventPool(eventPool) {}

    ~EventImp() override {}

    ze_result_t hostSignal() override;

    ze_result_t hostSynchronize(uint32_t timeout) override;

    ze_result_t queryStatus() override {
        uint64_t *hostAddr = static_cast<uint64_t *>(hostAddress);
        auto alloc = &(this->eventPool->getAllocation());
        auto csr = static_cast<DeviceImp *>(this->device)->neoDevice->getDefaultEngine().commandStreamReceiver;

        if (metricTracer != nullptr) {
            *hostAddr = metricTracer->getNotificationState();
        }

        csr->downloadAllocation(*alloc);

        if (isTimestampEvent) {
            auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);

            auto timeStampAddress = baseAddr + getOffsetOfProfilingEvent(ZE_EVENT_TIMESTAMP_CONTEXT_END);
            hostAddr = reinterpret_cast<uint64_t *>(timeStampAddress);
        }

        return *hostAddr == Event::STATE_CLEARED ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;
    }

    ze_result_t reset() override;

    ze_result_t getTimestamp(ze_event_timestamp_type_t timestampType, void *dstptr) override;

    Device *device;
    EventPool *eventPool;

  protected:
    ze_result_t hostEventSetValue(uint32_t eventValue);
    ze_result_t hostEventSetValueTimestamps(uint32_t eventVal);
    void makeAllocationResident();
};

struct EventPoolImp : public EventPool {
    EventPoolImp(Device *device, uint32_t count, ze_event_pool_flag_t flags) : device(device), count(count) {
        pool = std::vector<int>(this->count);
        eventPoolUsedCount = 0;
        for (uint32_t i = 0; i < count; i++) {
            pool[i] = EventPool::EVENT_STATE_INITIAL;
        }

        auto timestampMultiplier = 1;
        if (flags == ZE_EVENT_POOL_FLAG_TIMESTAMP) {
            isEventPoolUsedForTimestamp = true;
            timestampMultiplier = numEventTimestampTypes;
        }

        NEO::AllocationProperties properties(
            device->getRootDeviceIndex(), count * eventSize * timestampMultiplier, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
        properties.alignment = eventAlignment;
        eventPoolAllocation = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        UNRECOVERABLE_IF(eventPoolAllocation == nullptr);
    }

    ~EventPoolImp() override {
        device->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(eventPoolAllocation);
        eventPoolAllocation = nullptr;

        eventTracker.clear();
    }

    ze_result_t destroy() override;

    size_t getPoolSize() override { return this->pool.size(); }
    uint32_t getPoolUsedCount() override { return eventPoolUsedCount; }

    ze_result_t getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) override;

    ze_result_t closeIpcHandle() override;

    ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) override {
        *phEvent = Event::create(this, desc, this->getDevice());

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t reserveEventFromPool(int index, Event *event) override;

    ze_result_t releaseEventToPool(Event *event) override;

    uint32_t getEventSize() override { return eventSize; }

    uint32_t getNumEventTimestampTypes() override { return numEventTimestampTypes; }

    ze_result_t destroyPool() {
        if (eventPoolUsedCount != 0) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        pool.clear();
        return ZE_RESULT_SUCCESS;
    }

    Device *getDevice() override { return device; }

    Device *device;
    uint32_t count;
    uint32_t eventPoolUsedCount;
    std::vector<int> pool;
    std::unordered_map<Event *, int> eventTracker;

    std::queue<int> lastEventPoolOffsetUsed;

  protected:
    const uint32_t eventSize = 64u;
    const uint32_t eventAlignment = 64u;

    const uint32_t numEventTimestampTypes = 4u;
};

Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto event = new EventImp(eventPool, desc->index, device);
    UNRECOVERABLE_IF(event == nullptr);
    eventPool->reserveEventFromPool(desc->index, static_cast<Event *>(event));

    if (eventPool->isEventPoolUsedForTimestamp) {
        event->isTimestampEvent = true;
    }

    event->signalScope = desc->signal;
    event->waitScope = desc->wait;

    event->reset();

    return event;
}

NEO::GraphicsAllocation &Event::getAllocation() {
    auto eventImp = static_cast<EventImp *>(this);

    return eventImp->eventPool->getAllocation();
}

uint64_t Event::getOffsetOfProfilingEvent(uint32_t profileEventType) {
    auto eventImp = static_cast<EventImp *>(this);
    auto eventSize = eventImp->eventPool->getEventSize();
    return (profileEventType * eventSize);
}

ze_result_t Event::destroy() {
    auto eventImp = static_cast<EventImp *>(this);
    if (eventImp->eventPool->releaseEventToPool(this)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

void EventImp::makeAllocationResident() {
    auto deviceImp = static_cast<DeviceImp *>(this->device);
    NEO::MemoryOperationsHandler *memoryOperationsIface = deviceImp->neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto alloc = &(this->eventPool->getAllocation());

    if (memoryOperationsIface) {
        memoryOperationsIface->makeResident(ArrayRef<NEO::GraphicsAllocation *>(&alloc, 1));
    }
}

ze_result_t EventImp::hostEventSetValueTimestamps(uint32_t eventVal) {
    for (uint32_t i = 0; i < this->eventPool->getNumEventTimestampTypes(); i++) {
        auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);
        auto timeStampAddress = baseAddr + getOffsetOfProfilingEvent(i);
        auto tsptr = reinterpret_cast<uint64_t *>(timeStampAddress);

        *(tsptr) = eventVal;

        if (this->signalScope != ZE_EVENT_SCOPE_FLAG_NONE) {
            NEO::CpuIntrinsics::clFlush(tsptr);
        }
    }

    makeAllocationResident();

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::hostEventSetValue(uint32_t eventVal) {
    if (isTimestampEvent) {
        hostEventSetValueTimestamps(eventVal);
    }

    auto hostAddr = static_cast<uint64_t *>(hostAddress);
    UNRECOVERABLE_IF(hostAddr == nullptr);
    *(hostAddr) = eventVal;

    makeAllocationResident();

    if (this->signalScope != ZE_EVENT_SCOPE_FLAG_NONE) {
        NEO::CpuIntrinsics::clFlush(hostAddr);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::hostSignal() {
    return hostEventSetValue(Event::STATE_SIGNALED);
}

ze_result_t EventImp::hostSynchronize(uint32_t timeout) {
    std::chrono::high_resolution_clock::time_point time1, time2;
    int64_t timeDiff = 0;
    ze_result_t ret = ZE_RESULT_NOT_READY;
    auto csr = static_cast<DeviceImp *>(this->device)->neoDevice->getDefaultEngine().commandStreamReceiver;

    if (csr->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
    }

    if (timeout == 0) {
        return queryStatus();
    }

    time1 = std::chrono::high_resolution_clock::now();
    while (true) {
        ret = queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            return ZE_RESULT_SUCCESS;
        }

        std::this_thread::yield();
        NEO::CpuIntrinsics::pause();

        if (timeout == std::numeric_limits<uint32_t>::max()) {
            continue;
        }

        time2 = std::chrono::high_resolution_clock::now();
        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(time2 - time1).count();

        if (timeDiff >= timeout) {
            break;
        }
    }

    return ret;
}

ze_result_t EventImp::reset() {
    return hostEventSetValue(Event::STATE_INITIAL);
}

ze_result_t EventImp::getTimestamp(ze_event_timestamp_type_t timestampType, void *dstptr) {
    auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);

    if (!this->isTimestampEvent)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    auto timeStampAddress = baseAddr + getOffsetOfProfilingEvent(timestampType);
    auto tsptr = reinterpret_cast<uint64_t *>(timeStampAddress);

    memcpy_s(dstptr, sizeof(uint64_t), static_cast<void *>(tsptr), sizeof(uint64_t));

    return ZE_RESULT_SUCCESS;
}

EventPool *EventPool::create(Device *device, const ze_event_pool_desc_t *desc) {
    auto eventPool = new EventPoolImp(device, desc->count, desc->flags);
    UNRECOVERABLE_IF(eventPool == nullptr);

    return eventPool;
}

ze_result_t EventPoolImp::reserveEventFromPool(int index, Event *event) {
    if (pool[index] == EventPool::EVENT_STATE_CREATED) {
        return ZE_RESULT_SUCCESS;
    }

    pool[index] = EventPool::EVENT_STATE_CREATED;
    eventTracker.insert(std::pair<Event *, int>(event, index));

    if (lastEventPoolOffsetUsed.empty()) {
        event->offsetUsed = index;
    } else {
        event->offsetUsed = lastEventPoolOffsetUsed.front();
        lastEventPoolOffsetUsed.pop();
    }

    auto timestampMultiplier = 1;
    if (static_cast<struct EventPool *>(this)->isEventPoolUsedForTimestamp) {
        timestampMultiplier = numEventTimestampTypes;
    }

    uint64_t baseHostAddr = reinterpret_cast<uint64_t>(eventPoolAllocation->getUnderlyingBuffer());
    event->hostAddress = reinterpret_cast<void *>(baseHostAddr + (event->offsetUsed * eventSize * timestampMultiplier));
    event->gpuAddress = eventPoolAllocation->getGpuAddress() + (event->offsetUsed * eventSize * timestampMultiplier);

    eventPoolUsedCount++;

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPoolImp::releaseEventToPool(Event *event) {
    UNRECOVERABLE_IF(event == nullptr);
    if (eventTracker.find(event) == eventTracker.end()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    int index = eventTracker[event];
    pool[index] = EventPool::EVENT_STATE_DESTROYED;
    eventTracker.erase(event);

    event->hostAddress = nullptr;
    event->gpuAddress = 0;

    lastEventPoolOffsetUsed.push(event->offsetUsed);
    event->offsetUsed = -1;

    eventPoolUsedCount--;

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPoolImp::getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t EventPoolImp::closeIpcHandle() { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

ze_result_t EventPoolImp::destroy() {
    if (this->destroyPool()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t eventPoolOpenIpcHandle(ze_driver_handle_t hDriver, ze_ipc_event_pool_handle_t hIpc,
                                   ze_event_pool_handle_t *phEventPool) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t eventCreate(ze_event_pool_handle_t hEventPool, const ze_event_desc_t *desc,
                        ze_event_handle_t *phEvent) {
    EventPool *eventPool = EventPool::fromHandle(hEventPool);
    UNRECOVERABLE_IF(eventPool == nullptr);

    if (desc->index > (eventPool->getPoolSize() - 1)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if ((eventPool->getPoolUsedCount() + 1) > eventPool->getPoolSize()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return eventPool->createEvent(desc, phEvent);
}

ze_result_t eventDestroy(ze_event_handle_t hEvent) { return Event::fromHandle(hEvent)->destroy(); }

} // namespace L0
