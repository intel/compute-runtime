/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/helpers/pnext.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/driver_experimental/zex_cmdlist.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace L0 {

namespace {

constexpr bool isAppendKernelCommand(CaptureApi api) {
    return (api == CaptureApi::zeCommandListAppendLaunchKernel) ||
           (api == CaptureApi::zeCommandListAppendLaunchKernelWithParameters) ||
           (api == CaptureApi::zeCommandListAppendLaunchKernelWithArguments) ||
           (api == CaptureApi::zeCommandListAppendLaunchCooperativeKernel) ||
           (api == CaptureApi::zeCommandListAppendLaunchKernelIndirect);
}

template <CaptureApi api>
inline void updateSignalEventForClosure(Closure<api> &closure, ze_event_handle_t signalEvent) {
    if constexpr (HasHSignalEvent<typename Closure<api>::ApiArgs>) {
        closure.apiArgs.hSignalEvent = signalEvent;
    } else if constexpr (api == CaptureApi::zeCommandListAppendSignalEvent) {
        closure.apiArgs.hEvent = signalEvent;
    }
}

template <CaptureApi api>
inline void updateWaitEventsForClosure(Closure<api> &closure, ClosureExternalStorage &externalStorage, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if constexpr (HasPhWaitEvents<typename Closure<api>::ApiArgs>) {
        closure.apiArgs.numWaitEvents = numWaitEvents;
        closure.indirectArgs.waitEvents = externalStorage.registerEventsList(phWaitEvents, phWaitEvents + numWaitEvents);
    } else if constexpr (HasPhEvents<typename Closure<api>::ApiArgs> && (false == HasPhWaitEvents<typename Closure<api>::ApiArgs>)) {
        closure.apiArgs.numEvents = numWaitEvents;
        closure.indirectArgs.waitEvents = externalStorage.registerEventsList(phWaitEvents, phWaitEvents + numWaitEvents);
    }
}

template <CaptureApi api>
inline void updateKernelHandleForClosure(Closure<api> &closure, ClosureExternalStorage &externalStorage, ze_kernel_handle_t kernelHandle) {
    if constexpr (isAppendKernelCommand(api)) {
        auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(kernelHandle));
        closure.apiArgs.kernelHandle = kernelHandle;
        closure.indirectArgs.capturedKernel = kernel->makeDependentClone();
        if constexpr (api == CaptureApi::zeCommandListAppendLaunchKernelWithArguments) {
            // as per MCL spec - kernel arguments are invalidated when swapping kernel object
            auto &explicitArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;
            closure.indirectArgs.argumentsId = externalStorage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), nullptr);
        }
    }
}

template <CaptureApi api>
inline void updateKernelArgumentForClosure(Closure<api> &closure, ClosureExternalStorage &externalStorage, uint32_t argIndex, size_t argSize, const void *argValue) {
    if constexpr (isAppendKernelCommand(api)) {
        closure.indirectArgs.capturedKernel->setArgumentValue(argIndex, argSize, argValue);
        if constexpr (api == CaptureApi::zeCommandListAppendLaunchKernelWithArguments) {
            auto *kernelHandle = closure.indirectArgs.capturedKernel.get();
            auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(kernelHandle));
            auto &explicitArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;
            auto arguments = externalStorage.getKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), closure.indirectArgs.argumentsId);
            memcpy_s(arguments[argIndex], explicitArgs[argIndex].getTraits().argByValSize, argValue, argSize);
        }
    }
}

template <CaptureApi api>
inline void updateKernelGlobalOffsetForClosure(Closure<api> &closure, const uint32_t *globalOffset) {
    if constexpr (isAppendKernelCommand(api)) {
        closure.indirectArgs.capturedKernel->setGlobalOffsetExp(globalOffset[0], globalOffset[1], globalOffset[2]);
    }
}

template <CaptureApi api>
inline void updateKernelGroupSizeForClosure(Closure<api> &closure, const uint32_t *groupSize) {
    if constexpr (isAppendKernelCommand(api)) {
        if constexpr (api == CaptureApi::zeCommandListAppendLaunchKernelWithArguments) {
            closure.apiArgs.groupSizes = {groupSize[0], groupSize[1], groupSize[2]};
        } else {
            closure.indirectArgs.capturedKernel->setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
        }
    }
}

template <CaptureApi api>
inline void updateKernelGroupCountForClosure(Closure<api> &closure, const ze_group_count_t *groupCount) {
    if constexpr (isAppendKernelCommand(api)) {
        if constexpr (api == CaptureApi::zeCommandListAppendLaunchKernelIndirect) {
            // uses indirect group count buffer instead
        } else if constexpr (api == CaptureApi::zeCommandListAppendLaunchKernelWithParameters) {
            closure.indirectArgs.groupCounts = *groupCount;
        } else if constexpr (api == CaptureApi::zeCommandListAppendLaunchKernelWithArguments) {
            closure.apiArgs.groupCounts = *groupCount;
        } else {
            closure.indirectArgs.launchKernelArgs = *groupCount;
        }
    }
}

} // namespace

GraphInstatiateSettings::GraphInstatiateSettings(void *pNext, bool multiEngineGraph) {
    UNRECOVERABLE_IF(nullptr != pNext);

    this->forkPolicy = multiEngineGraph ? ForkPolicy::ForkPolicyMonolythicLevels : ForkPolicy::ForkPolicyFlat;
    int32_t overrideForceGraphForkPolicy = NEO::debugManager.flags.ForceGraphForkPolicy.get();
    if (overrideForceGraphForkPolicy != -1) {
        this->forkPolicy = static_cast<ForkPolicy>(overrideForceGraphForkPolicy);
    }
}

void RecordedApiCommands::updateKernelArgument(MclCommandId commandId, uint32_t argIndex, size_t argSize, const void *argValue) {
    auto *cmd = getCommandByMclId(commandId);
    if (nullptr == cmd) {
        return;
    }
    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                                                                     \
    case CaptureApi::X: {                                                                                                                                      \
        updateKernelArgumentForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), this->externalStorage, argIndex, argSize, argValue); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void RecordedApiCommands::updateKernelGroupCount(MclCommandId commandId, const ze_group_count_t *pGroupCount) {
    auto *cmd = getCommandByMclId(commandId);
    if ((nullptr == cmd) || (nullptr == pGroupCount)) {
        return;
    }

    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                                \
    case CaptureApi::X: {                                                                                                 \
        updateKernelGroupCountForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), pGroupCount); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void RecordedApiCommands::updateKernelGroupSize(MclCommandId commandId, const uint32_t *groupSize) {
    auto *cmd = getCommandByMclId(commandId);
    if ((nullptr == cmd) || (nullptr == groupSize)) {
        return;
    }

    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                             \
    case CaptureApi::X: {                                                                                              \
        updateKernelGroupSizeForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), groupSize); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void RecordedApiCommands::updateKernelGlobalOffset(MclCommandId commandId, const uint32_t *globalOffset) {
    auto *cmd = getCommandByMclId(commandId);
    if (nullptr == cmd) {
        return;
    }

    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                                   \
    case CaptureApi::X: {                                                                                                    \
        updateKernelGlobalOffsetForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), globalOffset); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void RecordedApiCommands::updateSignalEvent(MclCommandId commandId, ze_event_handle_t signalEvent) {
    auto *cmd = getCommandByMclId(commandId);
    if (nullptr == cmd) {
        return;
    }

    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                           \
    case CaptureApi::X: {                                                                                            \
        updateSignalEventForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), signalEvent); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void RecordedApiCommands::updateWaitEvents(MclCommandId commandId, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    auto *cmd = getCommandByMclId(commandId);
    if (nullptr == cmd) {
        return;
    }

    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                                                                 \
    case CaptureApi::X: {                                                                                                                                  \
        updateWaitEventsForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), this->externalStorage, numWaitEvents, phWaitEvents); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void RecordedApiCommands::updateKernel(MclCommandId commandId, ze_kernel_handle_t kernelHandle) {
    auto *cmd = getCommandByMclId(commandId);
    if ((nullptr == cmd) || (nullptr == kernelHandle)) {
        return;
    }

    switch (static_cast<CaptureApi>(cmd->index())) {
#define RR_CAPTURED_API(X)                                                                                                                    \
    case CaptureApi::X: {                                                                                                                     \
        updateKernelHandleForClosure<CaptureApi::X>(std::get<static_cast<size_t>(CaptureApi::X)>(*cmd), this->externalStorage, kernelHandle); \
    } break;
        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
    default:
        break;
    }
}

