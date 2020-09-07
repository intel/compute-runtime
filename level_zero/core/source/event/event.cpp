/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event/event.h"

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <queue>
#include <unordered_map>

namespace L0 {

struct EventPoolImp : public EventPool {
    EventPoolImp(DriverHandle *driver, uint32_t numDevices, ze_device_handle_t *phDevices, uint32_t numEvents, ze_event_pool_flags_t flags) : numEvents(numEvents) {
        if (flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
            isEventPoolUsedForTimestamp = true;
        }
        ze_device_handle_t hDevice;
        if (numDevices > 0) {
            hDevice = phDevices[0];
        } else {
            uint32_t count = 1;
            ze_result_t result = driver->getDevice(&count, &hDevice);

            UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);
        }
        device = Device::fromHandle(hDevice);

        NEO::AllocationProperties properties(
            device->getRootDeviceIndex(), numEvents * eventSize,
            isEventPoolUsedForTimestamp ? NEO::GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER
                                        : NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
            device->getNEODevice()->getDeviceBitfield());
        properties.alignment = MemoryConstants::cacheLineSize;
        eventPoolAllocation = driver->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        UNRECOVERABLE_IF(eventPoolAllocation == nullptr);
    }

    ~EventPoolImp() override {
        device->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(eventPoolAllocation);
        eventPoolAllocation = nullptr;
    }

    ze_result_t destroy() override;

    ze_result_t getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) override;

    ze_result_t closeIpcHandle() override;

    ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) override {
        if (desc->index > (getNumEvents() - 1)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *phEvent = Event::create(this, desc, this->getDevice());

        return ZE_RESULT_SUCCESS;
    }

    uint32_t getEventSize() override { return eventSize; }
    size_t getNumEvents() { return numEvents; }

    Device *getDevice() override { return device; }

    Device *device;
    size_t numEvents;

  protected:
    const uint32_t eventSize = static_cast<uint32_t>(alignUp(sizeof(struct KernelTimestampEvent),
                                                             MemoryConstants::cacheLineSize));
    const uint32_t eventAlignment = MemoryConstants::cacheLineSize;
};

Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto event = new EventImp(eventPool, desc->index, device);
    UNRECOVERABLE_IF(event == nullptr);

    if (eventPool->isEventPoolUsedForTimestamp) {
        event->isTimestampEvent = true;
    }

    uint64_t baseHostAddr = reinterpret_cast<uint64_t>(eventPool->getAllocation().getUnderlyingBuffer());
    event->hostAddress = reinterpret_cast<void *>(baseHostAddr + (desc->index * eventPool->getEventSize()));
    event->gpuAddress = eventPool->getAllocation().getGpuAddress() + (desc->index * eventPool->getEventSize());

    event->signalScope = desc->signal;
    event->waitScope = desc->wait;
    event->csr = static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver;

    event->reset();

    return event;
}

NEO::GraphicsAllocation &Event::getAllocation() {
    auto eventImp = static_cast<EventImp *>(this);

    return eventImp->eventPool->getAllocation();
}

ze_result_t Event::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::queryStatus() {
    uint64_t *hostAddr = static_cast<uint64_t *>(hostAddress);
    uint32_t queryVal = Event::STATE_CLEARED;
    if (metricStreamer != nullptr) {
        *hostAddr = metricStreamer->getNotificationState();
    }
    this->csr->downloadAllocations();
    if (isTimestampEvent) {
        auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);
        auto timeStampAddress = baseAddr + offsetof(KernelTimestampEvent, contextEnd);
        hostAddr = reinterpret_cast<uint64_t *>(timeStampAddress);
    }
    memcpy_s(static_cast<void *>(&queryVal), sizeof(uint32_t), static_cast<void *>(hostAddr), sizeof(uint32_t));
    return queryVal == Event::STATE_CLEARED ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::hostEventSetValueTimestamps(uint32_t eventVal) {

    auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);
    auto signalScopeFlag = this->signalScope;

    auto eventTsSetFunc = [&](auto tsAddr) {
        auto tsptr = reinterpret_cast<void *>(tsAddr);
        memcpy_s(tsptr, sizeof(uint32_t), static_cast<void *>(&eventVal), sizeof(uint32_t));
        if (!signalScopeFlag) {
            NEO::CpuIntrinsics::clFlush(tsptr);
        }
    };

    eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, contextStart));
    eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalStart));
    eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, contextEnd));
    eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalEnd));

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::hostEventSetValue(uint32_t eventVal) {
    if (isTimestampEvent) {
        return hostEventSetValueTimestamps(eventVal);
    }

    auto hostAddr = static_cast<uint64_t *>(hostAddress);
    UNRECOVERABLE_IF(hostAddr == nullptr);
    memcpy_s(static_cast<void *>(hostAddr), sizeof(uint32_t), static_cast<void *>(&eventVal), sizeof(uint32_t));

    NEO::CpuIntrinsics::clFlush(hostAddr);

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::hostSignal() {
    return hostEventSetValue(Event::STATE_SIGNALED);
}

ze_result_t EventImp::hostSynchronize(uint64_t timeout) {
    std::chrono::high_resolution_clock::time_point time1, time2;
    uint64_t timeDiff = 0;
    ze_result_t ret = ZE_RESULT_NOT_READY;

    if (this->csr->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
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

ze_result_t EventImp::queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) {
    auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);
    constexpr uint64_t tsMask = (1ull << 32) - 1;
    uint64_t tsData = Event::STATE_INITIAL & tsMask;
    ze_kernel_timestamp_result_t &result = *dstptr;

    // Ensure timestamps have been written
    if (queryStatus() != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_NOT_READY;
    }

    auto eventTsSetFunc = [&](auto tsAddr, uint64_t &timestampField) {
        memcpy_s(static_cast<void *>(&tsData), sizeof(uint32_t), reinterpret_cast<void *>(tsAddr), sizeof(uint32_t));

        tsData &= tsMask;
        memcpy_s(&(timestampField), sizeof(uint64_t), static_cast<void *>(&tsData), sizeof(uint64_t));
    };

    if (!NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps()) {
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, contextStart), result.context.kernelStart);
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalStart), result.global.kernelStart);
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, contextEnd), result.context.kernelEnd);
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalEnd), result.global.kernelEnd);
    } else {
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalStart), result.context.kernelStart);
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalStart), result.global.kernelStart);
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalEnd), result.context.kernelEnd);
        eventTsSetFunc(baseAddr + offsetof(KernelTimestampEvent, globalEnd), result.global.kernelEnd);
    }

    return ZE_RESULT_SUCCESS;
}

EventPool *EventPool::create(DriverHandle *driver, uint32_t numDevices,
                             ze_device_handle_t *phDevices,
                             const ze_event_pool_desc_t *desc) {
    return new EventPoolImp(driver, numDevices, phDevices, desc->count, desc->flags);
}

ze_result_t EventPoolImp::getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t EventPoolImp::closeIpcHandle() { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

ze_result_t EventPoolImp::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
