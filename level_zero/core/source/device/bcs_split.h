/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/transfer_direction.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/sku_info/sku_info_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"

#include <functional>
#include <mutex>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {
struct CommandQueue;
struct DeviceImp;

struct BcsSplit {
    template <GFXCORE_FAMILY gfxCoreFamily, typename T, typename K>
    using AppendCallFuncT = std::function<ze_result_t(CommandListCoreFamilyImmediate<gfxCoreFamily> *, T, K, size_t, ze_event_handle_t, uint64_t)>;
    using CsrContainer = StackVec<NEO::CommandStreamReceiver *, 12u>;

    DeviceImp &device;
    uint32_t clientCount = 0u;

    std::mutex mtx;

    struct Events {
        BcsSplit &bcsSplit;

        std::mutex mtx;
        std::vector<EventPool *> pools;
        std::vector<Event *> barrier;
        std::vector<Event *> subcopy;
        std::vector<Event *> marker;
        std::vector<void *> allocsForAggregatedEvents;
        size_t currentAggregatedAllocOffset = 0;
        size_t createdFromLatestPool = 0u;
        bool aggregatedEventsMode = false;

        std::optional<size_t> obtainForSplit(Context *context, size_t maxEventCountInPool);
        size_t obtainAggregatedEventsForSplit(Context *context);
        void resetEventPackage(size_t index);
        void resetAggregatedEventState(size_t index, bool markerCompleted);
        void releaseResources();
        bool allocatePool(Context *context, size_t maxEventCountInPool, size_t neededEvents);
        std::optional<size_t> createFromPool(Context *context, size_t maxEventCountInPool);
        size_t createAggregatedEvent(Context *context);
        uint64_t *getNextAllocationForAggregatedEvent();

        Events(BcsSplit &bcsSplit) : bcsSplit(bcsSplit) {}
    } events;

    std::vector<CommandList *> cmdLists;
    std::vector<CommandList *> h2dCmdLists;
    std::vector<CommandList *> d2hCmdLists;

