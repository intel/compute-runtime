/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t BcsSplit::appendImmediateSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
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

    const auto aggregatedEventsMode = this->events.isAggregatedEventMode();
    auto signalEvent = Event::fromHandle(hSignalEvent);

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto cmdListsForSplit = this->getCmdListsForSplit(direction, size);
    auto engineCount = cmdListsForSplit.size();
    size_t markerEventIndex = 0;

    const bool useSignalEventForSubcopy = aggregatedEventsMode && cmdList->isUsingAdditionalBlitProperties() && Event::isAggregatedEvent(signalEvent) &&
                                          (signalEvent->getInOrderIncrementValue(1) % engineCount == 0);

    if (!useSignalEventForSubcopy) {
        auto markerEventIndexRet = this->events.obtainForImmediateSplit(Context::fromHandle(cmdList->getCmdListContext()), maxEventCountInPool<GfxFamily>);
        if (!markerEventIndexRet.has_value()) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        markerEventIndex = *markerEventIndexRet;
    }

    const uint64_t aggregatedEventIncrementVal = getAggregatedEventIncrementValForSplit(signalEvent, useSignalEventForSubcopy, engineCount);

    auto barrierRequired = !cmdList->isInOrderExecutionEnabled() && cmdList->isBarrierRequired();
    if (barrierRequired) {
        cmdList->appendSignalEvent(this->events.getEventResources().barrier[markerEventIndex]->toHandle(), false);
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
            auto barrierEventHandle = this->events.getEventResources().barrier[markerEventIndex]->toHandle();
            subCmdList->addEventsToCmdList(1u, &barrierEventHandle, nullptr, hasRelaxedOrderingDependencies, false, true, false, false);
        }

        auto copyEventIndex = aggregatedEventsMode ? markerEventIndex : subcopyEventIndex + i;
        auto eventHandle = useSignalEventForSubcopy ? signalEvent : this->events.getEventResources().subcopy[copyEventIndex]->toHandle();

        result = appendSubSplitCommon<gfxCoreFamily>(cmdList, subCmdList, copyParams, size, signalEvent, numWaitEvents, phWaitEvents, eventHandle, aggregatedEventIncrementVal,
                                                     totalSize, engineCount, useSignalEventForSubcopy, (i == 0), appendCall);

        subCmdList->flushImmediate(result, true, !hasRelaxedOrderingDependencies, hasRelaxedOrderingDependencies, NEO::AppendOperations::nonKernel, false, nullptr, true, &lock, nullptr);

        if ((aggregatedEventsMode && i == 0) || !aggregatedEventsMode) {
            eventHandles.push_back(eventHandle);
        }

        if (signalEvent) {
            signalEvent->appendAdditionalCsr(subCmdList->getCsr(false));
        }
    }

    appendPostSubCopySync<gfxCoreFamily>(cmdList, eventHandles, signalEvent, markerEventIndex, useSignalEventForSubcopy, hasRelaxedOrderingDependencies);

    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void BcsSplit::appendPostSubCopySync(CommandListCoreFamily<gfxCoreFamily> *mainCmdList,
                                     StackVec<ze_event_handle_t, 16> &subCopyEvents,
                                     Event *signalEvent,
                                     size_t markerEventIndex,
                                     bool useSignalEventForSubCopy,
                                     bool hasRelaxedOrderingDependencies) {

    const bool dualStreamCopyOffload = mainCmdList->isDualStreamCopyOffloadOperation(mainCmdList->isCopyOffloadEnabled());

    if (useSignalEventForSubCopy && mainCmdList->isInOrderExecutionEnabled()) {
        auto currentCounter = signalEvent->getInOrderExecInfo()->getAggregatedEventUsageCounter();
        auto expectedCounter = currentCounter + signalEvent->getInOrderIncrementValue(1);
        mainCmdList->appendWaitOnInOrderDependency(signalEvent->getInOrderExecInfo(), nullptr,
                                                   expectedCounter,
                                                   signalEvent->getInOrderAllocationOffset(),
                                                   hasRelaxedOrderingDependencies, false, false, false, dualStreamCopyOffload);
    }

    if (!useSignalEventForSubCopy) {
        mainCmdList->addEventsToCmdList(static_cast<uint32_t>(subCopyEvents.size()), subCopyEvents.data(), nullptr, hasRelaxedOrderingDependencies, false, true, false, dualStreamCopyOffload);
    }

    const auto isCopyCmdList = mainCmdList->isCopyOnly(dualStreamCopyOffload);

    if (!useSignalEventForSubCopy && signalEvent) {
        mainCmdList->appendSignalEventPostWalker(signalEvent, nullptr, nullptr, !isCopyCmdList, false, isCopyCmdList);
    }

    if (!events.isAggregatedEventMode()) {
        mainCmdList->appendSignalEventPostWalker(this->events.getEventResources().marker[markerEventIndex].event, nullptr, nullptr, !isCopyCmdList, false, isCopyCmdList);
    }

    if (mainCmdList->isInOrderExecutionEnabled()) {
        mainCmdList->appendSignalInOrderDependencyCounter(signalEvent, dualStreamCopyOffload, false, false, useSignalEventForSubCopy);
    }
    mainCmdList->handleInOrderDependencyCounter(signalEvent, false, dualStreamCopyOffload);

    if (events.isAggregatedEventMode() && !useSignalEventForSubCopy) {
        auto lock = events.obtainLock();
        mainCmdList->assignInOrderExecInfoToEvent(this->events.getEventResources().marker[markerEventIndex].event);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t BcsSplit::appendSubSplitCommon(CommandListCoreFamily<gfxCoreFamily> *mainCmdList,
                                           CommandListCoreFamily<gfxCoreFamily> *subCmdList,
                                           const BcsSplitParams::CopyParams &copyParams,
                                           size_t size,
                                           Event *signalEvent,
                                           uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents,
                                           ze_event_handle_t subCopyOutEventHandle,
                                           uint64_t aggregatedEventIncrementVal,
                                           size_t &totalSize,
                                           size_t &engineCount,
                                           bool useSignalEventForSubcopy,
                                           bool appendStartProfiling,
                                           AppendCallFuncT<gfxCoreFamily> appendCall) {

    if (mainCmdList->hasInOrderDependencies()) {
        auto &inOrderExecInfo = mainCmdList->getInOrderExecInfo();
        subCmdList->appendWaitOnInOrderDependency(inOrderExecInfo, nullptr, inOrderExecInfo->getCounterValue(), inOrderExecInfo->getAllocationOffset(), false, false, false, false, false);
    }

    subCmdList->addEventsToCmdList(numWaitEvents, phWaitEvents, nullptr, false, false, false, false, false);

    if (!useSignalEventForSubcopy && signalEvent && appendStartProfiling) {
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

    if (events.isAggregatedEventMode() && !useSignalEventForSubcopy) {
        subCmdList->getCmdContainer().addToResidencyContainer(Event::fromHandle(subCopyOutEventHandle)->getInOrderExecInfo()->getDeviceCounterAllocation());
    }

    if (useSignalEventForSubcopy) {
        subCmdList->getCmdContainer().addToResidencyContainer(subCmdList->getInOrderExecInfo()->getDeviceCounterAllocation());
        subCmdList->getCmdContainer().addToResidencyContainer(subCmdList->getInOrderExecInfo()->getHostCounterAllocation());
    }

    auto result = appendCall(subCmdList, localCopyParams, localSize, subCopyOutEventHandle, aggregatedEventIncrementVal);

    totalSize -= localSize;
    engineCount--;

    return result;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t BcsSplit::appendRecordedInOrderSplitCall(CommandListCoreFamily<gfxCoreFamily> *cmdList,
                                                     const BcsSplitParams::CopyParams &copyParams,
                                                     size_t size,
                                                     ze_event_handle_t hSignalEvent,
                                                     uint32_t numWaitEvents,
                                                     ze_event_handle_t *phWaitEvents,
                                                     AppendCallFuncT<gfxCoreFamily> appendCall) {

    UNRECOVERABLE_IF(!this->events.isAggregatedEventMode());
    auto signalEvent = Event::fromHandle(hSignalEvent);

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto cmdListsForSplit = cmdList->getRegularCmdListsForSplit(size, splitSettings.perEngineMaxSize, this->cmdLists.size());
    auto engineCount = cmdListsForSplit.size();
    size_t markerEventIndex = 0;

    const bool useSignalEventForSubcopy = Event::isAggregatedEvent(signalEvent) && (signalEvent->getInOrderIncrementValue(1) % engineCount == 0);

    if (!useSignalEventForSubcopy) {
        markerEventIndex = this->events.obtainForRecordedSplit(Context::fromHandle(cmdList->getCmdListContext()));
        cmdList->storeEventsForBcsSplit(&this->events.getEventResources().marker[markerEventIndex]);
    }

    const uint64_t aggregatedEventIncrementVal = getAggregatedEventIncrementValForSplit(signalEvent, useSignalEventForSubcopy, engineCount);

    if (!cmdList->handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto subCopyOutEventHandle = useSignalEventForSubcopy ? signalEvent : this->events.getEventResources().subcopy[markerEventIndex]->toHandle();

    auto totalSize = size;
    for (size_t i = 0; i < cmdListsForSplit.size(); i++) {
        auto subCmdList = static_cast<CommandListCoreFamily<gfxCoreFamily> *>(cmdListsForSplit[i]);

        result = appendSubSplitCommon<gfxCoreFamily>(cmdList, subCmdList, copyParams, size, signalEvent, numWaitEvents, phWaitEvents, subCopyOutEventHandle, aggregatedEventIncrementVal,
                                                     totalSize, engineCount, useSignalEventForSubcopy, (i == 0), appendCall);
    }

    StackVec<ze_event_handle_t, 16> subCopyEvents{subCopyOutEventHandle};
    appendPostSubCopySync<gfxCoreFamily>(cmdList, subCopyEvents, signalEvent, markerEventIndex, useSignalEventForSubcopy, false);

    return result;
}

} // namespace L0