void Graph::setCaptureTargetRecursively(bool attach) {
    if (this->captureSrc != nullptr) {
        if (attach) {
            if (this->captureSrc->getGraphCaptureTarget() == nullptr) {
                this->captureSrc->setGraphCaptureTarget(this);
            }
        } else {
            if (this->captureSrc->getGraphCaptureTarget() == this) {
                this->captureSrc->releaseGraphCaptureTarget();
            }
        }
    }

    for (auto &subGraph : this->subGraphs) {
        subGraph->setCaptureTargetRecursively(attach);
    }
}

void Graph::setRecordedSignalsRecursively(bool attach) {
    auto *recordedSignalFrom = attach ? this->captureSrc : nullptr;
    for (const auto &recordedSignal : this->recordedSignals) {
        recordedSignal.first->setRecordedSignalFrom(recordedSignalFrom);
    }

    for (auto &subGraph : this->subGraphs) {
        subGraph->setRecordedSignalsRecursively(attach);
    }
}

Graph::~Graph() {
    this->unregisterSignallingEvents();
    for (auto *sg : subGraphs) {
        if (false == sg->wasPreallocated()) {
            delete sg;
        }
    }

    for (const auto &clb : this->destructorCallbacks) {
        clb.pfnCallback(clb.pUserData);
    }
}

void Graph::startCapturingFrom(L0::CommandList &captureSrc, bool isSubGraph) {
    this->captureSrc = &captureSrc;
    if (false == isSubGraph) {
        this->primaryCaptureSrc = &captureSrc;
    }
    this->captureSrc->setGraphCaptureTarget(this);
    captureSrc.getDeviceHandle(&this->captureTargetDesc.hDevice);
    this->captureTargetDesc.desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    this->captureTargetDesc.desc.pNext = nullptr;
    this->captureTargetDesc.desc.flags = captureSrc.getCmdListFlags();
    this->captureTargetDesc.desc.commandQueueGroupOrdinal = captureSrc.getOrdinal();
    if (isSubGraph) {
        this->executionTarget = &captureSrc;
    }
    this->multiEngineGraph = captureSrc.getDevice()->getGfxCoreHelper().getContextGroupContextsCount() > 1;
}

void Graph::stopCapturing() {
    this->wasCapturingStopped = true;
    this->orderedCommands->close();
    if (nullptr == this->captureSrc) {
        return;
    }
    this->unregisterSignallingEvents();
    this->captureSrc->releaseGraphCaptureTarget();
    this->captureSrc = nullptr;
    StackVec<std::pair<L0::CommandList *, ForkInfo>, 1> neverJoinedForks; // should stay empty for valid graphs
    for (auto &unjFork : this->unjoinedForks) {
        auto forkCmdId = unjFork.second.forkSignalCommandId;
        auto potentialJoin = this->potentialJoins.find(forkCmdId);
        if (this->potentialJoins.end() == potentialJoin) {
            neverJoinedForks.push_back({unjFork.first, unjFork.second});
            continue; // no join-like sequences found
        }
        auto potentialJoinSignalId = potentialJoin->second.joinSignalCommandId;
        if (false == potentialJoin->second.forkDestiny->isValidJoinCommand(potentialJoinSignalId)) {
            neverJoinedForks.push_back({unjFork.first, unjFork.second});
            continue; // join-like sequence found but is succeeded by unjoined commands
        }
        resolvedJoins[potentialJoin->second.forkDestiny] = potentialJoin->second;
    }
    this->unjoinedForks.clear();
    this->unjoinedForks.insert(neverJoinedForks.begin(), neverJoinedForks.end());
    for (auto &segment : segments) {
        orderedCommands->registerSegment(segment);
    }
    for (auto &subGraph : subGraphs) {
        subGraph->stopCapturing();
    }
}

