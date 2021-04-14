/*
 * Copyright (C) 2019-2021 Intel Corporation
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
#include "shared/source/utilities/wait_util.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <queue>
#include <unordered_map>

namespace L0 {

ze_result_t EventPoolImp::initialize(DriverHandle *driver, Context *context, uint32_t numDevices, ze_device_handle_t *phDevices, uint32_t numEvents) {
    std::vector<uint32_t> rootDeviceIndices;
    uint32_t maxRootDeviceIndex = 0u;

    void *eventPoolPtr = nullptr;
    ContextImp *contextImp = static_cast<ContextImp *>(context);

    size_t alignedSize = alignUp<size_t>(numEvents * eventSize, MemoryConstants::pageSize64k);
    NEO::GraphicsAllocation::AllocationType allocationType = isEventPoolUsedForTimestamp ? NEO::GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER
                                                                                         : NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;

    if (numDevices != 0) {
        for (uint32_t i = 0u; i < numDevices; i++) {
            ze_device_handle_t hDevice = phDevices[i];
            auto eventDevice = Device::fromHandle(hDevice);
            if (eventDevice == nullptr) {
                continue;
            }
            this->devices.push_back(eventDevice);
            rootDeviceIndices.push_back(eventDevice->getNEODevice()->getRootDeviceIndex());
            if (maxRootDeviceIndex < eventDevice->getNEODevice()->getRootDeviceIndex()) {
                maxRootDeviceIndex = eventDevice->getNEODevice()->getRootDeviceIndex();
            }
        }

        uint32_t rootDeviceIndex = rootDeviceIndices.at(0);
        auto deviceBitfield = devices[0]->getNEODevice()->getDeviceBitfield();

        NEO::AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                          true,
                                                          alignedSize,
                                                          allocationType,
                                                          deviceBitfield.count() > 1,
                                                          deviceBitfield.count() > 1,
                                                          deviceBitfield};
        unifiedMemoryProperties.alignment = eventAlignment;

        eventPoolAllocations = new NEO::MultiGraphicsAllocation(maxRootDeviceIndex);
        eventPoolPtr = driver->getMemoryManager()->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices,
                                                                                                   unifiedMemoryProperties,
                                                                                                   *eventPoolAllocations);

    } else {
        DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driver);
        numDevices = static_cast<uint32_t>(driverHandleImp->devices.size());

        for (uint32_t i = 0u; i < numDevices; i++) {
            Device *eventDevice = nullptr;
            eventDevice = driverHandleImp->devices[i];

            this->devices.push_back(eventDevice);
            rootDeviceIndices.push_back(eventDevice->getNEODevice()->getRootDeviceIndex());
            if (maxRootDeviceIndex < eventDevice->getNEODevice()->getRootDeviceIndex()) {
                maxRootDeviceIndex = eventDevice->getNEODevice()->getRootDeviceIndex();
            }
        }

        uint32_t rootDeviceIndex = rootDeviceIndices.at(0);
        auto &deviceBitfield = contextImp->deviceBitfields.at(rootDeviceIndex);

        NEO::AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                          true,
                                                          alignedSize,
                                                          allocationType,
                                                          false,
                                                          (deviceBitfield.count() > 1) && driverHandleImp->svmAllocsManager->getMultiOsContextSupport(),
                                                          deviceBitfield};
        unifiedMemoryProperties.flags.isUSMHostAllocation = true;
        unifiedMemoryProperties.flags.isUSMDeviceAllocation = false;
        unifiedMemoryProperties.alignment = eventAlignment;

        eventPoolAllocations = new NEO::MultiGraphicsAllocation(maxRootDeviceIndex);
        eventPoolPtr = driver->getMemoryManager()->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices,
                                                                                                   unifiedMemoryProperties,
                                                                                                   *eventPoolAllocations);
    }

    if (!eventPoolPtr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    return ZE_RESULT_SUCCESS;
}

EventPoolImp::~EventPoolImp() {
    auto graphicsAllocations = eventPoolAllocations->getGraphicsAllocations();
    auto memoryManager = devices[0]->getDriverHandle()->getMemoryManager();
    for (auto gpuAllocation : graphicsAllocations) {
        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
    delete eventPoolAllocations;
    eventPoolAllocations = nullptr;
}

ze_result_t EventPoolImp::getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t EventPoolImp::closeIpcHandle() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t EventPoolImp::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPoolImp::createEvent(const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    if (desc->index > (getNumEvents() - 1)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *phEvent = Event::create(this, desc, this->getDevice());

    return ZE_RESULT_SUCCESS;
}

uint64_t EventImp::getGpuAddress(Device *device) {
    auto alloc = eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
    return (alloc->getGpuAddress() + (index * eventPool->getEventSize()));
}

Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto event = new EventImp(eventPool, desc->index, device);
    UNRECOVERABLE_IF(event == nullptr);

    if (eventPool->isEventPoolUsedForTimestamp) {
        event->isTimestampEvent = true;
        event->kernelTimestampsData = std::make_unique<NEO::TimestampPackets<uint32_t>[]>(EventPacketsCount::maxKernelSplit);
    }

    auto alloc = eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());

    uint64_t baseHostAddr = reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer());
    event->hostAddress = reinterpret_cast<void *>(baseHostAddr + (desc->index * eventPool->getEventSize()));
    event->signalScope = desc->signal;
    event->waitScope = desc->wait;
    event->csr = static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver;
    event->reset();

    return event;
}

NEO::GraphicsAllocation &EventImp::getAllocation(Device *device) {
    return *this->eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
}

void Event::resetPackets() {
    for (uint32_t i = 0; i < kernelCount; i++) {
        kernelTimestampsData[i].setPacketsUsed(1);
    }
    kernelCount = 1;
}

uint32_t Event::getPacketsInUse() {
    if (isTimestampEvent) {
        uint32_t packetsInUse = 0;
        for (uint32_t i = 0; i < kernelCount; i++) {
            packetsInUse += kernelTimestampsData[i].getPacketsUsed();
        };
        return packetsInUse;
    } else {
        return 1;
    }
}

void Event::setPacketsInUse(uint32_t value) {
    kernelTimestampsData[getCurrKernelDataIndex()].setPacketsUsed(value);
};

uint64_t Event::getPacketAddress(Device *device) {
    uint64_t address = getGpuAddress(device);
    if (isTimestampEvent && kernelCount > 1) {
        for (uint32_t i = 0; i < kernelCount - 1; i++) {
            address += kernelTimestampsData[i].getPacketsUsed() *
                       NEO::TimestampPackets<uint32_t>::getSinglePacketSize();
        }
    }
    return address;
}

ze_result_t EventImp::calculateProfilingData() {
    globalStartTS = kernelTimestampsData[0].getGlobalStartValue(0);
    globalEndTS = kernelTimestampsData[0].getGlobalEndValue(0);
    contextStartTS = kernelTimestampsData[0].getContextStartValue(0);
    contextEndTS = kernelTimestampsData[0].getContextEndValue(0);

    for (uint32_t i = 0; i < kernelCount; i++) {
        for (auto packetId = 0u; packetId < kernelTimestampsData[i].getPacketsUsed(); packetId++) {
            if (globalStartTS > kernelTimestampsData[i].getGlobalStartValue(packetId)) {
                globalStartTS = kernelTimestampsData[i].getGlobalStartValue(packetId);
            }
            if (contextStartTS > kernelTimestampsData[i].getContextStartValue(packetId)) {
                contextStartTS = kernelTimestampsData[i].getContextStartValue(packetId);
            }
            if (contextEndTS < kernelTimestampsData[i].getContextEndValue(packetId)) {
                contextEndTS = kernelTimestampsData[i].getContextEndValue(packetId);
            }
            if (globalEndTS < kernelTimestampsData[i].getGlobalEndValue(packetId)) {
                globalEndTS = kernelTimestampsData[i].getGlobalEndValue(packetId);
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

void EventImp::assignTimestampData(void *address) {
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCopy = kernelTimestampsData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCopy; packetId++) {
            kernelTimestampsData[i].assignDataToAllTimestamps(packetId, address);
            address = ptrOffset(address, NEO::TimestampPackets<uint32_t>::getSinglePacketSize());
        }
    }
}

ze_result_t Event::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::queryStatusKernelTimestamp() {
    assignTimestampData(hostAddress);
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCheck = kernelTimestampsData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCheck; packetId++) {
            if (kernelTimestampsData[i].getContextEndValue(packetId) == Event::STATE_CLEARED) {
                return ZE_RESULT_NOT_READY;
            }
        }
    }
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
        return queryStatusKernelTimestamp();
    }
    memcpy_s(static_cast<void *>(&queryVal), sizeof(uint32_t), static_cast<void *>(hostAddr), sizeof(uint32_t));
    return queryVal == Event::STATE_CLEARED ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;
}

ze_result_t EventImp::hostEventSetValueTimestamps(uint32_t eventVal) {

    auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);
    auto signalScopeFlag = this->signalScope;

    auto eventTsSetFunc = [&eventVal, &signalScopeFlag](auto tsAddr) {
        auto tsptr = reinterpret_cast<void *>(tsAddr);

        memcpy_s(tsptr, sizeof(uint32_t), static_cast<void *>(&eventVal), sizeof(uint32_t));
        if (!signalScopeFlag) {
            NEO::CpuIntrinsics::clFlush(tsptr);
        }
    };
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToSet = kernelTimestampsData[i].getPacketsUsed();
        for (uint32_t i = 0; i < packetsToSet; i++) {
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<uint32_t>::getContextStartOffset());
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<uint32_t>::getGlobalStartOffset());
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<uint32_t>::getContextEndOffset());
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<uint32_t>::getGlobalEndOffset());
            baseAddr += NEO::TimestampPackets<uint32_t>::getSinglePacketSize();
        }
    }
    assignTimestampData(hostAddress);

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

        NEO::WaitUtils::waitFunction(nullptr, 0u);

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
    if (isTimestampEvent) {
        kernelCount = EventPacketsCount::maxKernelSplit;
        for (uint32_t i = 0; i < kernelCount; i++) {
            kernelTimestampsData[i].setPacketsUsed(NEO::TimestampPacketSizeControl::preferredPacketCount);
        }
        hostEventSetValue(Event::STATE_INITIAL);
        resetPackets();
        return ZE_RESULT_SUCCESS;
    } else {
        return hostEventSetValue(Event::STATE_INITIAL);
    }
}

ze_result_t EventImp::queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) {

    ze_kernel_timestamp_result_t &result = *dstptr;

    if (queryStatus() != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_NOT_READY;
    }

    assignTimestampData(hostAddress);
    calculateProfilingData();

    auto eventTsSetFunc = [&](uint64_t &timestampFieldToCopy, uint64_t &timestampFieldForWriting) {
        memcpy_s(&(timestampFieldForWriting), sizeof(uint64_t), static_cast<void *>(&timestampFieldToCopy), sizeof(uint64_t));
    };

    if (!NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps()) {
        eventTsSetFunc(contextStartTS, result.context.kernelStart);
        eventTsSetFunc(globalStartTS, result.global.kernelStart);
        eventTsSetFunc(contextEndTS, result.context.kernelEnd);
        eventTsSetFunc(globalEndTS, result.global.kernelEnd);
    } else {
        eventTsSetFunc(globalStartTS, result.context.kernelStart);
        eventTsSetFunc(globalStartTS, result.global.kernelStart);
        eventTsSetFunc(globalEndTS, result.context.kernelEnd);
        eventTsSetFunc(globalEndTS, result.global.kernelEnd);
    }

    return ZE_RESULT_SUCCESS;
}

EventPool *EventPool::create(DriverHandle *driver, Context *context, uint32_t numDevices,
                             ze_device_handle_t *phDevices,
                             const ze_event_pool_desc_t *desc) {
    auto eventPool = new (std::nothrow) EventPoolImp(driver, numDevices, phDevices, desc->count, desc->flags);
    if (!eventPool) {
        DEBUG_BREAK_IF(true);
        return nullptr;
    }

    ze_result_t result = eventPool->initialize(driver, context, numDevices, phDevices, desc->count);
    if (result) {
        delete eventPool;
        return nullptr;
    }
    return eventPool;
}

} // namespace L0
