/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"

#include "level_zero/core/source/event/event.h"

namespace L0 {
template <typename TagSizeT>
Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto event = new EventImp<TagSizeT>(eventPool, desc->index, device);
    UNRECOVERABLE_IF(event == nullptr);

    if (eventPool->isEventPoolTimestampFlagSet()) {
        event->setEventTimestampFlag(true);
    }
    event->kernelEventCompletionData = std::make_unique<KernelEventCompletionData<TagSizeT>[]>(EventPacketsCount::maxKernelSplit);

    auto alloc = eventPool->getAllocation().getGraphicsAllocation(device->getNEODevice()->getRootDeviceIndex());

    uint64_t baseHostAddr = reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer());
    event->hostAddress = reinterpret_cast<void *>(baseHostAddr + (desc->index * eventPool->getEventSize()));
    event->signalScope = desc->signal;
    event->waitScope = desc->wait;
    event->csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    EventPoolImp *EventPoolImp = static_cast<struct EventPoolImp *>(eventPool);
    // do not reset even if it has been imported, since event pool
    // might have been imported after events being already signaled
    if (EventPoolImp->isImportedIpcPool == false) {
        event->reset();
    }

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
    constexpr uint32_t skipL3EventPacketIndex = 2u;

    globalStartTS = kernelEventCompletionData[0].getGlobalStartValue(0);
    globalEndTS = kernelEventCompletionData[0].getGlobalEndValue(0);
    contextStartTS = kernelEventCompletionData[0].getContextStartValue(0);
    contextEndTS = kernelEventCompletionData[0].getContextEndValue(0);

    for (uint32_t i = 0; i < kernelCount; i++) {
        for (auto packetId = 0u; packetId < kernelEventCompletionData[i].getPacketsUsed(); packetId++) {
            if (this->l3FlushWaApplied && ((packetId % skipL3EventPacketIndex) != 0)) {
                continue;
            }
            if (globalStartTS > kernelEventCompletionData[i].getGlobalStartValue(packetId)) {
                globalStartTS = kernelEventCompletionData[i].getGlobalStartValue(packetId);
            }
            if (contextStartTS > kernelEventCompletionData[i].getContextStartValue(packetId)) {
                contextStartTS = kernelEventCompletionData[i].getContextStartValue(packetId);
            }
            if (contextEndTS < kernelEventCompletionData[i].getContextEndValue(packetId)) {
                contextEndTS = kernelEventCompletionData[i].getContextEndValue(packetId);
            }
            if (globalEndTS < kernelEventCompletionData[i].getGlobalEndValue(packetId)) {
                globalEndTS = kernelEventCompletionData[i].getGlobalEndValue(packetId);
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::assignKernelEventCompletionData(void *address) {
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCopy = 0;
        packetsToCopy = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCopy; packetId++) {
            kernelEventCompletionData[i].assignDataToAllTimestamps(packetId, address);
            address = ptrOffset(address, singlePacketSize);
        }
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatusKernelTimestamp() {
    assignKernelEventCompletionData(hostAddress);
    uint32_t queryVal = Event::STATE_CLEARED;
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCheck = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCheck; packetId++) {
            bool ready = NEO::WaitUtils::waitFunctionWithPredicate<const TagSizeT>(
                static_cast<TagSizeT const *>(kernelEventCompletionData[i].getContextEndAddress(packetId)),
                queryVal,
                std::not_equal_to<TagSizeT>());
            if (!ready) {
                return ZE_RESULT_NOT_READY;
            }
        }
    }
    this->csr->getInternalAllocationStorage()->cleanAllocationList(this->csr->peekTaskCount(), NEO::AllocationUsage::TEMPORARY_ALLOCATION);
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatusNonTimestamp() {
    assignKernelEventCompletionData(hostAddress);
    uint32_t queryVal = Event::STATE_CLEARED;
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCheck = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCheck; packetId++) {
            bool ready = NEO::WaitUtils::waitFunctionWithPredicate<const TagSizeT>(
                static_cast<TagSizeT const *>(kernelEventCompletionData[i].getContextStartAddress(packetId)),
                queryVal,
                std::not_equal_to<TagSizeT>());
            if (!ready) {
                return ZE_RESULT_NOT_READY;
            }
        }
    }
    this->csr->getInternalAllocationStorage()->cleanAllocationList(this->csr->peekTaskCount(), NEO::AllocationUsage::TEMPORARY_ALLOCATION);
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatus() {
    TagSizeT *hostAddr = static_cast<TagSizeT *>(hostAddress);

    if (metricStreamer != nullptr) {
        *hostAddr = metricStreamer->getNotificationState();
    }
    this->csr->downloadAllocations();
    if (isEventTimestampFlagSet()) {
        return queryStatusKernelTimestamp();
    } else {
        return queryStatusNonTimestamp();
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValueTimestamps(TagSizeT eventVal) {

    auto baseAddr = castToUint64(hostAddress);

    auto eventTsSetFunc = [&eventVal](auto tsAddr) {
        auto tsptr = reinterpret_cast<void *>(tsAddr);
        memcpy_s(tsptr, sizeof(TagSizeT), static_cast<void *>(&eventVal), sizeof(TagSizeT));
    };
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToSet = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t j = 0; j < packetsToSet; j++) {
            eventTsSetFunc(baseAddr + contextStartOffset);
            eventTsSetFunc(baseAddr + globalStartOffset);
            eventTsSetFunc(baseAddr + contextEndOffset);
            eventTsSetFunc(baseAddr + globalEndOffset);
            baseAddr += singlePacketSize;
        }
    }

    const auto dataSize = 4u * EventPacketsCount::maxKernelSplit * NEO::TimestampPacketSizeControl::preferredPacketCount;
    TagSizeT tagValues[dataSize];

    for (uint32_t index = 0u; index < dataSize; index++) {
        tagValues[index] = eventVal;
    }

    assignKernelEventCompletionData(tagValues);

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValue(TagSizeT eventVal) {
    if (isEventTimestampFlagSet()) {
        return hostEventSetValueTimestamps(eventVal);
    }

    auto packetHostAddr = hostAddress;
    UNRECOVERABLE_IF(packetHostAddr == nullptr);

    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToSet = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t j = 0; j < packetsToSet; j++) {
            memcpy_s(packetHostAddr, sizeof(TagSizeT), static_cast<void *>(&eventVal), sizeof(TagSizeT));
            packetHostAddr = ptrOffset(packetHostAddr, singlePacketSize);
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSignal() {
    return hostEventSetValue(Event::STATE_SIGNALED);
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSynchronize(uint64_t timeout) {
    std::chrono::microseconds elapsedTimeSinceGpuHangCheck{0};
    std::chrono::high_resolution_clock::time_point waitStartTime, lastHangCheckTime, currentTime;
    uint64_t timeDiff = 0;

    ze_result_t ret = ZE_RESULT_NOT_READY;

    if (this->csr->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
    }

    if (timeout == 0) {
        return queryStatus();
    }

    waitStartTime = std::chrono::high_resolution_clock::now();
    lastHangCheckTime = waitStartTime;
    while (true) {
        ret = queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            return ret;
        }

        currentTime = std::chrono::high_resolution_clock::now();
        elapsedTimeSinceGpuHangCheck = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastHangCheckTime);

        if (elapsedTimeSinceGpuHangCheck.count() >= this->gpuHangCheckPeriod.count()) {
            lastHangCheckTime = currentTime;
            if (this->csr->isGpuHangDetected()) {
                return ZE_RESULT_ERROR_DEVICE_LOST;
            }
        }

        if (timeout == std::numeric_limits<uint64_t>::max()) {
            continue;
        }

        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - waitStartTime).count();

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
            kernelEventCompletionData[i].setPacketsUsed(NEO::TimestampPacketSizeControl::preferredPacketCount);
        }
    }
    hostEventSetValue(Event::STATE_INITIAL);
    resetPackets();
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) {

    ze_kernel_timestamp_result_t &result = *dstptr;

    if (queryStatus() != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_NOT_READY;
    }

    assignKernelEventCompletionData(hostAddress);
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
ze_result_t EventImp<TagSizeT>::queryTimestampsExp(Device *device, uint32_t *pCount, ze_kernel_timestamp_result_t *pTimestamps) {
    uint32_t timestampPacket = 0;
    uint64_t globalStartTs, globalEndTs, contextStartTs, contextEndTs;
    globalStartTs = globalEndTs = contextStartTs = contextEndTs = Event::STATE_INITIAL;
    auto deviceImp = static_cast<DeviceImp *>(device);
    bool isStaticPartitioning = true;

    if (NEO::DebugManager.flags.EnableStaticPartitioning.get() == 0) {
        isStaticPartitioning = false;
    }

    if (!isStaticPartitioning) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint32_t numPacketsUsed = 1u;
    if (!deviceImp->isSubdevice) {
        numPacketsUsed = this->getPacketsInUse();
    }

    if ((*pCount == 0) ||
        (*pCount > numPacketsUsed)) {
        *pCount = numPacketsUsed;
        return ZE_RESULT_SUCCESS;
    }

    for (auto i = 0u; i < *pCount; i++) {
        ze_kernel_timestamp_result_t &result = *(pTimestamps + i);

        auto queryTsEventAssignFunc = [&](uint64_t &timestampFieldForWriting, uint64_t &timestampFieldToCopy) {
            memcpy_s(&timestampFieldForWriting, sizeof(uint64_t), static_cast<void *>(&timestampFieldToCopy), sizeof(uint64_t));
        };

        auto packetId = i;
        if (deviceImp->isSubdevice) {
            packetId = static_cast<NEO::SubDevice *>(deviceImp->getNEODevice())->getSubDeviceIndex();
        }

        globalStartTs = kernelEventCompletionData[timestampPacket].getGlobalStartValue(packetId);
        contextStartTs = kernelEventCompletionData[timestampPacket].getContextStartValue(packetId);
        contextEndTs = kernelEventCompletionData[timestampPacket].getContextEndValue(packetId);
        globalEndTs = kernelEventCompletionData[timestampPacket].getGlobalEndValue(packetId);

        queryTsEventAssignFunc(result.global.kernelStart, globalStartTs);
        queryTsEventAssignFunc(result.context.kernelStart, contextStartTs);
        queryTsEventAssignFunc(result.global.kernelEnd, globalEndTs);
        queryTsEventAssignFunc(result.context.kernelEnd, contextEndTs);
    }

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::resetPackets() {
    for (uint32_t i = 0; i < kernelCount; i++) {
        kernelEventCompletionData[i].setPacketsUsed(1);
    }
    kernelCount = 1;
}

template <typename TagSizeT>
uint32_t EventImp<TagSizeT>::getPacketsInUse() {
    uint32_t packetsInUse = 0;
    for (uint32_t i = 0; i < kernelCount; i++) {
        packetsInUse += kernelEventCompletionData[i].getPacketsUsed();
    }
    return packetsInUse;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::setPacketsInUse(uint32_t value) {
    kernelEventCompletionData[getCurrKernelDataIndex()].setPacketsUsed(value);
}

template <typename TagSizeT>
uint64_t EventImp<TagSizeT>::getPacketAddress(Device *device) {
    uint64_t address = getGpuAddress(device);
    for (uint32_t i = 0; i < kernelCount - 1; i++) {
        address += kernelEventCompletionData[i].getPacketsUsed() *
                   singlePacketSize;
    }
    return address;
}

} // namespace L0
