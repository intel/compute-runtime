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

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <queue>
#include <unordered_map>

namespace L0 {

ze_result_t EventPoolImp::initialize(DriverHandle *driver, uint32_t numDevices, ze_device_handle_t *phDevices, uint32_t numEvents) {
    std::vector<uint32_t> rootDeviceIndices;
    uint32_t maxRootDeviceIndex = 0u;
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

    if (this->devices.empty()) {
        ze_device_handle_t hDevice;
        uint32_t count = 1;
        ze_result_t result = driver->getDevice(&count, &hDevice);
        if (result) {
            return result;
        }
        this->devices.push_back(Device::fromHandle(hDevice));
        rootDeviceIndices.push_back(this->devices[0]->getNEODevice()->getRootDeviceIndex());
        maxRootDeviceIndex = rootDeviceIndices[0];
    }

    if (this->devices.size() > 1) {
        this->allocOnDevice = false;
    }

    if (allocOnDevice) {
        ze_command_queue_desc_t cmdQueueDesc = {};
        cmdQueueDesc.ordinal = 0;
        cmdQueueDesc.index = 0;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        eventPoolCommandList =
            CommandList::createImmediate(
                static_cast<DeviceImp *>(this->devices[0])->neoDevice->getHardwareInfo().platform.eProductFamily,
                this->devices[0],
                &cmdQueueDesc,
                true,
                NEO::EngineGroupType::RenderCompute,
                returnValue);

        if (!this->eventPoolCommandList) {
            this->allocOnDevice = false;
        }
    }

    eventPoolAllocations = new NEO::MultiGraphicsAllocation(maxRootDeviceIndex);

    uint32_t rootDeviceIndex = rootDeviceIndices.at(0);
    auto deviceBitfield = devices[0]->getNEODevice()->getDeviceBitfield();
    auto allocationType = isEventPoolUsedForTimestamp ? NEO::GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER : NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
    if (this->allocOnDevice) {
        allocationType = NEO::GraphicsAllocation::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER;
    }

    NEO::AllocationProperties eventPoolAllocationProperties{rootDeviceIndex,
                                                            true,
                                                            alignUp<size_t>(numEvents * eventSize, MemoryConstants::pageSize64k),
                                                            allocationType,
                                                            deviceBitfield.count() > 1,
                                                            false,
                                                            deviceBitfield};
    eventPoolAllocationProperties.alignment = MemoryConstants::cacheLineSize;

    void *eventPoolPtr = driver->getMemoryManager()->createMultiGraphicsAllocation(rootDeviceIndices,
                                                                                   eventPoolAllocationProperties,
                                                                                   *eventPoolAllocations);
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

    if (eventPoolCommandList) {
        eventPoolCommandList->destroy();
        eventPoolCommandList = nullptr;
    }
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

Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto event = new EventImp(eventPool, desc->index, device);
    UNRECOVERABLE_IF(event == nullptr);

    if (eventPool->isEventPoolUsedForTimestamp) {
        event->isTimestampEvent = true;
        event->timestampsData = std::make_unique<TimestampPacketStorage>();
    }

    if (eventPool->allocOnDevice) {
        event->allocOnDevice = true;
    }

    auto alloc = eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());

    uint64_t baseHostAddr = reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer());
    event->hostAddress = reinterpret_cast<void *>(baseHostAddr + (desc->index * eventPool->getEventSize()));
    event->gpuAddress = alloc->getGpuAddress() + (desc->index * eventPool->getEventSize());
    event->signalScope = desc->signal;
    event->waitScope = desc->wait;
    event->csr = static_cast<DeviceImp *>(device)->neoDevice->getDefaultEngine().commandStreamReceiver;
    event->reset();

    if (event->allocOnDevice) {
        eventPool->eventPoolCommandList->appendEventReset(event->toHandle());
    }

    return event;
}

