/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/sub_device.h"
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
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {
template <typename TagSizeT>
Event *Event::create(EventPool *eventPool, const ze_event_desc_t *desc, Device *device) {
    auto neoDevice = device->getNEODevice();
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    auto event = new EventImp<TagSizeT>(eventPool, desc->index, device, csr->isTbxMode());
    UNRECOVERABLE_IF(event == nullptr);

    if (eventPool->isEventPoolTimestampFlagSet()) {
        event->setEventTimestampFlag(true);
        event->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, NEO::TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize());
    }
    event->hasKerneMappedTsCapability = eventPool->isEventPoolKerneMappedTsFlagSet();
    auto &hwInfo = neoDevice->getHardwareInfo();

    event->signalAllEventPackets = L0GfxCoreHelper::useSignalAllEventPackets(hwInfo);

    auto alloc = eventPool->getAllocation().getGraphicsAllocation(neoDevice->getRootDeviceIndex());

    uint64_t baseHostAddr = reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer());
    event->totalEventSize = eventPool->getEventSize();
    event->eventPoolOffset = desc->index * event->totalEventSize;
    event->hostAddress = reinterpret_cast<void *>(baseHostAddr + event->eventPoolOffset);
    event->signalScope = desc->signal;
    event->waitScope = desc->wait;
    event->csrs.push_back(csr);
    event->maxKernelCount = eventPool->getMaxKernelCount();
    event->maxPacketCount = eventPool->getEventMaxPackets();
    event->isFromIpcPool = eventPool->getImportedIpcPool();
    if (event->isFromIpcPool || eventPool->isIpcPoolFlagSet()) {
        event->disableImplicitCounterBasedMode();
    }

    event->kernelEventCompletionData =
        std::make_unique<KernelEventCompletionData<TagSizeT>[]>(event->maxKernelCount);

    bool useContextEndOffset = false;
    int32_t overrideUseContextEndOffset = NEO::debugManager.flags.UseContextEndOffsetForEventCompletion.get();
    if (overrideUseContextEndOffset != -1) {
        useContextEndOffset = !!overrideUseContextEndOffset;
    }
    event->setUsingContextEndOffset(useContextEndOffset);

    // do not reset even if it has been imported, since event pool
    // might have been imported after events being already signaled
    if (event->isFromIpcPool == false) {
        event->resetDeviceCompletionData(true);
    }

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

    if (eventPool->isCounterBased() || NEO::debugManager.flags.ForceInOrderEvents.get() == 1) {
        event->enableCounterBasedMode(true);
    }

    auto extendedDesc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);

    bool interruptMode = false;
    bool kmdWaitMode = false;

    if (extendedDesc && (extendedDesc->stype == ZE_INTEL_STRUCTURE_TYPE_EVENT_SYNC_MODE_EXP_DESC)) {
        auto eventSyncModeDesc = reinterpret_cast<const ze_intel_event_sync_mode_exp_desc_t *>(extendedDesc);

        interruptMode = (eventSyncModeDesc->syncModeFlags & ZE_INTEL_EVENT_SYNC_MODE_EXP_FLAG_SIGNAL_INTERRUPT);
        kmdWaitMode = (eventSyncModeDesc->syncModeFlags & ZE_INTEL_EVENT_SYNC_MODE_EXP_FLAG_LOW_POWER_WAIT);
    }

    interruptMode |= (NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.get() == 1);
    kmdWaitMode |= (NEO::debugManager.flags.WaitForUserFenceOnEventHostSynchronize.get() == 1);

    if (kmdWaitMode) {
        event->enableKmdWaitMode();
    }

    if (interruptMode) {
        event->enableInterruptMode();
    }

    return event;
}

