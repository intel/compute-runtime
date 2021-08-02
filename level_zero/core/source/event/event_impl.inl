/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/event/event.h"

namespace L0 {
template <typename TagSizeT>
Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto event = new EventImp<TagSizeT>(eventPool, desc->index, device);
    UNRECOVERABLE_IF(event == nullptr);

    if (eventPool->isEventPoolTimestampFlagSet()) {
        event->setEventTimestampFlag(true);
        event->kernelTimestampsData = std::make_unique<KernelTimestampsData<TagSizeT>[]>(EventPacketsCount::maxKernelSplit);
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

template <typename TagSizeT>
uint64_t EventImp<TagSizeT>::getGpuAddress(Device *device) {
    auto alloc = eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
    return (alloc->getGpuAddress() + (index * eventPool->getEventSize()));
}

template <typename TagSizeT>
NEO::GraphicsAllocation &EventImp<TagSizeT>::getAllocation(Device *device) {
    return *this->eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::calculateProfilingData() {
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

template <typename TagSizeT>
void EventImp<TagSizeT>::assignTimestampData(void *address) {
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCopy = kernelTimestampsData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCopy; packetId++) {
            kernelTimestampsData[i].assignDataToAllTimestamps(packetId, address);
            address = ptrOffset(address, NEO::TimestampPackets<TagSizeT>::getSinglePacketSize());
        }
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatusKernelTimestamp() {
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

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatus() {
    uint64_t *hostAddr = static_cast<uint64_t *>(hostAddress);
    uint32_t queryVal = Event::STATE_CLEARED;
    ze_result_t retVal;

    if (metricStreamer != nullptr) {
        *hostAddr = metricStreamer->getNotificationState();
    }
    this->csr->downloadAllocations();
    if (isEventTimestampFlagSet()) {
        return queryStatusKernelTimestamp();
    }
    memcpy_s(static_cast<void *>(&queryVal), sizeof(uint32_t), static_cast<void *>(hostAddr), sizeof(uint32_t));
    retVal = (queryVal == Event::STATE_CLEARED) ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;

    if (retVal == ZE_RESULT_NOT_READY) {
        return retVal;
    }

    return retVal;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValueTimestamps(TagSizeT eventVal) {

    auto baseAddr = reinterpret_cast<uint64_t>(hostAddress);
    auto signalScopeFlag = this->signalScope;

    auto eventTsSetFunc = [&eventVal, &signalScopeFlag](auto tsAddr) {
        auto tsptr = reinterpret_cast<void *>(tsAddr);

        memcpy_s(tsptr, sizeof(TagSizeT), static_cast<void *>(&eventVal), sizeof(TagSizeT));
        if (!signalScopeFlag) {
            NEO::CpuIntrinsics::clFlush(tsptr);
        }
    };
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToSet = kernelTimestampsData[i].getPacketsUsed();
        for (uint32_t i = 0; i < packetsToSet; i++) {
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<TagSizeT>::getContextStartOffset());
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<TagSizeT>::getGlobalStartOffset());
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<TagSizeT>::getContextEndOffset());
            eventTsSetFunc(baseAddr + NEO::TimestampPackets<TagSizeT>::getGlobalEndOffset());
            baseAddr += NEO::TimestampPackets<TagSizeT>::getSinglePacketSize();
        }
    }
    assignTimestampData(hostAddress);

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValue(uint32_t eventVal) {
    if (isEventTimestampFlagSet()) {
        return hostEventSetValueTimestamps(static_cast<TagSizeT>(eventVal));
    }

    auto hostAddr = static_cast<uint64_t *>(hostAddress);
    UNRECOVERABLE_IF(hostAddr == nullptr);
    memcpy_s(static_cast<void *>(hostAddr), sizeof(uint32_t), static_cast<void *>(&eventVal), sizeof(uint32_t));

    NEO::CpuIntrinsics::clFlush(hostAddr);

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSignal() {
    return hostEventSetValue(Event::STATE_SIGNALED);
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSynchronize(uint64_t timeout) {
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
            return ret;
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

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::reset() {
    if (isEventTimestampFlagSet()) {
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

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) {

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

template <typename TagSizeT>
void EventImp<TagSizeT>::resetPackets() {
    for (uint32_t i = 0; i < kernelCount; i++) {
        kernelTimestampsData[i].setPacketsUsed(1);
    }
    kernelCount = 1;
}

template <typename TagSizeT>
uint32_t EventImp<TagSizeT>::getPacketsInUse() {
    if (isEventTimestampFlagSet()) {
        uint32_t packetsInUse = 0;
        for (uint32_t i = 0; i < kernelCount; i++) {
            packetsInUse += kernelTimestampsData[i].getPacketsUsed();
        };
        return packetsInUse;
    } else {
        return 1;
    }
}

template <typename TagSizeT>
void EventImp<TagSizeT>::setPacketsInUse(uint32_t value) {
    kernelTimestampsData[getCurrKernelDataIndex()].setPacketsUsed(value);
};

template <typename TagSizeT>
uint64_t EventImp<TagSizeT>::getPacketAddress(Device *device) {
    uint64_t address = getGpuAddress(device);
    if (isEventTimestampFlagSet() && kernelCount > 1) {
        for (uint32_t i = 0; i < kernelCount - 1; i++) {
            address += kernelTimestampsData[i].getPacketsUsed() *
                       NEO::TimestampPackets<TagSizeT>::getSinglePacketSize();
        }
    }
    return address;
}

} // namespace L0