ze_result_t Graph::pauseCapturing() {
    if (nullptr == this->captureSrc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (this->captureSrc->getGraphCaptureTarget() != this) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    this->setCaptureTargetRecursively(false);
    this->setRecordedSignalsRecursively(false);

    return ZE_RESULT_SUCCESS;
}

ze_result_t Graph::resumeCapturing() {
    if (nullptr == this->captureSrc) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (this->captureSrc->getGraphCaptureTarget() != nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    this->setCaptureTargetRecursively(true);
    this->setRecordedSignalsRecursively(true);
    return ZE_RESULT_SUCCESS;
}

void Graph::tryJoinOnNextCommand(L0::CommandList &childCmdList, L0::Event &joinEvent) {
    auto forkInfo = this->unjoinedForks.find(&childCmdList);
    if (this->unjoinedForks.end() == forkInfo) {
        return;
    }

    ForkJoinInfo forkJoinInfo = {};
    forkJoinInfo.forkSignalCommandId = forkInfo->second.forkSignalCommandId;
    forkJoinInfo.forkEvent = forkInfo->second.forkEvent;
    forkJoinInfo.joinWaitCommandId = static_cast<CapturedCommandId>(this->recordedApiCommands.size());
    forkJoinInfo.joinEvent = &joinEvent;
    forkJoinInfo.forkDestiny = childCmdList.getGraphCaptureTarget();
    auto joinRecordedSignal = forkJoinInfo.forkDestiny->recordedSignals.find(&joinEvent);
    UNRECOVERABLE_IF(forkJoinInfo.forkDestiny->recordedSignals.end() == joinRecordedSignal);
    forkJoinInfo.joinSignalCommandId = joinRecordedSignal->second;
    this->potentialJoins[forkInfo->second.forkSignalCommandId] = forkJoinInfo;
}

void Graph::forkTo(L0::CommandList &childCmdList, Graph *&child, L0::Event &forkEvent) {
    UNRECOVERABLE_IF(child || childCmdList.getGraphCaptureTarget()); // should not be capturing already
    child = new Graph(this->ctx, false, this->orderedCommands.share());
    child->parentGraph = this;
    child->startCapturingFrom(childCmdList, true);
    childCmdList.setGraphCaptureTarget(child);
    this->subGraphs.push_back(child);

    auto forkEventInfo = this->recordedSignals.find(&forkEvent);
    UNRECOVERABLE_IF(this->recordedSignals.end() == forkEventInfo);
    this->unjoinedForks[&childCmdList] = ForkInfo{.forkSignalCommandId = forkEventInfo->second,
                                                  .forkEvent = &forkEvent};
}

void Graph::registerSignallingEventFromPreviousCommand(L0::Event &ev) {
    ev.setRecordedSignalFrom(this->captureSrc);
    this->recordedSignals[&ev] = static_cast<CapturedCommandId>(this->recordedApiCommands.size() - 1);
}

void Graph::unregisterSignallingEvents() {
    for (auto ev : this->recordedSignals) {
        ev.first->setRecordedSignalFrom(nullptr);
    }
}

template <typename ContainerT>
auto getOptionalData(ContainerT &container) {
    return container.empty() ? nullptr : container.data();
}

inline L0::CommandList *resolveExecutionTargetForInstantiate(L0::CommandList *executionTarget, ze_command_list_handle_t hCommandList) {
    if (nullptr != executionTarget) {
        return executionTarget;
    }
    return L0::CommandList::fromHandle(hCommandList);
}

void handleExternalCbEvent(L0::Event *event, CbExternalEventInstantiateContext &cbEventContext) {
    if (event && event->isExternalEvent()) {
        cbEventContext.cbEventInfoContainer->addCbEventInfo(event, cbEventContext.executorCommandList);
    }
}

template <CaptureApi api, typename IndirectArgsT>
EventParams getEffectiveEventParams(const typename Closure<api>::ApiArgs &apiArgs,
                                    IndirectArgsT &indirectArgs,
                                    ClosureExternalStorage &externalStorage,
                                    std::optional<EventParams> enforcedEvents) {
    if (enforcedEvents.has_value()) {
        return *enforcedEvents;
    }

    auto waitEventsList = getClosureWaitEventsList<api>(apiArgs, indirectArgs, externalStorage);
    return EventParams{
        .hSignalEvent = getClosureSignalEvent<api>(apiArgs),
        .numWaitEvents = static_cast<uint32_t>(waitEventsList.size()),
        .phWaitEvents = waitEventsList.empty() ? nullptr : waitEventsList.data()};
}

void ExternalCbEventInfoContainer::attachExternalCbEventsToExecutableGraph() {
    for (auto &info : storage) {
        info.event->updateInOrdeState(info.inOrderExecEventHelper);
        auto &currentPreambleData = getPreambleData(info.executorCommandList);
        info.event->getInOrderExecEventHelper().assignPatchPreambleData(currentPreambleData.counter(),
                                                                        currentPreambleData.hostAddress(),
                                                                        currentPreambleData.deviceAddress(),
                                                                        currentPreambleData.allocation());
    }
}
void ExternalCbEventInfoContainer::finalizeExecutorContainer() {
    for (auto &info : storage) {
        auto it = getExecutorInfo(info.executorCommandList);
        if (it != executorStorage.end()) {
            *it = {0, nullptr, 0, nullptr, info.executorCommandList};
        } else {
            executorStorage.push_back({0, nullptr, 0, nullptr, info.executorCommandList});
        }
    }
}

void ExternalCbEventInfoContainer::updateExecutorContainer(L0::CommandList *currentRoot) {
    uint64_t *hostAddress = nullptr;
    uint64_t counter = 0;
    uint64_t deviceAddress = 0;
    NEO::GraphicsAllocation *allocation = nullptr;
    for (auto &elem : executorStorage) {
        L0::CommandList *currentKey = elem.key;
        if (currentKey == nullptr) {
            currentRoot->getPatchPreambleFullData(counter, hostAddress, deviceAddress, allocation);
        } else {
            currentKey->getPatchPreambleFullData(counter, hostAddress, deviceAddress, allocation);
        }
        elem = {counter, hostAddress, deviceAddress, allocation, currentKey};
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopy>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryCopy>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryCopy(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendBarrier>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendBarrier>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendBarrier(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendWaitOnEvents>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    return zeCommandListAppendWaitOnEvents(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), eventParams.numWaitEvents, eventParams.phWaitEvents);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendWriteGlobalTimestamp(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.dstptr, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryRangesBarrier>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryRangesBarrier(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.numRanges, getOptionalData(indirectArgs.rangeSizes), const_cast<const void **>(getOptionalData(indirectArgs.ranges)),
                                                         eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryFill>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryFill>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryFill(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size,
                                                eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryCopyRegion>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryCopyRegion(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.dstptr, externalStorage.getCopyRegion(indirectArgs.dstRegion), apiArgs.dstPitch, apiArgs.dstSlicePitch,
                                                      apiArgs.srcptr, externalStorage.getCopyRegion(indirectArgs.srcRegion), apiArgs.srcPitch, apiArgs.srcSlicePitch,
                                                      eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryCopyFromContext>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryCopyFromContext(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.dstptr, apiArgs.hContextSrc, apiArgs.srcptr, apiArgs.size,
                                                           eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopy>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendImageCopy>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendImageCopy(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.hDstImage, apiArgs.hSrcImage,
                                               eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyRegion>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendImageCopyRegion>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendImageCopyRegion(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.hDstImage, apiArgs.hSrcImage, externalStorage.getImageRegion(indirectArgs.dstRegion), externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                     eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemory>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendImageCopyToMemory>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendImageCopyToMemory(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                       apiArgs.dstptr,
                                                       apiArgs.hSrcImage,
                                                       externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                       eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendImageCopyFromMemory>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendImageCopyFromMemory(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                         apiArgs.hDstImage,
                                                         apiArgs.srcptr,
                                                         externalStorage.getImageRegion(indirectArgs.dstRegion),
                                                         eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryPrefetch>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    return zeCommandListAppendMemoryPrefetch(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                             apiArgs.ptr,
                                             apiArgs.size);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemAdvise>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    return zeCommandListAppendMemAdvise(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                        apiArgs.hDevice,
                                        apiArgs.ptr,
                                        apiArgs.size,
                                        apiArgs.advice);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalEvent>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendSignalEvent>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendSignalEvent(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), eventParams.hSignalEvent);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendEventReset>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    return zeCommandListAppendEventReset(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.hEvent);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendQueryKernelTimestamps(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                           apiArgs.numEvents,
                                                           const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.events)),
                                                           apiArgs.dstptr,
                                                           getOptionalData(indirectArgs.offsets),
                                                           eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendSignalExternalSemaphoreExt(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                                apiArgs.numSemaphores,
                                                                const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
                                                                const_cast<ze_external_semaphore_signal_params_ext_t *>(&indirectArgs.signalParams),
                                                                eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendWaitExternalSemaphoreExt(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                              apiArgs.numSemaphores,
                                                              const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
                                                              const_cast<ze_external_semaphore_wait_params_ext_t *>(&indirectArgs.waitParams),
                                                              eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendImageCopyToMemoryExt(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                          apiArgs.dstptr,
                                                          apiArgs.hSrcImage,
                                                          externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                          apiArgs.destRowPitch,
                                                          apiArgs.destSlicePitch,
                                                          eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendImageCopyFromMemoryExt(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList),
                                                            apiArgs.hDstImage,
                                                            apiArgs.srcptr,
                                                            externalStorage.getImageRegion(indirectArgs.dstRegion),
                                                            apiArgs.srcRowPitch,
                                                            apiArgs.srcSlicePitch,
                                                            eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricStreamerMarker>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    auto *target = resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList);
    return target->appendMetricStreamerMarker(apiArgs.hMetricStreamer, apiArgs.value);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricQueryBegin>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    auto *target = resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList);
    return target->appendMetricQueryBegin(apiArgs.hMetricQuery);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricQueryEnd>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zetCommandListAppendMetricQueryEnd>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto *target = resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList);
    auto result = target->appendMetricQueryEnd(apiArgs.hMetricQuery, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricMemoryBarrier>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    auto *target = resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList);
    return target->appendMetricMemoryBarrier();
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMarkerExp>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    return L0::metricAppendMarker(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.hMetricGroup, apiArgs.value);
}

Closure<CaptureApi::zeCommandListAppendLaunchKernel>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->launchKernelArgs = *apiArgs.launchKernelArgs;
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernel>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendLaunchKernel>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernel(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), kernelHandle, &indirectArgs.launchKernelArgs, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->launchKernelArgs = *apiArgs.launchKernelArgs;
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchCooperativeKernel(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), kernelHandle, &indirectArgs.launchKernelArgs, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendLaunchKernelIndirect>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernelIndirect(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), kernelHandle, apiArgs.launchArgsBuffer, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->capturedKernels.reserve(apiArgs.numKernels);
    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.phKernels[i]));
        this->capturedKernels.emplace_back(kernel->makeDependentClone().release());
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    std::vector<ze_kernel_handle_t> phKernelClones(apiArgs.numKernels);
    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        phKernelClones[i] = this->indirectArgs.capturedKernels[i].get();
    }
    auto result = zeCommandListAppendLaunchMultipleKernelsIndirect(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.numKernels, phKernelClones.data(), apiArgs.pCountBuffer, apiArgs.launchArgsBuffer, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->groupCounts = *apiArgs.pGroupCounts;
    this->pNext = nullptr;

    externalStorage.lastResult = CommandList::cloneAppendKernelExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
    if (externalStorage.lastResult == ZE_RESULT_SUCCESS) {
        auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
        this->capturedKernel = kernel->makeDependentClone();
    }
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendKernelExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernelWithParameters(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), kernelHandle, &indirectArgs.groupCounts, indirectArgs.pNext, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->pNext = nullptr;

    externalStorage.lastResult = CommandList::cloneAppendKernelExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
    if (externalStorage.lastResult == ZE_RESULT_SUCCESS) {
        auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
        externalStorage.lastResult = CommandList::setKernelState(kernel, apiArgs.groupSizes, apiArgs.pArguments);
        if (externalStorage.lastResult == ZE_RESULT_SUCCESS) {
            auto &explicitArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;
            this->argumentsId = externalStorage.registerKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), apiArgs.pArguments);

            this->capturedKernel = kernel->makeDependentClone();
        }
    }
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendKernelExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(kernelHandle));
    auto &explicitArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;
    auto arguments = externalStorage.getKernelArguments(std::span(explicitArgs.begin(), explicitArgs.end()), this->indirectArgs.argumentsId);
    auto result = zeCommandListAppendLaunchKernelWithArguments(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), kernelHandle, apiArgs.groupCounts, apiArgs.groupSizes, arguments.data(), this->indirectArgs.pNext, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->pNext = nullptr;

    externalStorage.lastResult = CommandList::cloneAppendMemoryCopyExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
}

Closure<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendMemoryCopyExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryCopyWithParameters(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, indirectArgs.pNext, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->pNext = nullptr;

    externalStorage.lastResult = CommandList::cloneAppendMemoryCopyExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
}

Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendMemoryCopyExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = L0::zexCommandListAppendMemoryCopyWithParameters(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, indirectArgs.pNext, eventParams.numWaitEvents, eventParams.phWaitEvents, eventParams.hSignalEvent);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zeCommandListAppendMemoryFillWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    pattern.resize(apiArgs.patternSize);
    memcpy_s(pattern.data(), pattern.size(), apiArgs.pattern, apiArgs.patternSize);

    this->pNext = nullptr;

    externalStorage.lastResult = CommandList::cloneAppendMemoryCopyExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
}

Closure<CaptureApi::zeCommandListAppendMemoryFillWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendMemoryCopyExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryFillWithParameters>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendMemoryFillWithParameters>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = zeCommandListAppendMemoryFillWithParameters(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size, indirectArgs.pNext, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    pattern.resize(apiArgs.patternSize);
    memcpy_s(pattern.data(), pattern.size(), apiArgs.pattern, apiArgs.patternSize);

    this->pNext = nullptr;

    externalStorage.lastResult = CommandList::cloneAppendMemoryCopyExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
}

Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendMemoryCopyExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zexCommandListAppendMemoryFillWithParameters>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = L0::zexCommandListAppendMemoryFillWithParameters(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size, indirectArgs.pNext, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendHostFunction>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &cbEventContext, std::optional<EventParams> enforcedEvents) const {
    auto eventParams = getEffectiveEventParams<CaptureApi::zeCommandListAppendHostFunction>(apiArgs, indirectArgs, externalStorage, enforcedEvents);
    auto result = L0::zeCommandListAppendHostFunction(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), apiArgs.pHostFunction, apiArgs.pUserData, apiArgs.pNext, eventParams.hSignalEvent, eventParams.numWaitEvents, eventParams.phWaitEvents);
    handleExternalCbEvent(L0::Event::fromHandle(eventParams.hSignalEvent), cbEventContext);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListGetNextCommandIdExp>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    uint64_t commandId = indirectArgs.commandId;
    return zeCommandListGetNextCommandIdExp(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), &indirectArgs.desc, &commandId);
}

ze_result_t Closure<CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp>::instantiateTo(L0::CommandList *executionTarget, ClosureExternalStorage &externalStorage, CbExternalEventInstantiateContext &, std::optional<EventParams>) const {
    uint64_t commandId = indirectArgs.commandId;
    auto kernelHandles = indirectArgs.kernels.empty() ? nullptr : const_cast<ze_kernel_handle_t *>(indirectArgs.kernels.data());
    return zeCommandListGetNextCommandIdWithKernelsExp(resolveExecutionTargetForInstantiate(executionTarget, apiArgs.hCommandList), &indirectArgs.desc, static_cast<uint32_t>(indirectArgs.kernels.size()), kernelHandles, &commandId);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopy>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const void *, size_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryCopy>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendBarrier>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendBarrier>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendWaitOnEvents>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint64_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint32_t, const size_t *, const void **, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryRangesBarrier>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.numRanges, getOptionalData(indirectArgs.rangeSizes), const_cast<const void **>(getOptionalData(indirectArgs.ranges)),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryFill>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const void *, size_t, size_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryFill>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const ze_copy_region_t *, uint32_t, uint32_t, const void *, const ze_copy_region_t *, uint32_t, uint32_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryCopyRegion>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, externalStorage.getCopyRegion(indirectArgs.dstRegion), apiArgs.dstPitch, apiArgs.dstSlicePitch,
              apiArgs.srcptr, externalStorage.getCopyRegion(indirectArgs.srcRegion), apiArgs.srcPitch, apiArgs.srcSlicePitch,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, ze_context_handle_t, const void *, size_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryCopyFromContext>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.hContextSrc, apiArgs.srcptr, apiArgs.size,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopy>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_image_handle_t, ze_image_handle_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendImageCopy>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.hDstImage, apiArgs.hSrcImage,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyRegion>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_image_handle_t, ze_image_handle_t, const ze_image_region_t *, const ze_image_region_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendImageCopyRegion>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.hDstImage, apiArgs.hSrcImage,
              externalStorage.getImageRegion(indirectArgs.dstRegion), externalStorage.getImageRegion(indirectArgs.srcRegion),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemory>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, ze_image_handle_t, const ze_image_region_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendImageCopyToMemory>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.hSrcImage, externalStorage.getImageRegion(indirectArgs.srcRegion),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_image_handle_t, const void *, const ze_image_region_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendImageCopyFromMemory>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.hDstImage, apiArgs.srcptr, externalStorage.getImageRegion(indirectArgs.dstRegion),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryPrefetch>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, const void *, size_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.ptr, apiArgs.size, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemAdvise>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_device_handle_t, const void *, size_t, ze_memory_advice_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.hDevice, apiArgs.ptr, apiArgs.size, apiArgs.advice, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalEvent>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_event_handle_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.hEvent, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendEventReset>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_event_handle_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.hEvent, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint32_t, ze_event_handle_t *, void *, const size_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendQueryKernelTimestamps>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.numEvents,
              const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.events)),
              apiArgs.dstptr, getOptionalData(indirectArgs.offsets),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint32_t, ze_external_semaphore_ext_handle_t *, ze_external_semaphore_signal_params_ext_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.numSemaphores,
              const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
              const_cast<ze_external_semaphore_signal_params_ext_t *>(&indirectArgs.signalParams),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint32_t, ze_external_semaphore_ext_handle_t *, ze_external_semaphore_wait_params_ext_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.numSemaphores,
              const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
              const_cast<ze_external_semaphore_wait_params_ext_t *>(&indirectArgs.waitParams),
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, ze_image_handle_t, const ze_image_region_t *, uint32_t, uint32_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.hSrcImage, externalStorage.getImageRegion(indirectArgs.srcRegion),
              apiArgs.destRowPitch, apiArgs.destSlicePitch,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_image_handle_t, const void *, const ze_image_region_t *, uint32_t, uint32_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.hDstImage, apiArgs.srcptr, externalStorage.getImageRegion(indirectArgs.dstRegion),
              apiArgs.srcRowPitch, apiArgs.srcSlicePitch,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernel>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_kernel_handle_t, const ze_group_count_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendLaunchKernel>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, indirectArgs.capturedKernel.get(), &indirectArgs.launchKernelArgs,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_kernel_handle_t, const ze_group_count_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, indirectArgs.capturedKernel.get(), &indirectArgs.launchKernelArgs,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_kernel_handle_t, const ze_group_count_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendLaunchKernelIndirect>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, indirectArgs.capturedKernel.get(), apiArgs.launchArgsBuffer,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, uint32_t, ze_kernel_handle_t *, const uint32_t *, const ze_group_count_t *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>(apiArgs, indirectArgs, externalStorage);
    std::vector<ze_kernel_handle_t> phKernelClones(apiArgs.numKernels);
    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        phKernelClones[i] = indirectArgs.capturedKernels[i].get();
    }
    return cb(apiArgs.hCommandList, apiArgs.numKernels, phKernelClones.data(), apiArgs.pCountBuffer, apiArgs.launchArgsBuffer,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_kernel_handle_t, const ze_group_count_t *, const void *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, indirectArgs.capturedKernel.get(), &indirectArgs.groupCounts, indirectArgs.pNext,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_kernel_handle_t, const ze_group_count_t, const ze_group_size_t, void **, const void *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>(apiArgs, indirectArgs, externalStorage);
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(kernelHandle));
    auto &explicitArgs = kernel->getKernelDescriptor().payloadMappings.explicitArgs;
    auto arguments = externalStorage.getKernelArguments(explicitArgs, this->indirectArgs.argumentsId);
    return cb(apiArgs.hCommandList, kernelHandle, apiArgs.groupCounts, apiArgs.groupSizes, arguments.data(), indirectArgs.pNext,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricStreamerMarker>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(zet_command_list_handle_t, zet_metric_streamer_handle_t, uint32_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.hMetricStreamer, apiArgs.value, userData);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricQueryBegin>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(zet_command_list_handle_t, zet_metric_query_handle_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.hMetricQuery, userData);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricQueryEnd>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(zet_command_list_handle_t, zet_metric_query_handle_t, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zetCommandListAppendMetricQueryEnd>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.hMetricQuery,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMetricMemoryBarrier>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(zet_command_list_handle_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, userData);
}

ze_result_t Closure<CaptureApi::zetCommandListAppendMarkerExp>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(zet_command_list_handle_t, zet_metric_group_handle_t, uint32_t, void *)>(visitorCallback);
    return cb(apiArgs.hCommandList, apiArgs.hMetricGroup, apiArgs.value, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const void *, size_t, const void *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryCopyWithParameters>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, indirectArgs.pNext,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const void *, size_t, const void *, uint32_t, ze_event_handle_t *, ze_event_handle_t, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, indirectArgs.pNext,
              static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), apiArgs.hSignalEvent, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryFillWithParameters>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const void *, size_t, size_t, const void *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendMemoryFillWithParameters>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size, indirectArgs.pNext,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, void *, const void *, size_t, size_t, const void *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zexCommandListAppendMemoryFillWithParameters>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size, indirectArgs.pNext,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendHostFunction>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &externalStorage) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, ze_host_function_callback_t, void *, const void *, ze_event_handle_t, uint32_t, ze_event_handle_t *, void *)>(visitorCallback);
    auto waitEventsList = getClosureWaitEventsList<CaptureApi::zeCommandListAppendHostFunction>(apiArgs, indirectArgs, externalStorage);
    return cb(apiArgs.hCommandList, apiArgs.pHostFunction, apiArgs.pUserData, apiArgs.pNext,
              apiArgs.hSignalEvent, static_cast<uint32_t>(waitEventsList.size()), waitEventsList.empty() ? nullptr : waitEventsList.data(), userData);
}

