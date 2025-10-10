/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t BcsSplit::appendSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
                                      const BcsSplitParams::CopyParams &copyParams,
                                      size_t size,
                                      ze_event_handle_t hSignalEvent,
                                      uint32_t numWaitEvents,
                                      ze_event_handle_t *phWaitEvents,
                                      bool performMigration,
                                      bool hasRelaxedOrderingDependencies,
                                      NEO::TransferDirection direction,
                                      size_t estimatedCmdBufferSize,
                                      AppendCallFuncT<gfxCoreFamily> appendCall) {

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    const auto aggregatedEventsMode = this->events.aggregatedEventsMode;
    auto signalEvent = Event::fromHandle(hSignalEvent);

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto cmdListsForSplit = this->getCmdListsForSplit(direction, size);
    auto engineCount = cmdListsForSplit.size();
    size_t markerEventIndex = 0;
    uint64_t aggregatedEventIncrementVal = 1;

    const bool useSignalEventForSubcopy = aggregatedEventsMode && cmdList->isUsingAdditionalBlitProperties() && Event::isAggregatedEvent(signalEvent) &&
                                          (signalEvent->getInOrderIncrementValue(1) % engineCount == 0);

    if (useSignalEventForSubcopy) {
        aggregatedEventIncrementVal = signalEvent->getInOrderIncrementValue(1) / engineCount;
    } else {
        auto markerEventIndexRet = this->events.obtainForSplit(Context::fromHandle(cmdList->getCmdListContext()), maxEventCountInPool<GfxFamily>);
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

        auto lock = subCmdList->getCsr(false)->obtainUniqueOwnership();

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

        BcsSplitParams::CopyParams localCopyParams;

        std::visit([&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            localCopyParams = T{ptrOffset(arg.dst, size - totalSize),
                                ptrOffset(arg.src, size - totalSize)};
        },
                   copyParams);

        auto copyEventIndex = aggregatedEventsMode ? markerEventIndex : subcopyEventIndex + i;
        auto eventHandle = useSignalEventForSubcopy ? signalEvent : this->events.subcopy[copyEventIndex]->toHandle();
        result = appendCall(subCmdList, localCopyParams, localSize, eventHandle, aggregatedEventIncrementVal);
        subCmdList->flushImmediate(result, true, !hasRelaxedOrderingDependencies, hasRelaxedOrderingDependencies, NEO::AppendOperations::nonKernel, false, nullptr, true, &lock, nullptr);

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
        std::lock_guard<std::mutex> lock(events.mtx);
        cmdList->assignInOrderExecInfoToEvent(this->events.marker[markerEventIndex]);
    }

    return result;
}

} // namespace L0