NEO::GraphicsAllocation &Event::getAllocation() {
    auto eventImp = static_cast<EventImp *>(this);

    return *eventImp->eventPool->getAllocation().getGraphicsAllocation(eventImp->device->getNEODevice()->getRootDeviceIndex());
}

uint64_t Event::getTimestampPacketAddress() {
    return gpuAddress + packetsInUse * sizeof(TimestampPacketStorage::Packet);
}

ze_result_t EventImp::calculateProfilingData() {
    globalStartTS = timestampsData->packets[0].globalStart;
    globalEndTS = timestampsData->packets[0].globalEnd;
    contextStartTS = timestampsData->packets[0].contextStart;
    contextEndTS = timestampsData->packets[0].contextEnd;

    for (auto i = 1u; i < packetsInUse; i++) {
        auto &packet = timestampsData->packets[i];
        if (globalStartTS > packet.globalStart) {
            globalStartTS = packet.globalStart;
        }
        if (contextStartTS > packet.contextStart) {
            contextStartTS = packet.contextStart;
        }
        if (contextEndTS < packet.contextEnd) {
            contextEndTS = packet.contextEnd;
        }
        if (globalEndTS < packet.globalEnd) {
            globalEndTS = packet.globalEnd;
        }
    }

    return ZE_RESULT_SUCCESS;
}

void EventImp::assignTimestampData(void *address) {
    auto baseAddr = reinterpret_cast<uint64_t>(address);
    uint32_t packetsToCopy = packetsInUse ? packetsInUse : NEO::TimestampPacketSizeControl::preferredPacketCount;

    auto copyData = [&](uint32_t &timestampField, auto tsAddr) {
        memcpy_s(static_cast<void *>(&timestampField), sizeof(uint32_t), reinterpret_cast<void *>(tsAddr), sizeof(uint32_t));
    };

    for (uint32_t i = 0; i < packetsToCopy; i++) {
        auto &packet = timestampsData->packets[i];
        copyData(packet.globalStart, baseAddr + offsetof(TimestampPacketStorage::Packet, globalStart));
        copyData(packet.contextStart, baseAddr + offsetof(TimestampPacketStorage::Packet, contextStart));
        copyData(packet.globalEnd, baseAddr + offsetof(TimestampPacketStorage::Packet, globalEnd));
        copyData(packet.contextEnd, baseAddr + offsetof(TimestampPacketStorage::Packet, contextEnd));
        baseAddr += sizeof(struct TimestampPacketStorage::Packet);
    }
}

uint64_t Event::getGpuAddress() {
    return gpuAddress;
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
        auto timeStampAddress = baseAddr + offsetof(TimestampPacketStorage::Packet, contextEnd);
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

    for (uint32_t i = 0; i < NEO::TimestampPacketSizeControl::preferredPacketCount; i++) {
        eventTsSetFunc(baseAddr + offsetof(TimestampPacketStorage::Packet, contextStart));
        eventTsSetFunc(baseAddr + offsetof(TimestampPacketStorage::Packet, globalStart));
        eventTsSetFunc(baseAddr + offsetof(TimestampPacketStorage::Packet, contextEnd));
        eventTsSetFunc(baseAddr + offsetof(TimestampPacketStorage::Packet, globalEnd));
        baseAddr += sizeof(struct TimestampPacketStorage::Packet);
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
    if (allocOnDevice) {
        return ZE_RESULT_SUCCESS;
    }

    resetPackets();
    return hostEventSetValue(Event::STATE_INITIAL);
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

EventPool *EventPool::create(DriverHandle *driver, uint32_t numDevices,
                             ze_device_handle_t *phDevices,
                             const ze_event_pool_desc_t *desc) {
    auto eventPool = new EventPoolImp(driver, numDevices, phDevices, desc->count, desc->flags);
    if (eventPool == nullptr) {
        return nullptr;
    }

    ze_result_t result = eventPool->initialize(driver, numDevices, phDevices, desc->count);
    if (result) {
        delete eventPool;
        return nullptr;
    }
    return eventPool;
}

} // namespace L0