ze_result_t Closure<CaptureApi::zeCommandListGetNextCommandIdExp>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, const ze_mutable_command_id_exp_desc_t *, uint64_t *, void *)>(visitorCallback);
    uint64_t commandId = indirectArgs.commandId;
    return cb(apiArgs.hCommandList, &indirectArgs.desc, &commandId, userData);
}

ze_result_t Closure<CaptureApi::zeCommandListGetNextCommandIdWithKernelsExp>::invokeVisitor(void *visitorCallback, void *userData, ClosureExternalStorage &) const {
    auto cb = reinterpret_cast<ze_result_t(VISITOR_CCONV *)(ze_command_list_handle_t, const ze_mutable_command_id_exp_desc_t *, uint32_t, ze_kernel_handle_t *, uint64_t *, void *)>(visitorCallback);
    uint64_t commandId = indirectArgs.commandId;
    auto kernelHandles = indirectArgs.kernels.empty() ? nullptr : const_cast<ze_kernel_handle_t *>(indirectArgs.kernels.data());
    return cb(apiArgs.hCommandList, &indirectArgs.desc, static_cast<uint32_t>(indirectArgs.kernels.size()), kernelHandles, &commandId, userData);
}

ExecutableGraph::ExecutableGraph(WeaklyShared<OrderedExecutableSegmentsList> &&orderedCommands,
                                 WeaklyShared<ExternalCbEventInfoContainer> &&externalCbEvent) : orderedCommands(std::move(orderedCommands)),
                                                                                                 externalCbEventStorage(std::move(externalCbEvent)) {
    int32_t overrideDisablePatchingPreamble = NEO::debugManager.flags.ForceDisableGraphPatchPreamble.get();
    if (overrideDisablePatchingPreamble != -1) {
        this->usePatchingPreamble = (overrideDisablePatchingPreamble == 0);
    }
}