    template <GFXCORE_FAMILY gfxCoreFamily, typename T, typename K>
    ze_result_t appendSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
                                T dstptr,
                                K srcptr,
                                size_t size,
                                ze_event_handle_t hSignalEvent,
                                uint32_t numWaitEvents,
                                ze_event_handle_t *phWaitEvents,
                                bool performMigration,
                                bool hasRelaxedOrderingDependencies,
                                NEO::TransferDirection direction,
                                size_t estimatedCmdBufferSize,
                                AppendCallFuncT<gfxCoreFamily, T, K> appendCall) {
        constexpr size_t maxEventCountInPool = MemoryConstants::pageSize64k / sizeof(typename CommandListCoreFamilyImmediate<gfxCoreFamily>::GfxFamily::TimestampPacketType);

        const auto aggregatedEventsMode = this->events.aggregatedEventsMode;
        auto signalEvent = Event::fromHandle(hSignalEvent);

        ze_result_t result = ZE_RESULT_SUCCESS;
        auto &cmdListsForSplit = this->getCmdListsForSplit(direction);
        auto engineCount = cmdListsForSplit.size();
        size_t markerEventIndex = 0;
        uint64_t aggregatedEventIncrementVal = 1;

        const bool useSignalEventForSubcopy = aggregatedEventsMode && cmdList->isUsingAdditionalBlitProperties() && Event::isAggregatedEvent(signalEvent) &&
                                              (signalEvent->getInOrderIncrementValue(1) % engineCount == 0);

        if (useSignalEventForSubcopy) {
            aggregatedEventIncrementVal = signalEvent->getInOrderIncrementValue(1) / engineCount;
        } else {
            auto markerEventIndexRet = this->events.obtainForSplit(Context::fromHandle(cmdList->getCmdListContext()), maxEventCountInPool);
            if (!markerEventIndexRet.has_value()) {
                return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
            }
            markerEventIndex = *markerEventIndexRet;
        }

        auto barrierRequired = !cmdList->isInOrderExecutionEnabled() && cmdList->isBarrierRequired();
        if (barrierRequired) {
            cmdList->appendSignalEvent(this->events.barrier[markerEventIndex]->toHandle(), false);
        }

        auto subcopyEventIndex = markerEventIndex * this->cmdLists.size();
        StackVec<ze_event_handle_t, 16> eventHandles;

        if (!cmdList->handleCounterBasedEventOperations(signalEvent, false)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto totalSize = size;
        for (size_t i = 0; i < cmdListsForSplit.size(); i++) {
            auto subCmdList = static_cast<CommandListCoreFamilyImmediate<gfxCoreFamily> *>(cmdListsForSplit[i]);

            subCmdList->checkAvailableSpace(numWaitEvents, hasRelaxedOrderingDependencies, estimatedCmdBufferSize, false);

            if (barrierRequired) {
                auto barrierEventHandle = this->events.barrier[markerEventIndex]->toHandle();
                subCmdList->addEventsToCmdList(1u, &barrierEventHandle, nullptr, hasRelaxedOrderingDependencies, false, true, false, false);
            }

            if (cmdList->hasInOrderDependencies()) {
                auto &inOrderExecInfo = cmdList->getInOrderExecInfo();
                subCmdList->appendWaitOnInOrderDependency(inOrderExecInfo, nullptr, inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset(), hasRelaxedOrderingDependencies, false, false, false, false);
            }
            subCmdList->addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, hasRelaxedOrderingDependencies, false, false, false, false);

            if (!useSignalEventForSubcopy && signalEvent && i == 0u) {
                subCmdList->appendEventForProfilingAllWalkers(signalEvent, nullptr, nullptr, true, true, false, true);
            }

            auto localSize = totalSize / engineCount;
            auto localDstPtr = ptrOffset(dstptr, size - totalSize);
            auto localSrcPtr = ptrOffset(srcptr, size - totalSize);

            auto copyEventIndex = aggregatedEventsMode ? markerEventIndex : subcopyEventIndex + i;
            auto eventHandle = useSignalEventForSubcopy ? signalEvent : this->events.subcopy[copyEventIndex]->toHandle();
            result = appendCall(subCmdList, localDstPtr, localSrcPtr, localSize, eventHandle, aggregatedEventIncrementVal);
            subCmdList->flushImmediate(result, true, !hasRelaxedOrderingDependencies, hasRelaxedOrderingDependencies, NEO::AppendOperations::nonKernel, false, nullptr, true, nullptr, nullptr);

            if ((aggregatedEventsMode && i == 0) || !aggregatedEventsMode) {
                eventHandles.push_back(eventHandle);
            }

            totalSize -= localSize;
            engineCount--;

            if (signalEvent) {
                signalEvent->appendAdditionalCsr(subCmdList->getCsr(false));
            }
        }

        const bool dualStreamCopyOffload = cmdList->isDualStreamCopyOffloadOperation(cmdList->isCopyOffloadEnabled());

        cmdList->addEventsToCmdList(static_cast<uint32_t>(eventHandles.size()), eventHandles.data(), nullptr, hasRelaxedOrderingDependencies, false, true, false, dualStreamCopyOffload);

        const auto isCopyCmdList = cmdList->isCopyOnly(dualStreamCopyOffload);

        if (!useSignalEventForSubcopy && signalEvent) {
            cmdList->appendSignalEventPostWalker(signalEvent, nullptr, nullptr, !isCopyCmdList, false, isCopyCmdList);
        }

        if (!aggregatedEventsMode) {
            cmdList->appendSignalEventPostWalker(this->events.marker[markerEventIndex], nullptr, nullptr, !isCopyCmdList, false, isCopyCmdList);
        }

        if (cmdList->isInOrderExecutionEnabled()) {
            cmdList->appendSignalInOrderDependencyCounter(signalEvent, dualStreamCopyOffload, false, false, useSignalEventForSubcopy);
        }
        cmdList->handleInOrderDependencyCounter(signalEvent, false, dualStreamCopyOffload);

        if (aggregatedEventsMode && !useSignalEventForSubcopy) {
            cmdList->assignInOrderExecInfoToEvent(this->events.marker[markerEventIndex]);
        }

        return result;
    }

    bool setupDevice(NEO::CommandStreamReceiver *csr, bool copyOffloadEnabled);
    void releaseResources();
    std::vector<CommandList *> &getCmdListsForSplit(NEO::TransferDirection direction);
    void setupEnginesMask(NEO::BcsSplitSettings &settings);
    bool setupQueues(const NEO::BcsSplitSettings &settings);

    BcsSplit(DeviceImp &device) : device(device), events(*this){};
};

} // namespace L0