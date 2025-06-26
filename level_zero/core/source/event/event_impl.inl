/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/wait_util.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/driver_experimental/zex_common.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {
template <typename TagSizeT>
Event *Event::create(const EventDescriptor &eventDescriptor, Device *device, ze_result_t &result) {
    auto neoDevice = device->getNEODevice();
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto &hwInfo = neoDevice->getHardwareInfo();

    auto event = std::make_unique<EventImp<TagSizeT>>(eventDescriptor.index, device, csr->isTbxMode());
    UNRECOVERABLE_IF(!event.get());

    event->eventPoolAllocation = eventDescriptor.eventPoolAllocation;

    if (eventDescriptor.timestampPool) {
        event->setEventTimestampFlag(true);
        event->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }
    event->hasKernelMappedTsCapability = eventDescriptor.kernelMappedTsPoolFlag;

    event->signalAllEventPackets = L0GfxCoreHelper::useSignalAllEventPackets(hwInfo);

    void *baseHostAddress = 0;
    if (event->eventPoolAllocation) {
        baseHostAddress = event->eventPoolAllocation->getGraphicsAllocation(neoDevice->getRootDeviceIndex())->getUnderlyingBuffer();
    }

    event->totalEventSize = eventDescriptor.totalEventSize;
    event->eventPoolOffset = eventDescriptor.index * event->totalEventSize;
    event->offsetInSharedAlloc = eventDescriptor.offsetInSharedAlloc;
    event->hostAddressFromPool = ptrOffset(baseHostAddress, event->eventPoolOffset + event->offsetInSharedAlloc);
    event->signalScope = eventDescriptor.signalScope;

    if (NEO::debugManager.flags.ForceHostSignalScope.get() == 1) {
        event->signalScope |= ZE_EVENT_SCOPE_FLAG_HOST;
    }
    if (NEO::debugManager.flags.ForceHostSignalScope.get() == 0) {
        event->signalScope &= ~ZE_EVENT_SCOPE_FLAG_HOST;
    }

    event->waitScope = eventDescriptor.waitScope;
    event->csrs.push_back(csr);
    event->maxKernelCount = eventDescriptor.maxKernelCount;
    event->maxPacketCount = eventDescriptor.maxPacketsCount;
    event->isFromIpcPool = eventDescriptor.importedIpcPool;
    if ((event->isFromIpcPool || eventDescriptor.ipcPool) && (eventDescriptor.counterBasedFlags == 0)) {
        event->disableImplicitCounterBasedMode();
    }

    event->kernelEventCompletionData = std::make_unique<KernelEventCompletionData<TagSizeT>[]>(event->maxKernelCount);

    bool useContextEndOffset = false;
    int32_t overrideUseContextEndOffset = NEO::debugManager.flags.UseContextEndOffsetForEventCompletion.get();
    if (overrideUseContextEndOffset != -1) {
        useContextEndOffset = !!overrideUseContextEndOffset;
    }
    event->setUsingContextEndOffset(useContextEndOffset);

    const auto frequency = device->getNEODevice()->getDeviceInfo().profilingTimerResolution;
    const auto maxKernelTsValue = maxNBitValue(hwInfo.capabilityTable.kernelTimestampValidBits);
    if (hwInfo.capabilityTable.kernelTimestampValidBits < 64u) {
        event->timestampRefreshIntervalInNanoSec = static_cast<uint64_t>(maxKernelTsValue * frequency) / 2;
    } else {
        event->timestampRefreshIntervalInNanoSec = maxKernelTsValue / 2;
    }
    if (NEO::debugManager.flags.EventTimestampRefreshIntervalInMilliSec.get() != -1) {
        constexpr uint32_t milliSecondsToNanoSeconds = 1000000u;
        const uint32_t refreshTime = NEO::debugManager.flags.EventTimestampRefreshIntervalInMilliSec.get();
        event->timestampRefreshIntervalInNanoSec = refreshTime * milliSecondsToNanoSeconds;
    }

    if (eventDescriptor.counterBasedFlags != 0 || NEO::debugManager.flags.ForceInOrderEvents.get() == 1) {
        event->enableCounterBasedMode(true, eventDescriptor.counterBasedFlags);
        if (eventDescriptor.ipcPool) {
            event->isSharableCounterBased = true;
        }
    }

    // do not reset even if it has been imported, since event pool
    // might have been imported after events being already signaled
    if (event->isFromIpcPool == false) {
        event->resetDeviceCompletionData(true);
    }

    result = event->enableExtensions(eventDescriptor);

    if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
    }

    return event.release();
}

template <typename TagSizeT>
Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    EventDescriptor eventDescriptor = {
        .eventPoolAllocation = &eventPool->getAllocation(),
        .extensions = desc->pNext,
        .totalEventSize = eventPool->getEventSize(),
        .maxKernelCount = eventPool->getMaxKernelCount(),
        .maxPacketsCount = eventPool->getEventMaxPackets(),
        .counterBasedFlags = eventPool->getCounterBasedFlags(),
        .index = desc->index,
        .signalScope = desc->signal,
        .waitScope = desc->wait,
        .timestampPool = eventPool->isEventPoolTimestampFlagSet(),
        .kernelMappedTsPoolFlag = eventPool->isEventPoolKernelMappedTsFlagSet(),
        .importedIpcPool = eventPool->getImportedIpcPool(),
        .ipcPool = eventPool->isIpcPoolFlagSet(),
    };

    if (eventPool->getCounterBasedFlags() != 0) {
        eventDescriptor.eventPoolAllocation = nullptr;
    }

    if (eventPool->getSharedTimestampAllocation()) {
        eventDescriptor.offsetInSharedAlloc = eventPool->getSharedTimestampAllocation()->getOffset();
    }

    ze_result_t result = ZE_RESULT_SUCCESS;

    Event *event = Event::create<TagSizeT>(eventDescriptor, device, result);
    UNRECOVERABLE_IF(event == nullptr);
    event->setEventPool(eventPool);
    return event;
}