ExecutableGraph::~ExecutableGraph() {
    for (auto hEvent : trailingEvents) {
        Event::fromHandle(hEvent)->destroy();
    }
    if (trailingEventsPool) {
        trailingEventsPool->destroy();
    }
}

L0::CommandList *ExecutableGraph::allocateAndAddCommandListSubmissionNode() {
    ze_command_list_handle_t newCmdListHandle = nullptr;
    src->getContext()->createCommandList(src->getCaptureTargetDesc().hDevice, &src->getCaptureTargetDesc().desc, &newCmdListHandle);
    L0::CommandList *newCmdList = L0::CommandList::fromHandle(newCmdListHandle);
    newCmdList->disableFlatCapture();
    UNRECOVERABLE_IF(nullptr == newCmdList);
    this->myCommandLists.emplace_back(newCmdList);
    return newCmdList;
}

void prepareExecSubGraphs(const Graph &currLevelSrc, ExecutableGraph &parent, std::unordered_map<const Graph *, ExecSubGraphBuilder> &subgraphs) {
    std::unique_ptr<ExecutableGraph> currLevelDst = std::make_unique<ExecutableGraph>(parent.getOrderedCommands().share(),
                                                                                      parent.getExternalCbEventInfoContainer().share());
    subgraphs[&currLevelSrc] = {currLevelDst.get()};
    for (auto &child : currLevelSrc.getSubgraphs()) {
        prepareExecSubGraphs(*child, *currLevelDst, subgraphs);
    }
    parent.addSubgraph(std::move(currLevelDst));
};