template <typename TagSizeT>
EventImp<TagSizeT>::EventImp(EventPool *eventPool, int index, Device *device, bool tbxMode)
    : Event(eventPool, index, device), tbxMode(tbxMode) {
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
        for (auto packetId = 0u; packetId < eventCompletion.getPacketsUsed(); packetId++) {
            if (this->l3FlushAppliedOnKernel.test(kernelId) && ((packetId % skipL3EventPacketIndex) != 0)) {
                continue;
            }
            const std::pair<uint64_t, uint64_t> currentGlobal(eventCompletion.getGlobalStartValue(packetId),
                                                              eventCompletion.getGlobalEndValue(packetId));
            const std::pair<uint64_t, uint64_t> currentContext(eventCompletion.getContextStartValue(packetId),
                                                               eventCompletion.getContextEndValue(packetId));

            globalStartTS = std::min(globalStartTS, currentGlobal.first);
            contextStartTS = std::min(contextStartTS, currentContext.first);
            globalEndTS = getEndTS(isGlobalTsOverflowed, currentGlobal, globalEndTS);
            contextEndTS = getEndTS(isContextTsOverflowed, currentContext, contextEndTS);
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
ze_result_t EventImp<TagSizeT>::queryCounterBasedEventStatus() {
    if (!this->inOrderExecInfo.get()) {
        return ZE_RESULT_NOT_READY;
    }

    const uint64_t *hostAddress = ptrOffset(inOrderExecInfo->getBaseHostAddress(), this->inOrderAllocationOffset);
    auto waitValue = getInOrderExecSignalValueWithSubmissionCounter();
    bool signaled = true;

    for (uint32_t i = 0; i < inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
        if (!NEO::WaitUtils::waitFunctionWithPredicate<const uint64_t>(hostAddress, waitValue, std::greater_equal<uint64_t>())) {
            signaled = false;
            break;
        }

        hostAddress = ptrOffset(hostAddress, sizeof(uint64_t));
    }

    if (!signaled) {
        return ZE_RESULT_NOT_READY;
    }

    handleSuccessfulHostSynchronization();

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
void EventImp<TagSizeT>::handleSuccessfulHostSynchronization() {
    if (this->tbxMode) {
        for (auto &csr : csrs) {
            csr->downloadAllocations();
        }
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
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::queryStatusEventPackets() {
    assignKernelEventCompletionData(this->hostAddress);
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
                std::not_equal_to<TagSizeT>());
            if (!ready) {
                return ZE_RESULT_NOT_READY;
            }
        }
    }
    if (this->signalAllEventPackets) {
        if (packets < getMaxPacketsCount()) {
            uint32_t remainingPackets = getMaxPacketsCount() - packets;
            auto remainingPacketSyncAddress = ptrOffset(this->hostAddress, packets * this->singlePacketSize);
            remainingPacketSyncAddress = ptrOffset(remainingPacketSyncAddress, this->getCompletionFieldOffset());
            for (uint32_t i = 0; i < remainingPackets; i++) {
                void const *queryAddress = remainingPacketSyncAddress;
                bool ready = NEO::WaitUtils::waitFunctionWithPredicate<const TagSizeT>(
                    static_cast<TagSizeT const *>(queryAddress),
                    queryVal,
                    std::not_equal_to<TagSizeT>());
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
bool EventImp<TagSizeT>::handlePreQueryStatusOperationsAndCheckCompletion() {
    if (metricNotification != nullptr) {
        hostEventSetValue(metricNotification->getNotificationState());
    }
    if (this->tbxMode) {
        auto &allEngines = this->device->getNEODevice()->getAllEngines();

        bool downloadedAllocation = false;
        bool downloadedInOrdedAllocation = false;

        for (auto const &engine : allEngines) {
            const auto &csr = engine.commandStreamReceiver;
            if (!downloadedAllocation) {
                if (auto &alloc = this->getAllocation(this->device); alloc.isUsedByOsContext(csr->getOsContext().getContextId())) {
                    csr->downloadAllocation(alloc);
                    downloadedAllocation = true;
                }
            }

            if (!downloadedInOrdedAllocation && inOrderExecInfo) {
                auto alloc = inOrderExecInfo->isHostStorageDuplicated() ? inOrderExecInfo->getHostCounterAllocation() : &inOrderExecInfo->getDeviceCounterAllocation();

                if (alloc->isUsedByOsContext(csr->getOsContext().getContextId())) {
                    csr->downloadAllocation(*alloc);
                    downloadedInOrdedAllocation = true;
                }
            }

            if (downloadedAllocation && downloadedInOrdedAllocation) {
                break;
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
ze_result_t EventImp<TagSizeT>::hostEventSetValueTimestamps(TagSizeT eventVal) {

    auto baseHostAddr = this->hostAddress;
    auto baseGpuAddr = getAllocation(device).getGpuAddress();

    uint64_t timestampStart = static_cast<uint64_t>(eventVal);
    uint64_t timestampEnd = static_cast<uint64_t>(eventVal);
    if (eventVal == Event::STATE_SIGNALED) {
        timestampStart = static_cast<uint64_t>(this->gpuStartTimestamp);
        timestampEnd = static_cast<uint64_t>(this->gpuEndTimestamp);
    }

    uint32_t packets = 0;
    for (uint32_t i = 0; i < this->kernelCount; i++) {
        uint32_t packetsToSet = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t j = 0; j < packetsToSet; j++, packets++) {
            if (castToUint64(baseHostAddr) >= castToUint64(ptrOffset(this->hostAddress, totalEventSize))) {
                break;
            }
            copyDataToEventAlloc(ptrOffset(baseHostAddr, contextStartOffset), baseGpuAddr + contextStartOffset, sizeof(TagSizeT), timestampStart);
            copyDataToEventAlloc(ptrOffset(baseHostAddr, globalStartOffset), baseGpuAddr + globalStartOffset, sizeof(TagSizeT), timestampStart);
            copyDataToEventAlloc(ptrOffset(baseHostAddr, contextEndOffset), baseGpuAddr + contextEndOffset, sizeof(TagSizeT), timestampEnd);
            copyDataToEventAlloc(ptrOffset(baseHostAddr, globalEndOffset), baseGpuAddr + globalEndOffset, sizeof(TagSizeT), timestampEnd);

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
void EventImp<TagSizeT>::copyDataToEventAlloc(void *dstHostAddr, uint64_t dstGpuVa, size_t copySize, const uint64_t &copyData) {
    memcpy_s(dstHostAddr, copySize, &copyData, copySize);

    if (this->tbxMode) {
        auto &alloc = getAllocation(device);
        constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();
        alloc.setTbxWritable(true, allBanks);

        auto offset = ptrDiff(dstGpuVa, alloc.getGpuAddress());

        csrs[0]->writeMemory(alloc, true, offset, copySize);

        alloc.setTbxWritable(true, allBanks);
    }
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostEventSetValue(TagSizeT eventVal) {
    UNRECOVERABLE_IF(hostAddress == nullptr);

    if (isEventTimestampFlagSet()) {
        return hostEventSetValueTimestamps(eventVal);
    }

    auto basePacketHostAddr = getCompletionFieldHostAddress();
    auto basePacketGpuAddr = getCompletionFieldGpuAddress(device);

    UNRECOVERABLE_IF(sizeof(TagSizeT) > sizeof(uint64_t));

    uint32_t packets = 0;

    std::array<uint64_t, 16 * 4 * 3> tempCopyData = {}; // 16 packets, 4 timestamps, 3 kernels
    UNRECOVERABLE_IF(tempCopyData.size() * sizeof(uint64_t) < totalEventSize);

    const auto numElements = getMaxPacketsCount() * kernelCount;
    std::fill_n(tempCopyData.begin(), numElements, static_cast<uint64_t>(eventVal));

    auto packetHostAddr = basePacketHostAddr;
    auto packetGpuAddr = basePacketGpuAddr;

    size_t totalSizeToCopy = 0;

    for (uint32_t i = 0; i < kernelCount; i++) {
        uint32_t packetsToSet = kernelEventCompletionData[i].getPacketsUsed();
        for (uint32_t j = 0; j < packetsToSet; j++, packets++) {
            if (castToUint64(packetHostAddr) >= castToUint64(ptrOffset(this->hostAddress, totalEventSize))) {
                break;
            }

            packetHostAddr = ptrOffset(packetHostAddr, this->singlePacketSize);
            packetGpuAddr = ptrOffset(packetGpuAddr, this->singlePacketSize);

            totalSizeToCopy += this->singlePacketSize;
        }
    }

    copyDataToEventAlloc(basePacketHostAddr, basePacketGpuAddr, totalSizeToCopy, tempCopyData[0]);

    if (this->signalAllEventPackets) {
        setRemainingPackets(eventVal, packetGpuAddr, packetHostAddr, packets);
    }

    return ZE_RESULT_SUCCESS;
}

template <typename TagSizeT>
ze_result_t EventImp<TagSizeT>::hostSignal() {
    if (this->isCounterBased()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto status = hostEventSetValue(Event::STATE_SIGNALED);
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
        return ZE_RESULT_NOT_READY;
    }

    uint64_t waitAddress = castToUint64(ptrOffset(inOrderExecInfo->getBaseHostAddress(), this->inOrderAllocationOffset));

    if (!csrs[0]->waitUserFence(getInOrderExecSignalValueWithSubmissionCounter(), waitAddress, timeout)) {
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

    if (this->csrs[0]->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
    }

    if (NEO::debugManager.flags.OverrideEventSynchronizeTimeout.get() != -1) {
        timeout = NEO::debugManager.flags.OverrideEventSynchronizeTimeout.get();
    }

    waitStartTime = std::chrono::high_resolution_clock::now();
    lastHangCheckTime = waitStartTime;
    do {
        if (isKmdWaitModeEnabled() && isCounterBased()) {
            ret = waitForUserFence(timeout);
        } else {
            ret = queryStatus();
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

    auto &gfxCoreHelper = device->getNEODevice()->getGfxCoreHelper();
    auto &hwInfo = device->getNEODevice()->getHardwareInfo();
    const auto resolution = device->getNEODevice()->getDeviceInfo().profilingTimerResolution;
    auto deviceTsInNs = gfxCoreHelper.getGpuTimeStampInNS(referenceTs.gpuTimeStamp, resolution);
    const auto maxKernelTsValue = maxNBitValue(hwInfo.capabilityTable.kernelTimestampValidBits);

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

    const auto &referenceHostTsInNs = referenceTs.cpuTimeinNS;

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
            offset += static_cast<uint64_t>(maxNBitValue(gfxCoreHelper.getGlobalTimeStampBits()) * resolution);
            startTimeStampInNs = static_cast<uint64_t>(deviceTs->kernelStart * resolution) + offset;
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

    ze_result_t status = queryTimestampsExp(device, pCount, pResults->pKernelTimestampsBuffer);

    if (status == ZE_RESULT_SUCCESS && hasKerneMappedTsCapability) {
        getSynchronizedKernelTimestamps(pResults->pSynchronizedTimestampsBuffer, *pCount, pResults->pKernelTimestampsBuffer);
    }
    return status;
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
            copyDataToEventAlloc(nextPacketAddress, nextPacketGpuVa, sizeof(TagSizeT), copyData);
            nextPacketAddress = ptrOffset(nextPacketAddress, this->singlePacketSize);
            nextPacketGpuVa = ptrOffset(nextPacketGpuVa, this->singlePacketSize);
        }
    }
}

} // namespace L0