template <typename TagSizeT>
EventImp<TagSizeT>::EventImp(int index, Device *device, bool tbxMode)
    : Event(index, device), tbxMode(tbxMode) {
    contextStartOffset = NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getContextStartOffset();
    contextEndOffset = NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getContextEndOffset();
    globalStartOffset = NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getGlobalStartOffset();
    globalEndOffset = NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getGlobalEndOffset();
    timestampSizeInDw = (sizeof(TagSizeT) / sizeof(uint32_t));
    singlePacketSize = device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset();
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::calculateProfilingData() {
    constexpr uint32_t skipL3EventPacketIndex = 2u;
    globalStartTS = kernelEventCompletionData[0].getGlobalStartValue(0);
    globalEndTS = kernelEventCompletionData[0].getGlobalEndValue(0);
    contextStartTS = kernelEventCompletionData[0].getContextStartValue(0);
    contextEndTS = kernelEventCompletionData[0].getContextEndValue(0);

    auto getEndTS = [](bool &isOverflowed, const std::pair<uint64_t, uint64_t> &currTs, const uint64_t &end) {
        auto &[currStartTs, currEndTs] = currTs;

        if (isOverflowed == false) {
            if (currEndTs < currStartTs) {
                isOverflowed = true;
                return currEndTs;
            } else {
                return std::max(end, currEndTs);
            }
        } else {
            // if already overflowed, then track the endTs of new overflowing ones
            if (currEndTs < currStartTs) {
                return std::max(end, currEndTs);
            }
        }
        return end;
    };

    bool isGlobalTsOverflowed = false;
    bool isContextTsOverflowed = false;

    for (uint32_t kernelId = 0; kernelId < kernelCount; kernelId++) {
        const auto &eventCompletion = kernelEventCompletionData[kernelId];

        auto numPackets = eventCompletion.getPacketsUsed();
        if (inOrderIncrementValue > 0) {
            numPackets *= static_cast<uint32_t>(inOrderTimestampNode.size());
        }

        if (additionalTimestampNode.size() > 0) {
            numPackets += static_cast<uint32_t>(additionalTimestampNode.size());
        }

        for (auto packetId = 0u; packetId < numPackets; packetId++) {
            if (this->l3FlushAppliedOnKernel.test(kernelId) && ((packetId % skipL3EventPacketIndex) != 0)) {
                continue;
            }
            const std::pair<uint64_t, uint64_t> currentGlobal(eventCompletion.getGlobalStartValue(packetId),
                                                              eventCompletion.getGlobalEndValue(packetId));
            const std::pair<uint64_t, uint64_t> currentContext(eventCompletion.getContextStartValue(packetId),
                                                               eventCompletion.getContextEndValue(packetId));

            auto calculatedGlobalEndTs = getEndTS(isGlobalTsOverflowed, currentGlobal, globalEndTS);
            auto calculatedContextEndTs = getEndTS(isContextTsOverflowed, currentContext, contextEndTS);
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintTimestampPacketContents.get(), stdout, "kernel id: %d, packet: %d, globalStartTS: %llu, globalEndTS: %llu, contextStartTS: %llu, contextEndTS: %llu\n",
                               kernelId, packetId, currentGlobal.first, calculatedGlobalEndTs, currentContext.first, calculatedContextEndTs);

            globalStartTS = std::min(globalStartTS, currentGlobal.first);
            contextStartTS = std::min(contextStartTS, currentContext.first);
            globalEndTS = calculatedGlobalEndTs;
            contextEndTS = calculatedContextEndTs;
        }
    }
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::clearTimestampTagData(uint32_t partitionCount, NEO::TagNodeBase *newNode) {
    auto node = newNode;
    auto hostAddress = node->getCpuBase();
    auto deviceAddress = node->getGpuAddress();

    const std::array<TagSizeT, 4> data = {Event::STATE_INITIAL, Event::STATE_INITIAL, Event::STATE_INITIAL, Event::STATE_INITIAL};
    constexpr size_t copySize = data.size() * sizeof(TagSizeT);

    for (uint32_t i = 0; i < partitionCount; i++) {
        copyDataToEventAlloc(hostAddress, deviceAddress, copySize, data.data());

        hostAddress = ptrOffset(hostAddress, singlePacketSize);
        deviceAddress += singlePacketSize;
    }
}

template <typename TagSizeT>
void EventImp<TagSizeT>::assignKernelEventCompletionData(void *address) {
    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToCopy = kernelEventCompletionData[i].getPacketsUsed();
        if (inOrderIncrementValue > 0) {
            packetsToCopy *= static_cast<uint32_t>(inOrderTimestampNode.size());
        }

        uint32_t nodeId = 0;

        for (uint32_t packetId = 0; packetId < packetsToCopy; packetId++) {
            if (inOrderIncrementValue > 0 && (packetId % kernelEventCompletionData[i].getPacketsUsed() == 0)) {
                address = inOrderTimestampNode[nodeId++]->getCpuBase();
            }

            kernelEventCompletionData[i].assignDataToAllTimestamps(packetId, address);
            address = ptrOffset(address, singlePacketSize);
        }

        // Account for additional timestamp nodes
        uint32_t remainingPackets = 0;
        if (!additionalTimestampNode.empty()) {
            remainingPackets = kernelEventCompletionData[i].getPacketsUsed();
            if (inOrderIncrementValue > 0) {
                remainingPackets *= static_cast<uint32_t>(additionalTimestampNode.size());
            }
        }

        if (remainingPackets == 0) {
            continue;
        }

        nodeId = 0;
        uint32_t normalizedPacketId = 0;
        for (uint32_t packetId = packetsToCopy; packetId < packetsToCopy + remainingPackets; packetId++) {
            if (normalizedPacketId % kernelEventCompletionData[i].getPacketsUsed() == 0) {
                address = additionalTimestampNode[nodeId++]->getCpuBase();
            }

            kernelEventCompletionData[i].assignDataToAllTimestamps(packetId, address);
            address = ptrOffset(address, singlePacketSize);
            normalizedPacketId++;
        }
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryCounterBasedEventStatus() {
    if (!this->inOrderExecInfo.get()) {
        return reportEmptyCbEventAsReady ? ZE_RESULT_SUCCESS : ZE_RESULT_NOT_READY;
    }

    auto waitValue = getInOrderExecSignalValueWithSubmissionCounter();

    if (!inOrderExecInfo->isCounterAlreadyDone(waitValue)) {
        bool signaled = true;

        if (this->isCounterBased() && !this->inOrderTimestampNode.empty() && !this->device->getCompilerProductHelper().isHeaplessModeEnabled(this->device->getHwInfo())) {
            this->synchronizeTimestampCompletionWithTimeout();
            signaled = this->isTimestampPopulated();
        } else {
            const uint64_t *hostAddress = ptrOffset(inOrderExecInfo->getBaseHostAddress(), this->inOrderAllocationOffset);
            for (uint32_t i = 0; i < inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
                if (!NEO::WaitUtils::waitFunctionWithPredicate<const uint64_t>(hostAddress, waitValue, std::greater_equal<uint64_t>(), 0)) {
                    signaled = false;
                    break;
                }

                hostAddress = ptrOffset(hostAddress, device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            }
        }

        if (!signaled) {
            return ZE_RESULT_NOT_READY;
        }
        inOrderExecInfo->setLastWaitedCounterValue(waitValue);
    }

    handleSuccessfulHostSynchronization();

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
TaskCountType EventImp<TagSizeT>::getTaskCount(const NEO::CommandStreamReceiver &csr) const {
    auto contextId = csr.getOsContext().getContextId();

    TaskCountType taskCount = getAllocation(this->device) ? getAllocation(this->device)->getTaskCount(contextId) : 0;

    if (inOrderExecInfo) {
        if (inOrderExecInfo->getDeviceCounterAllocation()) {
            taskCount = std::max(taskCount, inOrderExecInfo->getDeviceCounterAllocation()->getTaskCount(contextId));
        } else {
            DEBUG_BREAK_IF(true); // external allocation - not able to download
        }
    }

    return taskCount;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::downloadAllTbxAllocations() {
    for (auto &csr : csrs) {
        auto taskCount = getTaskCount(*csr);
        if (taskCount == NEO::GraphicsAllocation::objectNotUsed) {
            taskCount = csr->peekLatestFlushedTaskCount();
        }
        csr->downloadAllocations(true, taskCount);
    }

    for (auto &engine : this->device->getNEODevice()->getMemoryManager()->getRegisteredEngines()[this->device->getRootDeviceIndex()]) {
        if (!engine.commandStreamReceiver->isInitialized()) {
            continue;
        }
        auto taskCount = getTaskCount(*engine.commandStreamReceiver);

        if (taskCount != NEO::GraphicsAllocation::objectNotUsed) {
            engine.commandStreamReceiver->downloadAllocations(false, taskCount);
        }
    }
}

template <typename TagSizeT>
void EventImp<TagSizeT>::handleSuccessfulHostSynchronization() {
    csrs[0]->pollForAubCompletion();

    if (this->tbxMode) {
        downloadAllTbxAllocations();
    }
    this->setIsCompleted();
    unsetCmdQueue();

    if (!isCounterBased()) {
        // Temporary assignment. If in-order CmdList required to use Event allocation for HW commands chaining, we need to wait for the counter.
        // After successful host synchronization, we can unset CL counter.
        unsetInOrderExecInfo();
    }
    for (auto &csr : csrs) {
        csr->getInternalAllocationStorage()->cleanAllocationList(csr->peekTaskCount(), NEO::AllocationUsage::TEMPORARY_ALLOCATION);
    }

    releaseTempInOrderTimestampNodes();
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatusEventPackets() {
    assignKernelEventCompletionData(getHostAddress());
    uint32_t queryVal = Event::STATE_CLEARED;
    uint32_t packets = 0;
    for (uint32_t i = 0; i < this->kernelCount; i++) {
        uint32_t packetsToCheck = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t packetId = 0; packetId < packetsToCheck; packetId++, packets++) {
            void const *queryAddress = isUsingContextEndOffset()
                                           ? kernelEventCompletionData[i].getContextEndAddress(packetId)
                                           : kernelEventCompletionData[i].getContextStartAddress(packetId);
            bool ready = NEO::WaitUtils::waitFunctionWithPredicate<const TagSizeT>(
                static_cast<TagSizeT const *>(queryAddress),
                queryVal,
                std::not_equal_to<TagSizeT>(),
                0);
            if (!ready) {
                return ZE_RESULT_NOT_READY;
            }
        }
    }
    if (this->signalAllEventPackets) {
        if (packets < getMaxPacketsCount()) {
            uint32_t remainingPackets = getMaxPacketsCount() - packets;
            auto remainingPacketSyncAddress = ptrOffset(getHostAddress(), packets * this->singlePacketSize);
            remainingPacketSyncAddress = ptrOffset(remainingPacketSyncAddress, this->getCompletionFieldOffset());
            for (uint32_t i = 0; i < remainingPackets; i++) {
                void const *queryAddress = remainingPacketSyncAddress;
                bool ready = NEO::WaitUtils::waitFunctionWithPredicate<const TagSizeT>(
                    static_cast<TagSizeT const *>(queryAddress),
                    queryVal,
                    std::not_equal_to<TagSizeT>(),
                    0);
                if (!ready) {
                    return ZE_RESULT_NOT_READY;
                }
                remainingPacketSyncAddress = ptrOffset(remainingPacketSyncAddress, this->singlePacketSize);
            }
        }
    }

    handleSuccessfulHostSynchronization();

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
bool EventImp<TagSizeT>::tbxDownload(NEO::CommandStreamReceiver &csr, bool &downloadedAllocation, bool &downloadedInOrdedAllocation) {
    if (!downloadedAllocation) {
        if (auto &alloc = *this->getAllocation(this->device); alloc.isUsedByOsContext(csr.getOsContext().getContextId())) {
            csr.downloadAllocation(alloc);
            downloadedAllocation = true;
        }
    }

    if (!downloadedInOrdedAllocation) {
        auto alloc = inOrderExecInfo->isHostStorageDuplicated() ? inOrderExecInfo->getHostCounterAllocation() : inOrderExecInfo->getDeviceCounterAllocation();

        if (alloc->isUsedByOsContext(csr.getOsContext().getContextId())) {
            csr.downloadAllocation(*alloc);
            downloadedInOrdedAllocation = true;
        }
    }

    if (downloadedAllocation && downloadedInOrdedAllocation) {
        return false;
    }

    return true;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::tbxDownload(NEO::Device &device, bool &downloadedAllocation, bool &downloadedInOrdedAllocation) {
    for (auto const &engine : device.getAllEngines()) {
        if (!engine.commandStreamReceiver->isInitialized()) {
            continue;
        }
        if (!tbxDownload(*engine.commandStreamReceiver, downloadedAllocation, downloadedInOrdedAllocation)) {
            break;
        }
    }

    for (auto &csr : device.getSecondaryCsrs()) {
        if (!csr->isInitialized()) {
            continue;
        }
        if (!tbxDownload(*csr, downloadedAllocation, downloadedInOrdedAllocation)) {
            break;
        }
    }
}

template <typename TagSizeT>
bool EventImp<TagSizeT>::handlePreQueryStatusOperationsAndCheckCompletion() {
    if (metricNotification != nullptr && eventPoolAllocation) {
        hostEventSetValue(metricNotification->getNotificationState());
    }
    if (this->tbxMode) {
        bool downloadedAllocation = (eventPoolAllocation == nullptr);
        bool downloadedInOrdedAllocation = (inOrderExecInfo.get() == nullptr);
        if (inOrderExecInfo && !inOrderExecInfo->getDeviceCounterAllocation()) {
            downloadedInOrdedAllocation = true;
            DEBUG_BREAK_IF(true); //  external allocation - not able to download
        }

        tbxDownload(*this->device->getNEODevice(), downloadedAllocation, downloadedInOrdedAllocation);

        if (!downloadedAllocation || !downloadedInOrdedAllocation) {
            for (auto &subDevice : this->device->getNEODevice()->getRootDevice()->getSubDevices()) {
                tbxDownload(*subDevice, downloadedAllocation, downloadedInOrdedAllocation);
            }
        }
    }

    if (!this->isFromIpcPool && isAlreadyCompleted()) {
        return true;
    }

    return false;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatus() {
    if (handlePreQueryStatusOperationsAndCheckCompletion()) {
        return ZE_RESULT_SUCCESS;
    }

    if (isCounterBased() || this->inOrderExecInfo.get()) {
        return queryCounterBasedEventStatus();
    } else {
        return queryStatusEventPackets();
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValueTimestamps(Event::State eventState) {
    if (isCounterBased() && !getAllocation(this->device)) {
        return ZE_RESULT_SUCCESS;
    }

    auto baseHostAddr = getHostAddress();
    auto baseGpuAddr = getGpuAddress(device);

    auto eventVal = static_cast<TagSizeT>(eventState);
    auto timestampStart = static_cast<TagSizeT>(eventState);
    auto timestampEnd = static_cast<TagSizeT>(eventState);
    if (eventState == Event::STATE_SIGNALED) {
        if (this->gpuStartTimestamp != 0u) {
            timestampStart = static_cast<TagSizeT>(this->gpuStartTimestamp);
        }
        if (this->gpuEndTimestamp != 0u) {
            timestampEnd = static_cast<TagSizeT>(this->gpuEndTimestamp);
        }
    }

    auto hostAddresss = getHostAddress();

    const std::array<TagSizeT, 4> copyData = {{timestampStart, timestampStart, timestampEnd, timestampEnd}};
    constexpr size_t copySize = copyData.size() * sizeof(TagSizeT);

    uint32_t packets = 0;
    for (uint32_t i = 0; i < this->kernelCount; i++) {
        uint32_t packetsToSet = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t j = 0; j < packetsToSet; j++, packets++) {
            if (castToUint64(baseHostAddr) >= castToUint64(ptrOffset(hostAddresss, totalEventSize))) {
                break;
            }

            copyDataToEventAlloc(baseHostAddr, baseGpuAddr, copySize, copyData.data());

            baseHostAddr = ptrOffset(baseHostAddr, singlePacketSize);
            baseGpuAddr += singlePacketSize;
        }
    }
    if (this->signalAllEventPackets) {
        baseHostAddr = ptrOffset(baseHostAddr, this->contextEndOffset);
        baseGpuAddr += this->contextEndOffset;
        setRemainingPackets(eventVal, baseGpuAddr, baseHostAddr, packets);
    }

    const auto dataSize = 4u * EventPacketsCount::maxKernelSplit * NEO::TimestampPacketConstants::preferredPacketCount;
    TagSizeT tagValues[dataSize];

    for (uint32_t index = 0u; index < dataSize; index++) {
        tagValues[index] = eventVal;
    }

    assignKernelEventCompletionData(tagValues);

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::copyTbxData(uint64_t dstGpuVa, size_t copySize) {
    auto alloc = getAllocation(device);
    if (!alloc) {
        DEBUG_BREAK_IF(true);
        return;
    }

    constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();

    if (alloc->isTbxWritable(allBanks)) {
        // initialize full page tables for the first time
        csrs[0]->writeMemory(*alloc, false, 0, 0);
    }

    alloc->setTbxWritable(true, allBanks);

    auto offset = ptrDiff(dstGpuVa, alloc->getGpuAddress());

    csrs[0]->writeMemory(*alloc, true, offset, copySize);

    alloc->setTbxWritable(false, allBanks);
}

template <typename TagSizeT>
void EventImp<TagSizeT>::copyDataToEventAlloc(void *dstHostAddr, uint64_t dstGpuVa, size_t copySize, const void *copyData) {
    memcpy_s(dstHostAddr, copySize, copyData, copySize);

    if (this->tbxMode) {
        copyTbxData(dstGpuVa, copySize);
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValue(Event::State eventState) {
    if (!hostAddressFromPool && this->inOrderTimestampNode.empty()) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    if (isEventTimestampFlagSet()) {
        return hostEventSetValueTimestamps(eventState);
    }

    if (isCounterBased()) {
        return ZE_RESULT_SUCCESS;
    }

    auto eventVal = static_cast<TagSizeT>(eventState);

    auto basePacketHostAddr = getCompletionFieldHostAddress();
    auto basePacketGpuAddr = getCompletionFieldGpuAddress(device);

    UNRECOVERABLE_IF(sizeof(TagSizeT) > sizeof(uint64_t));

    uint32_t packets = 0;

    auto packetHostAddr = basePacketHostAddr;
    auto packetGpuAddr = basePacketGpuAddr;

    size_t totalSizeToCopy = 0;

    auto hostAddresss = getHostAddress();

    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToSet = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t j = 0; j < packetsToSet; j++, packets++) {
            if (castToUint64(packetHostAddr) >= castToUint64(ptrOffset(hostAddresss, totalEventSize))) {
                break;
            }

            packetHostAddr = ptrOffset(packetHostAddr, this->singlePacketSize);
            packetGpuAddr = ptrOffset(packetGpuAddr, this->singlePacketSize);

            totalSizeToCopy += this->singlePacketSize;
        }
    }

    if (packets > 0) {
        const size_t numElements = totalSizeToCopy / sizeof(uint64_t);
        StackVec<uint64_t, 16 * 4 * 3> tempCopyData; // 16 packets, 4 timestamps, 3 kernels
        tempCopyData.reserve(numElements);
        std::fill_n(tempCopyData.begin(), numElements, static_cast<uint64_t>(eventVal));
        copyDataToEventAlloc(basePacketHostAddr, basePacketGpuAddr, totalSizeToCopy, &tempCopyData[0]);
    }

    if (this->signalAllEventPackets) {
        setRemainingPackets(eventVal, packetGpuAddr, packetHostAddr, packets);
    }

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSignal(bool allowCounterBased) {
    if (!allowCounterBased && this->isCounterBased()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t status = ZE_RESULT_SUCCESS;

    if (!isCounterBased() || isEventTimestampFlagSet()) {
        status = hostEventSetValue(Event::STATE_SIGNALED);
    }

    if (status == ZE_RESULT_SUCCESS) {
        this->setIsCompleted();
    }
    return status;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::waitForUserFence(uint64_t timeout) {
    if (handlePreQueryStatusOperationsAndCheckCompletion()) {
        return ZE_RESULT_SUCCESS;
    }

    if (!inOrderExecInfo) {
        return ZE_RESULT_SUCCESS;
    }

    uint64_t waitAddress = castToUint64(ptrOffset(inOrderExecInfo->getBaseHostAddress(), this->inOrderAllocationOffset));

    NEO::GraphicsAllocation *hostAlloc = nullptr;
    if (inOrderExecInfo->isExternalMemoryExecInfo()) {
        hostAlloc = inOrderExecInfo->getExternalHostAllocation();
    } else {
        hostAlloc = inOrderExecInfo->isHostStorageDuplicated() ? inOrderExecInfo->getHostCounterAllocation() : inOrderExecInfo->getDeviceCounterAllocation();
    }

    if (!csrs[0]->waitUserFence(getInOrderExecSignalValueWithSubmissionCounter(), waitAddress, timeout, true, this->externalInterruptId, hostAlloc)) {
        return ZE_RESULT_NOT_READY;
    }

    handleSuccessfulHostSynchronization();

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSynchronize(uint64_t timeout) {
    std::chrono::microseconds elapsedTimeSinceGpuHangCheck{0};
    std::chrono::high_resolution_clock::time_point waitStartTime, lastHangCheckTime, currentTime;
    uint64_t timeDiff = 0;

    ze_result_t ret = ZE_RESULT_NOT_READY;

    if (NEO::debugManager.flags.AbortHostSyncOnNonHostVisibleEvent.get()) {
        UNRECOVERABLE_IF(!this->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST));
    }

    if (this->csrs[0]->getType() == NEO::CommandStreamReceiverType::aub) {
        this->csrs[0]->pollForAubCompletion();
        return ZE_RESULT_SUCCESS;
    }

    if (NEO::debugManager.flags.OverrideEventSynchronizeTimeout.get() != -1) {
        timeout = NEO::debugManager.flags.OverrideEventSynchronizeTimeout.get();
    }

    TaskCountType taskCountToWaitForL3Flush = 0;
    auto &hwInfo = this->device->getHwInfo();
    if (((this->isCounterBased() && !this->inOrderTimestampNode.empty()) || this->mitigateHostVisibleSignal) && this->device->getProductHelper().isDcFlushAllowed() && !this->device->getCompilerProductHelper().isHeaplessModeEnabled(hwInfo)) {
        auto lock = this->csrs[0]->obtainUniqueOwnership();
        this->csrs[0]->flushTagUpdate();
        taskCountToWaitForL3Flush = this->csrs[0]->peekLatestFlushedTaskCount();
    }

    waitStartTime = std::chrono::high_resolution_clock::now();
    lastHangCheckTime = waitStartTime;

    const bool fenceWait = isKmdWaitModeEnabled() && isCounterBased() && csrs[0]->waitUserFenceSupported();

    do {
        if (this->isCounterBased() && !this->inOrderTimestampNode.empty() && !this->device->getCompilerProductHelper().isHeaplessModeEnabled(hwInfo)) {
            synchronizeTimestampCompletionWithTimeout();
            if (this->isTimestampPopulated()) {
                inOrderExecInfo->setLastWaitedCounterValue(getInOrderExecSignalValueWithSubmissionCounter());
                handleSuccessfulHostSynchronization();
                ret = ZE_RESULT_SUCCESS;
            }
        } else {
            if (fenceWait) {
                ret = waitForUserFence(timeout);
            } else {
                ret = queryStatus();
            }
        }
        if (ret == ZE_RESULT_SUCCESS) {
            if (this->getKernelWithPrintfDeviceMutex() != nullptr) {
                std::lock_guard<std::mutex> lock(*this->getKernelWithPrintfDeviceMutex());
                if (!this->getKernelForPrintf().expired()) {
                    this->getKernelForPrintf().lock()->printPrintfOutput(true);
                }
                this->resetKernelForPrintf();
                this->resetKernelWithPrintfDeviceMutex();
            }
            if (device->getNEODevice()->getRootDeviceEnvironment().assertHandler.get()) {
                device->getNEODevice()->getRootDeviceEnvironment().assertHandler->printAssertAndAbort();
            }
            if (NEO::debugManager.flags.ForceGpuStatusCheckOnSuccessfulEventHostSynchronize.get() == 1) {
                const bool hangDetected = this->csrs[0]->isGpuHangDetected();
                if (hangDetected) {
                    return ZE_RESULT_ERROR_DEVICE_LOST;
                }
            }
            if (taskCountToWaitForL3Flush) {
                this->csrs[0]->waitForTaskCount(taskCountToWaitForL3Flush);
            }
            return ret;
        }

        currentTime = std::chrono::high_resolution_clock::now();
        elapsedTimeSinceGpuHangCheck = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastHangCheckTime);

        if (elapsedTimeSinceGpuHangCheck.count() >= this->gpuHangCheckPeriod.count()) {
            lastHangCheckTime = currentTime;
            if (this->csrs[0]->isGpuHangDetected()) {
                if (device->getNEODevice()->getRootDeviceEnvironment().assertHandler.get()) {
                    device->getNEODevice()->getRootDeviceEnvironment().assertHandler->printAssertAndAbort();
                }
                return ZE_RESULT_ERROR_DEVICE_LOST;
            }
        }

        if (timeout == std::numeric_limits<uint64_t>::max()) {
            continue;
        } else if (timeout == 0) {
            break;
        }

        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - waitStartTime).count();

    } while (timeDiff < timeout);

    if (device->getNEODevice()->getRootDeviceEnvironment().assertHandler.get()) {
        device->getNEODevice()->getRootDeviceEnvironment().assertHandler->printAssertAndAbort();
    }
    return ret;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::reset() {
    if (this->counterBasedMode == CounterBasedMode::explicitlyEnabled) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (NEO::debugManager.flags.SynchronizeEventBeforeReset.get() != -1) {
        if (NEO::debugManager.flags.SynchronizeEventBeforeReset.get() == 2 && queryStatus() != ZE_RESULT_SUCCESS) {
            printf("\nzeEventHostReset: Event %p not ready. Calling zeEventHostSynchronize.", this);
        }

        hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    unsetInOrderExecInfo();
    unsetCmdQueue();
    this->resetCompletionStatus();
    this->resetDeviceCompletionData(false);
    this->l3FlushAppliedOnKernel.reset();
    this->resetAdditionalTimestampNode(nullptr, 0, true);
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::resetDeviceCompletionData(bool resetAllPackets) {

    if (resetAllPackets) {
        this->kernelCount = this->maxKernelCount;
        for (uint32_t i = 0; i < kernelCount; i++) {
            this->kernelEventCompletionData[i].setPacketsUsed(NEO::TimestampPacketConstants::preferredPacketCount);
        }
    }

    this->hostEventSetValue(Event::STATE_INITIAL);
    this->resetPackets(resetAllPackets);
}

template <typename TagSizeT>
void EventImp<TagSizeT>::synchronizeTimestampCompletionWithTimeout() {
    std::chrono::high_resolution_clock::time_point startTime = std::chrono::high_resolution_clock::now();
    uint64_t timeDiff = 0;

    do {
        assignKernelEventCompletionData(getHostAddress());
        calculateProfilingData();

        timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
    } while (!isTimestampPopulated() && (timeDiff < completionTimeoutMs));
    DEBUG_BREAK_IF(!isTimestampPopulated());
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryKernelTimestamp(ze_kernel_timestamp_result_t *dstptr) {
    ze_kernel_timestamp_result_t &result = *dstptr;

    if (!this->isCounterBased() || this->inOrderTimestampNode.empty()) {
        if (queryStatus() != ZE_RESULT_SUCCESS) {
            return ZE_RESULT_NOT_READY;
        }
    }

    assignKernelEventCompletionData(getHostAddress());
    calculateProfilingData();

    if (!isTimestampPopulated()) {
        synchronizeTimestampCompletionWithTimeout();
        if (!this->inOrderTimestampNode.empty()) {
            if (!isTimestampPopulated()) {
                return ZE_RESULT_NOT_READY;
            }
        }
    }

    auto eventTsSetFunc = [&](uint64_t &timestampFieldToCopy, uint64_t &timestampFieldForWriting) {
        memcpy_s(&(timestampFieldForWriting), sizeof(uint64_t), static_cast<void *>(&timestampFieldToCopy), sizeof(uint64_t));
    };

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
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
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintCalculatedTimestamps.get(), stdout, "globalStartTS: %llu, globalEndTS: %llu, contextStartTS: %llu, contextEndTS: %llu\n",
                       result.global.kernelStart, result.global.kernelEnd, result.context.kernelStart, result.context.kernelEnd);

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryTimestampsExp(Device *device, uint32_t *count, ze_kernel_timestamp_result_t *timestamps) {
    uint32_t timestampPacket = 0;
    uint64_t globalStartTs, globalEndTs, contextStartTs, contextEndTs;
    globalStartTs = globalEndTs = contextStartTs = contextEndTs = Event::STATE_INITIAL;
    bool isStaticPartitioning = true;

    if (NEO::debugManager.flags.EnableStaticPartitioning.get() == 0) {
        isStaticPartitioning = false;
    }

    if (!isStaticPartitioning) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint32_t numPacketsUsed = this->getPacketsInUse();

    if ((*count == 0) ||
        (*count > numPacketsUsed)) {
        *count = numPacketsUsed;
        return ZE_RESULT_SUCCESS;
    }

    for (auto i = 0u; i < *count; i++) {
        ze_kernel_timestamp_result_t &result = *(timestamps + i);

        auto queryTsEventAssignFunc = [&](uint64_t &timestampFieldForWriting, uint64_t &timestampFieldToCopy) {
            memcpy_s(&timestampFieldForWriting, sizeof(uint64_t), static_cast<void *>(&timestampFieldToCopy), sizeof(uint64_t));
        };

        auto packetId = i;

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
void EventImp<TagSizeT>::getSynchronizedKernelTimestamps(ze_synchronized_timestamp_result_ext_t *pSynchronizedTimestampsBuffer,
                                                         const uint32_t count, const ze_kernel_timestamp_result_t *pKernelTimestampsBuffer) {

    auto &hwInfo = device->getNEODevice()->getHardwareInfo();
    const auto resolution = device->getNEODevice()->getDeviceInfo().profilingTimerResolution;
    const auto maxKernelTsValue = maxNBitValue(hwInfo.capabilityTable.kernelTimestampValidBits);

    const auto numBitsForResolution = Math::log2(static_cast<uint64_t>(resolution)) + 1u;
    const auto clampedBitsCount = std::min(hwInfo.capabilityTable.kernelTimestampValidBits, 64u - numBitsForResolution);
    const auto maxClampedTsValue = maxNBitValue(clampedBitsCount);

    auto convertDeviceTsToNanoseconds = [&resolution, &maxClampedTsValue](uint64_t deviceTs) {
        // Use clamped maximum to avoid overflows
        return static_cast<uint64_t>((deviceTs & maxClampedTsValue) * resolution);
    };

    NEO::TimeStampData *referenceTs = static_cast<NEO::TimeStampData *>(ptrOffset(getHostAddress(), maxPacketCount * singlePacketSize));
    auto deviceTsInNs = convertDeviceTsToNanoseconds(referenceTs->gpuTimeStamp);

    auto getDuration = [&](uint64_t startTs, uint64_t endTs) {
        const uint64_t maxValue = maxKernelTsValue;
        startTs &= maxValue;
        endTs &= maxValue;

        if (startTs > endTs) {
            // Resolve overflows
            return endTs + (maxValue - startTs);
        } else {
            return endTs - startTs;
        }
    };

    const auto &referenceHostTsInNs = referenceTs->cpuTimeinNS;

    // High Level Approach:
    // startTimeStamp = (referenceHostTsInNs - submitDeviceTs) + kernelDeviceTsStart
    // deviceDuration = kernelDeviceTsEnd - kernelDeviceTsStart
    // endTimeStamp = startTimeStamp + deviceDuration

    // Get offset between Device and Host timestamps
    const int64_t tsOffsetInNs = referenceHostTsInNs - deviceTsInNs;

    auto calculateSynchronizedTs = [&](ze_synchronized_timestamp_data_ext_t *synchronizedTs, const ze_kernel_timestamp_data_t *deviceTs) {
        // Add the offset to the kernel timestamp to find the start timestamp on the CPU timescale
        int64_t offset = tsOffsetInNs;
        uint64_t startTimeStampInNs = static_cast<uint64_t>(deviceTs->kernelStart * resolution) + offset;
        if (startTimeStampInNs < referenceHostTsInNs) {
            offset += static_cast<uint64_t>(convertDeviceTsToNanoseconds(maxKernelTsValue));
            startTimeStampInNs = static_cast<uint64_t>(convertDeviceTsToNanoseconds(deviceTs->kernelStart) + offset);
        }

        // Get the kernel timestamp duration
        uint64_t deviceDuration = getDuration(deviceTs->kernelStart, deviceTs->kernelEnd);
        uint64_t deviceDurationNs = static_cast<uint64_t>(deviceDuration * resolution);
        // Add the duration to the startTimeStamp to get the endTimeStamp
        uint64_t endTimeStampInNs = startTimeStampInNs + deviceDurationNs;

        synchronizedTs->kernelStart = startTimeStampInNs;
        synchronizedTs->kernelEnd = endTimeStampInNs;
    };

    for (uint32_t index = 0; index < count; index++) {
        calculateSynchronizedTs(&pSynchronizedTimestampsBuffer[index].global, &pKernelTimestampsBuffer[index].global);

        pSynchronizedTimestampsBuffer[index].context.kernelStart = pSynchronizedTimestampsBuffer[index].global.kernelStart;
        uint64_t deviceDuration = getDuration(pKernelTimestampsBuffer[index].context.kernelStart,
                                              pKernelTimestampsBuffer[index].context.kernelEnd);
        uint64_t deviceDurationNs = static_cast<uint64_t>(deviceDuration * resolution);
        pSynchronizedTimestampsBuffer[index].context.kernelEnd = pSynchronizedTimestampsBuffer[index].context.kernelStart +
                                                                 deviceDurationNs;
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryKernelTimestampsExt(Device *device, uint32_t *pCount, ze_event_query_kernel_timestamps_results_ext_properties_t *pResults) {

    if (*pCount == 0) {
        return queryTimestampsExp(device, pCount, nullptr);
    }

    if (queryStatus() != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_NOT_READY;
    }

    ze_result_t status = queryTimestampsExp(device, pCount, pResults->pKernelTimestampsBuffer);

    if (status == ZE_RESULT_SUCCESS && hasKernelMappedTsCapability) {
        getSynchronizedKernelTimestamps(pResults->pSynchronizedTimestampsBuffer, *pCount, pResults->pKernelTimestampsBuffer);
    }
    return status;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::getEventPool(ze_event_pool_handle_t *phEventPool) {
    *phEventPool = eventPool;
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::getSignalScope(ze_event_scope_flags_t *pSignalScope) {
    *pSignalScope = signalScope;
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::getWaitScope(ze_event_scope_flags_t *pWaitScope) {
    *pWaitScope = waitScope;
    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
uint32_t EventImp<TagSizeT>::getPacketsInUse() const {
    uint32_t packetsInUse = 0;
    for (uint32_t i = 0; i < kernelCount; i++) {
        packetsInUse += kernelEventCompletionData[i].getPacketsUsed();
    }
    return packetsInUse;
}

template <typename TagSizeT>
uint32_t EventImp<TagSizeT>::getPacketsUsedInLastKernel() {
    return kernelEventCompletionData[getCurrKernelDataIndex()].getPacketsUsed();
}

template <typename TagSizeT>
void EventImp<TagSizeT>::setPacketsInUse(uint32_t value) {
    kernelEventCompletionData[getCurrKernelDataIndex()].setPacketsUsed(value);
}

template <typename TagSizeT>
void EventImp<TagSizeT>::resetKernelCountAndPacketUsedCount() {
    for (auto i = 0u; i < this->kernelCount; i++) {
        this->kernelEventCompletionData[i].setPacketsUsed(1);
    }
    this->kernelCount = 1;
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

template <typename TagSizeT>
void EventImp<TagSizeT>::setRemainingPackets(TagSizeT eventVal, uint64_t nextPacketGpuVa, void *nextPacketAddress, uint32_t packetsAlreadySet) {
    const uint64_t copyData = eventVal;

    if (getMaxPacketsCount() > packetsAlreadySet) {
        uint32_t remainingPackets = getMaxPacketsCount() - packetsAlreadySet;
        for (uint32_t i = 0; i < remainingPackets; i++) {
            copyDataToEventAlloc(nextPacketAddress, nextPacketGpuVa, sizeof(TagSizeT), &copyData);
            nextPacketAddress = ptrOffset(nextPacketAddress, this->singlePacketSize);
            nextPacketGpuVa = ptrOffset(nextPacketGpuVa, this->singlePacketSize);
        }
    }
}

} // namespace L0