ExecGraphBuilder::ExecGraphBuilder(Graph &rootSrc, ExecutableGraph &rootDst) : rootSrc(rootSrc), rootDst(rootDst) {
    this->subgraphs[&rootSrc] = {&rootDst};

    for (auto &child : rootSrc.getSubgraphs()) {
        prepareExecSubGraphs(*child, rootDst, this->subgraphs);
    }
}

ExecGraphBuilder::~ExecGraphBuilder() {
    for (auto hEvent : trailingEvents) {
        Event::fromHandle(hEvent)->destroy();
    }
    if (trailingEventsPool) {
        trailingEventsPool->destroy();
    }
}

inline void getSubgraphsWithPostJoinCommands(Graph &graph, std::unordered_set<const Graph *> &subgraphsWithPostJoinCommands) {
    const auto &resolvedJoins = graph.getResolvedJoins();
    for (auto *subgraph : graph.getSubgraphs()) {
        auto resolvedSubgraphJoinIt = resolvedJoins.find(subgraph);
        if ((resolvedSubgraphJoinIt == resolvedJoins.end()) || (false == subgraph->isLastCommand(resolvedSubgraphJoinIt->second.joinSignalCommandId))) {
            subgraphsWithPostJoinCommands.insert(subgraph);
        }
        getSubgraphsWithPostJoinCommands(*subgraph, subgraphsWithPostJoinCommands);
    }
}

void ExecGraphBuilder::createEventPoolForTrailingEvents(size_t numEvents) {
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.count = static_cast<uint32_t>(numEvents);

    ze_device_handle_t hDevice = rootSrc.getCaptureTargetDesc().hDevice;
    uint32_t numDevices = hDevice ? 1u : 0u;
    ze_result_t result = ZE_RESULT_SUCCESS;
    this->trailingEventsPool = EventPool::create(rootSrc.getContext()->getDriverHandle(), rootSrc.getContext(), numDevices, &hDevice, &eventPoolDesc, result);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);
    trailingEvents.reserve(numEvents);
}

L0::Event *ExecGraphBuilder::createTrailingEvent() {
    UNRECOVERABLE_IF(nullptr == this->trailingEventsPool);

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = static_cast<uint32_t>(this->trailingEvents.size());
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_event_handle_t hEvent = nullptr;
    auto result = this->trailingEventsPool->createEvent(&eventDesc, &hEvent);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

    auto *event = L0::Event::fromHandle(hEvent);
    this->trailingEvents.push_back(event);
    return event;
}

void ExecGraphBuilder::finalize(const GraphInstatiateSettings &settings) {
    std::unordered_set<const Graph *> subgraphsWithPostJoinCommands;
    if (settings.forkPolicy == GraphInstatiateSettings::ForkPolicyMonolythicLevels) {
        getSubgraphsWithPostJoinCommands(this->rootSrc, subgraphsWithPostJoinCommands);
        if (false == subgraphsWithPostJoinCommands.empty()) {
            createEventPoolForTrailingEvents(subgraphsWithPostJoinCommands.size());
        }
    }
    if (this->flatCommandList) {
        this->flatCommandList->close();
    } else if (this->subgraphs[&rootSrc].currCmdList != nullptr) {
        for (auto &subgraph : this->subgraphs) {
            if (subgraph.first == &rootSrc) {
                continue; // finalize root level as last
            }
            if (subgraph.second.currCmdList) {
                if (subgraphsWithPostJoinCommands.count(subgraph.first)) {
                    auto *event = this->createTrailingEvent();
                    subgraph.second.currCmdList->appendSignalEvent(event->toHandle(), false);
                }
                subgraph.second.currCmdList->close();
            }
        }
        if (this->trailingEvents.size() > 0) {
            this->subgraphs[&rootSrc].currCmdList->appendWaitOnEvents(static_cast<uint32_t>(this->trailingEvents.size()), this->trailingEvents.data(), nullptr, false, true, true, false, false, false);
            for (auto &ev : this->trailingEvents) {
                this->subgraphs[&rootSrc].currCmdList->appendEventReset(ev);
            }
        }
        this->subgraphs[&rootSrc].currCmdList->close(); // finalize root level
    }

    auto orderedExecutableCommands = rootDst.getOrderedCommands();
    for (const auto &segment : this->rootSrc.getOrderedCommands()) {
        if (this->subgraphs[segment.subgraph].dst->segmentRequiresSeperateSubmission(segment.begin)) {
            orderedExecutableCommands->push_back(OrderedCommandsExecutableSegment{.dst = this->subgraphs[segment.subgraph].dst,
                                                                                  .segmentStart = segment.begin});
        }
    }
}

ze_result_t ExecutableGraph::instantiateFrom(Graph &rootSrc, const GraphInstatiateSettings &settings) {
    this->src = &rootSrc;
    ExecGraphBuilder builder{rootSrc, *this};
    const auto &allCommands = rootSrc.getOrderedCommands();
    for (const auto &segment : allCommands) {
        auto err = builder.getSubGraphBuilder(segment.subgraph).dst->instantiateFrom(segment, builder, settings);
        if (ZE_RESULT_SUCCESS != err) {
            return err;
        }
    }
    builder.finalize(settings);
    this->trailingEventsPool = builder.releaseTrailingEventsPool();
    this->trailingEvents = builder.releaseTrailingEvents();
    if (this->getExternalCbEventInfoContainer()->externalCbEventsPresent()) {
        this->getExternalCbEventInfoContainer()->finalizeExecutorContainer();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ExecutableGraph::instantiateFrom(const OrderedCommandsSegment &segment, ExecGraphBuilder &builder, const GraphInstatiateSettings &settings) {
    ze_result_t err = ZE_RESULT_SUCCESS;
    this->src = segment.subgraph;

    this->executionTarget = this->src->getExecutionTarget();
    auto &segmentBuilder = builder.getSubGraphBuilder(segment.subgraph);

    CbExternalEventInstantiateContext externalCbEventsContext{this->getExternalCbEventInfoContainer().get(),
                                                              this->executionTarget};

    if (segment.empty() == false) {
        const auto &allCommands = src->getCapturedCommands();
        for (CapturedCommandId cmdId = segment.subBegin; cmdId < segment.subBegin + segment.numCommands; ++cmdId) {
            auto &cmd = allCommands[cmdId];
            if (nullptr == segmentBuilder.currCmdList) {
                if (settings.forkPolicy == GraphInstatiateSettings::ForkPolicyFlat) {
                    segmentBuilder.currCmdList = builder.getFlatCommandList();
                    if (nullptr == segmentBuilder.currCmdList) {
                        segmentBuilder.currCmdList = this->allocateAndAddCommandListSubmissionNode();
                        this->myOrderedSegments[segment.begin] = segmentBuilder.currCmdList;
                        builder.setFlatCommandList(segmentBuilder.currCmdList);
                    }
                } else {
                    segmentBuilder.currCmdList = this->allocateAndAddCommandListSubmissionNode();
                    this->myOrderedSegments[segment.begin] = segmentBuilder.currCmdList;
                }
            }
            switch (static_cast<CaptureApi>(cmd.index())) {
            default:
                break;
#define RR_CAPTURED_API(X)                                                                                                                                                         \
    case CaptureApi::X:                                                                                                                                                            \
        err = std::get<static_cast<size_t>(CaptureApi::X)>(cmd).instantiateTo(segmentBuilder.currCmdList, this->src->getExternalStorage(), externalCbEventsContext, std::nullopt); \
        break;
                RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
            }
            if (err != ZE_RESULT_SUCCESS) {
                return err;
            }
        }
    }
    if ((settings.forkPolicy == GraphInstatiateSettings::ForkPolicySplitLevels) && segmentBuilder.currCmdList) {
        segmentBuilder.currCmdList->close();
        segmentBuilder.currCmdList = nullptr;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ExecutableGraph::execute(L0::CommandList *executionTarget, void *pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (nullptr == executionTarget) {
        executionTarget = this->executionTarget;
    }
    UNRECOVERABLE_IF(nullptr == executionTarget);
    if (this->empty()) {
        return executionTarget->appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, false);
    } else {
        UNRECOVERABLE_IF(this->orderedCommands->empty());

        if (this->externalCbEventStorage->externalCbEventsPresent()) {
            this->externalCbEventStorage->updateExecutorContainer(executionTarget);
            this->externalCbEventStorage->attachExternalCbEventsToExecutableGraph();
        }

        auto segmentIt = this->getOrderedCommands()->begin();
        if (this->orderedCommands->size() == 1) {
            segmentIt->dst->executeSegment(executionTarget, segmentIt->segmentStart, hSignalEvent, numWaitEvents, phWaitEvents);
        } else {
            bool monolithicMode = myOrderedSegments.size() == 1;
            bool splitMode = (false == monolithicMode);
            auto lastSegment = this->getOrderedCommands()->end() - 1;
            segmentIt->dst->executeSegment(executionTarget, segmentIt->segmentStart,
                                           monolithicMode ? hSignalEvent : nullptr,
                                           numWaitEvents, phWaitEvents);
            ++segmentIt;
            while (segmentIt != lastSegment) {
                segmentIt->dst->executeSegment(executionTarget, segmentIt->segmentStart, nullptr, 0, nullptr);
                ++segmentIt;
            }
            DEBUG_BREAK_IF(segmentIt != lastSegment);
            segmentIt->dst->executeSegment(executionTarget, segmentIt->segmentStart,
                                           splitMode ? hSignalEvent : nullptr,
                                           0, nullptr);
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ExecutableGraph::executeSegment(L0::CommandList *executionTarget, GraphCommandId segmentStart, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (nullptr != this->executionTarget) {
        executionTarget = this->executionTarget;
    }

    CommandListExecutionInternalOptions internalOptions = {};
    if (this->externalCbEventStorage->externalCbEventsPresent()) {
        internalOptions.patchPreambleRequiredCounter = this->externalCbEventStorage->getPreambleCounter(this->executionTarget);
    }

    auto segmentIt = this->myOrderedSegments.find(segmentStart);
    if (segmentIt == this->myOrderedSegments.end()) {
        return ZE_RESULT_SUCCESS; // part of preceeding segment
    }
    ze_command_list_handle_t hCmdList = segmentIt->second;
    executionTarget->setPatchingPreamble(this->usePatchingPreamble);
    auto res = executionTarget->appendCommandLists(1, &hCmdList, hSignalEvent, numWaitEvents, phWaitEvents, internalOptions);
    executionTarget->setPatchingPreamble(false);
    return res;
}

void recordHandleWaitEventsFromNextCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, std::span<ze_event_handle_t> events) {
    if (captureTarget) {
        // already recording, look for joins
        for (auto evh : events) {
            auto *potentialJoinEvent = L0::Event::fromHandle(evh);
            auto signalFromCmdList = potentialJoinEvent->getRecordedSignalFrom();
            if (nullptr == signalFromCmdList) {
                continue;
            }
            captureTarget->tryJoinOnNextCommand(*signalFromCmdList, *potentialJoinEvent);
        }
    } else {
        // not recording yet, look for forks
        for (auto evh : events) {
            auto *potentialForkEvent = L0::Event::fromHandle(evh);
            auto signalFromCmdList = potentialForkEvent->getRecordedSignalFrom();
            if (nullptr == signalFromCmdList) {
                continue;
            }

            signalFromCmdList->getGraphCaptureTarget()->forkTo(srcCmdList, captureTarget, *potentialForkEvent);
        }
    }
}

void recordHandleSignalEventFromPreviousCommand(L0::CommandList &srcCmdList, Graph &captureTarget, ze_event_handle_t event) {
    if (nullptr == event) {
        return;
    }

    captureTarget.registerSignallingEventFromPreviousCommand(*L0::Event::fromHandle(event));
}

bool isGraphCapturingAllowed(const L0::CommandList &srcCmdList) {
    return srcCmdList.isImmediateType() && (false == srcCmdList.isInSynchronousMode());
}

bool usesForkEvents(std::span<ze_event_handle_t> events) {
    for (const auto &event : events) {
        if (L0::Event::fromHandle(event)->getRecordedSignalFrom()) {
            return true;
        }
    }
    return false;
}

struct VisitContext {
    explicit VisitContext(const ze_visit_ext_desc_t *desc)
        : desc(desc),
          userData(desc->userData),
          hReappendTargetCmdList(desc->hReappendTargetCmdList),
          preAction(desc->beforeDefaultOpClb),
          defaultOp(desc->defaultOp),
          postAction(desc->afterDefaultOpClb) {
    }

    ze_result_t initialize() {
        L0::PNextRange pnexts{desc->pNext};
        for (auto &pnext : pnexts) {
            if (pnext.stype != ZEX_STRUCTURE_TYPE_CONCRETE_VISITOR_EXT_DESC) {
                return ZE_RESULT_ERROR_INVALID_ENUMERATION;
            }
            const auto &concreteVisitor = reinterpret_cast<const ze_concrete_visitor_ext_desc_t &>(pnext);
            if (nullptr == concreteVisitor.fname) {
                return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
            }
            while (true) {
#define RR_CAPTURED_API(X)                                                                   \
    {                                                                                        \
        constexpr std::string_view apiName = #X;                                             \
        if (apiName == concreteVisitor.fname) {                                              \
            visitorCallbacks[static_cast<size_t>(CaptureApi::X)] = concreteVisitor.callback; \
            break;                                                                           \
        }                                                                                    \
    }
                RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }

        return ZE_RESULT_SUCCESS;
    }

    const ze_visit_ext_desc_t *desc = nullptr;
    ExternalCbEventInfoContainer cbEventInfo;
    void *userData = nullptr;
    ze_command_list_handle_t hReappendTargetCmdList = nullptr;
    ze_visit_ext_default_op_callback_t preAction = nullptr;
    ze_visit_ext_default_op_t defaultOp = ZE_VISIT_EXT_DEFAULT_OP_IGNORE;
    ze_visit_ext_default_op_callback_t postAction = nullptr;
    void *visitorCallbacks[numCapturedApis] = {};
};

ze_result_t Graph::visit(const ze_visit_ext_desc_t *desc) {
    if (nullptr == desc) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    VisitContext visitContext{desc};
    auto visitCtxInitResult = visitContext.initialize();
    if (ZE_RESULT_SUCCESS != visitCtxInitResult) {
        return visitCtxInitResult;
    }

    for (auto &seg : *orderedCommands) {
        auto &allCommands = seg.subgraph->recordedApiCommands;
        for (CapturedCommandId cmdId = seg.subBegin; cmdId < seg.subBegin + seg.numCommands; ++cmdId) {
            auto result = allCommands.visit(cmdId, visitContext);
            if (ZE_RESULT_SUCCESS != result) {
                return result;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t RecordedApiCommands::visit(const ze_visit_ext_desc_t *desc) {
    VisitContext visitContext{desc};
    auto visitCtxInitResult = visitContext.initialize();
    if (ZE_RESULT_SUCCESS != visitCtxInitResult) {
        return visitCtxInitResult;
    }

    for (CapturedCommandId id = 0; id < this->commands.size(); ++id) {
        auto result = this->visit(id, visitContext);
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t RecordedApiCommands::visit(CapturedCommandId id, VisitContext &visitContext) {
    if (id >= this->commands.size()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    CbExternalEventInstantiateContext cbEventContext{&visitContext.cbEventInfo, nullptr};

    auto &cmdIt = this->commands[id];
    switch (static_cast<CaptureApi>(cmdIt.index())) {
#define RR_CAPTURED_API(X)                                                                                                                               \
    case CaptureApi::X: {                                                                                                                                \
        auto &cmd = std::get<static_cast<size_t>(CaptureApi::X)>(cmdIt);                                                                                 \
        const char *apiName = #X;                                                                                                                        \
        if (visitContext.visitorCallbacks[static_cast<size_t>(CaptureApi::X)]) {                                                                         \
            auto ret = cmd.invokeVisitor(visitContext.visitorCallbacks[static_cast<size_t>(CaptureApi::X)], visitContext.userData, externalStorage);     \
            if (ZE_RESULT_SUCCESS != ret) {                                                                                                              \
                return ret;                                                                                                                              \
            }                                                                                                                                            \
        } else {                                                                                                                                         \
            auto signalEvent = getClosureSignalEvent<CaptureApi::X>(cmd.apiArgs);                                                                        \
            auto waitEventsList = getClosureWaitEventsList<CaptureApi::X>(cmd.apiArgs, cmd.indirectArgs, externalStorage);                               \
            ze_event_handle_t *waitEvents = waitEventsList.empty() ? nullptr : waitEventsList.data();                                                    \
            uint32_t numWaitEvents = static_cast<uint32_t>(waitEventsList.size());                                                                       \
            if (visitContext.preAction) {                                                                                                                \
                visitContext.preAction(apiName, visitContext.userData, &numWaitEvents, &waitEvents, &signalEvent);                                       \
            }                                                                                                                                            \
            if (ZE_VISIT_EXT_DEFAULT_OP_REAPPEND == visitContext.defaultOp) {                                                                            \
                EventParams events{.hSignalEvent = signalEvent, .numWaitEvents = numWaitEvents, .phWaitEvents = waitEvents};                             \
                auto ret = cmd.instantiateTo(L0::CommandList::fromHandle(visitContext.hReappendTargetCmdList), externalStorage, cbEventContext, events); \
                if (ZE_RESULT_SUCCESS != ret) {                                                                                                          \
                    return ret;                                                                                                                          \
                }                                                                                                                                        \
            }                                                                                                                                            \
            if (visitContext.postAction) {                                                                                                               \
                visitContext.postAction(apiName, visitContext.userData, &numWaitEvents, &waitEvents, &signalEvent);                                      \
            }                                                                                                                                            \
        }                                                                                                                                                \
    } break;

        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API

    default:
        UNREACHABLE();
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
