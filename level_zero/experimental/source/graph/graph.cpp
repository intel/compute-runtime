/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/driver_experimental/zex_cmdlist.h"

#include <unordered_map>

namespace L0 {

GraphInstatiateSettings::GraphInstatiateSettings(void *pNext, bool multiEngineGraph) {
    UNRECOVERABLE_IF(nullptr != pNext);

    this->forkPolicy = multiEngineGraph ? ForkPolicy::ForkPolicyMonolythicLevels : ForkPolicy::ForkPolicySplitLevels;
    int32_t overrideForceGraphForkPolicy = NEO::debugManager.flags.ForceGraphForkPolicy.get();
    if (overrideForceGraphForkPolicy != -1) {
        this->forkPolicy = static_cast<ForkPolicy>(overrideForceGraphForkPolicy);
    }
}

Graph::~Graph() {
    this->unregisterSignallingEvents();
    for (auto *sg : subGraphs) {
        if (false == sg->wasPreallocated()) {
            delete sg;
        }
    }
}

void Graph::startCapturingFrom(L0::CommandList &captureSrc, bool isSubGraph) {
    this->captureSrc = &captureSrc;
    this->captureSrc->setCaptureTarget(this);
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
    this->captureSrc->releaseCaptureTarget();
    this->captureSrc = nullptr;
    StackVec<std::pair<L0::CommandList *, ForkInfo>, 1> neverJoinedForks; // should stay empty for valid graphs
    for (auto &unjFork : this->unjoinedForks) {
        auto forkCmdId = unjFork.second.forkCommandId;
        auto potentialJoin = this->potentialJoins.find(forkCmdId);
        if (this->potentialJoins.end() == potentialJoin) {
            neverJoinedForks.push_back({unjFork.first, unjFork.second});
            continue; // no join-like sequences found
        }
        auto potentialJoinEvent = static_cast<L0::Event *>(potentialJoin->second.joinEvent);
        auto potentialJoinRecordedSignal = potentialJoin->second.forkDestiny->recordedSignals.find(potentialJoinEvent);
        UNRECOVERABLE_IF(potentialJoin->second.forkDestiny->recordedSignals.end() == potentialJoinRecordedSignal);
        auto potentialJoinSignalId = potentialJoinRecordedSignal->second;
        if (false == potentialJoin->second.forkDestiny->isLastCommand(potentialJoinSignalId)) {
            neverJoinedForks.push_back({unjFork.first, unjFork.second});
            continue; // join-like sequence found but is succeeded by unjoined commands
        }
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

void Graph::tryJoinOnNextCommand(L0::CommandList &childCmdList, L0::Event &joinEvent) {
    auto forkInfo = this->unjoinedForks.find(&childCmdList);
    if (this->unjoinedForks.end() == forkInfo) {
        return;
    }

    ForkJoinInfo forkJoinInfo = {};
    forkJoinInfo.forkCommandId = forkInfo->second.forkCommandId;
    forkJoinInfo.forkEvent = forkInfo->second.forkEvent;
    forkJoinInfo.joinCommandId = static_cast<CapturedCommandId>(this->commands.size());
    forkJoinInfo.joinEvent = &joinEvent;
    forkJoinInfo.forkDestiny = childCmdList.getCaptureTarget();
    this->potentialJoins[forkInfo->second.forkCommandId] = forkJoinInfo;
}

void Graph::forkTo(L0::CommandList &childCmdList, Graph *&child, L0::Event &forkEvent) {
    UNRECOVERABLE_IF(child || childCmdList.getCaptureTarget()); // should not be capturing already
    child = new Graph(this->ctx, false, this->orderedCommands.share());
    child->startCapturingFrom(childCmdList, true);
    childCmdList.setCaptureTarget(child);
    this->subGraphs.push_back(child);

    auto forkEventInfo = this->recordedSignals.find(&forkEvent);
    UNRECOVERABLE_IF(this->recordedSignals.end() == forkEventInfo);
    this->unjoinedForks[&childCmdList] = ForkInfo{.forkCommandId = forkEventInfo->second,
                                                  .forkEvent = &forkEvent};
}

void Graph::registerSignallingEventFromPreviousCommand(L0::Event &ev) {
    ev.setRecordedSignalFrom(this->captureSrc);
    this->recordedSignals[&ev] = static_cast<CapturedCommandId>(this->commands.size() - 1);
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

void handleExternalCbEvent(L0::Event *event, ExternalCbEventInfoContainer &container) {
    if (event && event->isExternalEvent()) {
        container.addCbEventInfo(event, event->getInOrderExecInfo(), event->getInOrderExecBaseSignalValue(), event->getInOrderAllocationOffset());
    }
}

void ExternalCbEventInfoContainer::attachExternalCbEventsToExecutableGraph() {
    for (auto &info : storage) {
        auto sharedPtr = info.eventSharedPtrInfo.lock();
        info.event->updateInOrderExecState(sharedPtr, info.signalValue, info.allocationOffset);
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopy>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendMemoryCopy(&executionTarget, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendBarrier>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendBarrier(&executionTarget, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    return zeCommandListAppendWaitOnEvents(&executionTarget, apiArgs.numEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendWriteGlobalTimestamp(&executionTarget, apiArgs.dstptr, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendMemoryRangesBarrier(&executionTarget, apiArgs.numRanges, getOptionalData(indirectArgs.rangeSizes), const_cast<const void **>(getOptionalData(indirectArgs.ranges)),
                                                         apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryFill>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendMemoryFill(&executionTarget, apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size,
                                                apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendMemoryCopyRegion(&executionTarget, apiArgs.dstptr, externalStorage.getCopyRegion(indirectArgs.dstRegion), apiArgs.dstPitch, apiArgs.dstSlicePitch,
                                                      apiArgs.srcptr, externalStorage.getCopyRegion(indirectArgs.srcRegion), apiArgs.srcPitch, apiArgs.srcSlicePitch,
                                                      apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendMemoryCopyFromContext(&executionTarget, apiArgs.dstptr, apiArgs.hContextSrc, apiArgs.srcptr, apiArgs.size,
                                                           apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopy>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendImageCopy(&executionTarget, apiArgs.hDstImage, apiArgs.hSrcImage,
                                               apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyRegion>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendImageCopyRegion(&executionTarget, apiArgs.hDstImage, apiArgs.hSrcImage, externalStorage.getImageRegion(indirectArgs.dstRegion), externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                     apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemory>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendImageCopyToMemory(&executionTarget,
                                                       apiArgs.dstptr,
                                                       apiArgs.hSrcImage,
                                                       externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                       apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendImageCopyFromMemory(&executionTarget,
                                                         apiArgs.hDstImage,
                                                         apiArgs.srcptr,
                                                         externalStorage.getImageRegion(indirectArgs.dstRegion),
                                                         apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryPrefetch>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    return zeCommandListAppendMemoryPrefetch(&executionTarget,
                                             apiArgs.ptr,
                                             apiArgs.size);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemAdvise>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    return zeCommandListAppendMemAdvise(&executionTarget,
                                        apiArgs.hDevice,
                                        apiArgs.ptr,
                                        apiArgs.size,
                                        apiArgs.advice);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalEvent>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendSignalEvent(&executionTarget,
                                                 apiArgs.hEvent);
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendEventReset>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    return zeCommandListAppendEventReset(&executionTarget, apiArgs.hEvent);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendQueryKernelTimestamps(&executionTarget,
                                                           apiArgs.numEvents,
                                                           const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.events)),
                                                           apiArgs.dstptr,
                                                           getOptionalData(indirectArgs.offsets),
                                                           apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendSignalExternalSemaphoreExt(&executionTarget,
                                                                apiArgs.numSemaphores,
                                                                const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
                                                                const_cast<ze_external_semaphore_signal_params_ext_t *>(&indirectArgs.signalParams),
                                                                apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendWaitExternalSemaphoreExt(&executionTarget,
                                                              apiArgs.numSemaphores,
                                                              const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
                                                              const_cast<ze_external_semaphore_wait_params_ext_t *>(&indirectArgs.waitParams),
                                                              apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendImageCopyToMemoryExt(&executionTarget,
                                                          apiArgs.dstptr,
                                                          apiArgs.hSrcImage,
                                                          externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                          apiArgs.destRowPitch,
                                                          apiArgs.destSlicePitch,
                                                          apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendImageCopyFromMemoryExt(&executionTarget,
                                                            apiArgs.hDstImage,
                                                            apiArgs.srcptr,
                                                            externalStorage.getImageRegion(indirectArgs.dstRegion),
                                                            apiArgs.srcRowPitch,
                                                            apiArgs.srcSlicePitch,
                                                            apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernel>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->launchKernelArgs = *apiArgs.launchKernelArgs;
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernel>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernel(&executionTarget, kernelHandle, &indirectArgs.launchKernelArgs, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->launchKernelArgs = *apiArgs.launchKernelArgs;
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchCooperativeKernel(&executionTarget, kernelHandle, &indirectArgs.launchKernelArgs, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernelIndirect(&executionTarget, kernelHandle, apiArgs.launchArgsBuffer, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->capturedKernels.reserve(apiArgs.numKernels);
    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.phKernels[i]));
        this->capturedKernels.emplace_back(kernel->makeDependentClone().release());
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    std::vector<ze_kernel_handle_t> phKernelClones(apiArgs.numKernels);
    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        phKernelClones[i] = this->indirectArgs.capturedKernels[i].get();
    }
    auto result = zeCommandListAppendLaunchMultipleKernelsIndirect(&executionTarget, apiArgs.numKernels, phKernelClones.data(), apiArgs.pCountBuffer, apiArgs.launchArgsBuffer, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->groupCounts = *apiArgs.pGroupCounts;
    this->pNext = nullptr;

    auto result = CommandList::cloneAppendKernelExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    this->capturedKernel = kernel->makeDependentClone();
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendKernelExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelWithParameters>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernelWithParameters(&executionTarget, kernelHandle, &indirectArgs.groupCounts, indirectArgs.pNext, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->pNext = nullptr;

    auto result = CommandList::cloneAppendKernelExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    result = CommandList::setKernelState(kernel, apiArgs.groupSizes, apiArgs.pArguments);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

    this->capturedKernel = kernel->makeDependentClone();
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendKernelExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelWithArguments>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto *kernelHandle = this->indirectArgs.capturedKernel.get();
    auto result = zeCommandListAppendLaunchKernelWithParameters(&executionTarget, kernelHandle, &apiArgs.groupCounts, this->indirectArgs.pNext, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->pNext = nullptr;

    auto result = CommandList::cloneAppendMemoryCopyExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);
}

Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendMemoryCopyExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zexCommandListAppendMemoryCopyWithParameters>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zexCommandListAppendMemoryCopyWithParameters(&executionTarget, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, indirectArgs.pNext, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents), apiArgs.hSignalEvent);
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    pattern.resize(apiArgs.patternSize);
    memcpy_s(pattern.data(), pattern.size(), apiArgs.pattern, apiArgs.patternSize);

    this->pNext = nullptr;

    auto result = CommandList::cloneAppendMemoryCopyExtensions(reinterpret_cast<const ze_base_desc_t *>(apiArgs.pNext), this->pNext);
    UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);
}

Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::IndirectArgs::~IndirectArgs() {
    CommandList::freeClonedAppendMemoryCopyExtensions(this->pNext);
}

ze_result_t Closure<CaptureApi::zexCommandListAppendMemoryFillWithParameters>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zexCommandListAppendMemoryFillWithParameters(&executionTarget, apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size, indirectArgs.pNext, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ze_result_t Closure<CaptureApi::zeCommandListAppendHostFunction>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage, ExternalCbEventInfoContainer &externalCbEventStorage) const {
    auto result = zeCommandListAppendHostFunction(&executionTarget, apiArgs.pHostFunction, apiArgs.pUserData, apiArgs.pNext, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
    handleExternalCbEvent(L0::Event::fromHandle(apiArgs.hSignalEvent), externalCbEventStorage);
    return result;
}

ExecutableGraph::ExecutableGraph(WeaklyShared<OrderedExecutableSegmentsList> &&orderedCommands) : orderedCommands(std::move(orderedCommands)) {
    int32_t overrideDisablePatchingPreamble = NEO::debugManager.flags.ForceDisableGraphPatchPreamble.get();
    if (overrideDisablePatchingPreamble != -1) {
        this->usePatchingPreamble = (overrideDisablePatchingPreamble == 0);
    }
}

ExecutableGraph::~ExecutableGraph() = default;

L0::CommandList *ExecutableGraph::allocateAndAddCommandListSubmissionNode() {
    ze_command_list_handle_t newCmdListHandle = nullptr;
    src->getContext()->createCommandList(src->getCaptureTargetDesc().hDevice, &src->getCaptureTargetDesc().desc, &newCmdListHandle);
    L0::CommandList *newCmdList = L0::CommandList::fromHandle(newCmdListHandle);
    UNRECOVERABLE_IF(nullptr == newCmdList);
    this->myCommandLists.emplace_back(newCmdList);
    return newCmdList;
}

void prepareExecSubGraphs(const Graph &currLevelSrc, ExecutableGraph &parent, std::unordered_map<const Graph *, ExecSubGraphBuilder> &subgraphs) {
    std::unique_ptr<ExecutableGraph> currLevelDst = std::make_unique<ExecutableGraph>(parent.getOrderedCommands().share());
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

void ExecGraphBuilder::finalize() {
    for (auto &subgraph : this->subgraphs) {
        if (subgraph.second.currCmdList) {
            subgraph.second.currCmdList->close();
        }
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
    ExecGraphBuilder builder{rootSrc, *this};
    const auto &allCommands = rootSrc.getOrderedCommands();
    for (const auto &segment : allCommands) {
        auto err = builder.getSubGraphBuilder(segment.subgraph).dst->instantiateFrom(segment, builder, settings);
        if (ZE_RESULT_SUCCESS != err) {
            return err;
        }
    }
    builder.finalize();
    return ZE_RESULT_SUCCESS;
}

ze_result_t ExecutableGraph::instantiateFrom(const OrderedCommandsSegment &segment, ExecGraphBuilder &builder, const GraphInstatiateSettings &settings) {
    ze_result_t err = ZE_RESULT_SUCCESS;
    this->src = segment.subgraph;

    this->executionTarget = this->src->getExecutionTarget();
    auto &segmentBuilder = builder.getSubGraphBuilder(segment.subgraph);

    if (segment.empty() == false) {
        const auto &allCommands = src->getCapturedCommands();
        for (CapturedCommandId cmdId = segment.subBegin; cmdId < segment.subBegin + segment.numCommands; ++cmdId) {
            auto &cmd = allCommands[cmdId];
            if (nullptr == segmentBuilder.currCmdList) {
                segmentBuilder.currCmdList = this->allocateAndAddCommandListSubmissionNode();
                this->myOrderedSegments[segment.begin] = segmentBuilder.currCmdList;
            }
            switch (static_cast<CaptureApi>(cmd.index())) {
            default:
                break;
#define RR_CAPTURED_API(X)                                                                                                                                                            \
    case CaptureApi::X:                                                                                                                                                               \
        err = std::get<static_cast<size_t>(CaptureApi::X)>(cmd).instantiateTo(*segmentBuilder.currCmdList, this->src->getExternalStorage(), this->getExternalCbEventInfoContainer()); \
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
        if (numWaitEvents) {
            executionTarget->appendWaitOnEvents(numWaitEvents, phWaitEvents, nullptr, false, true, true, false, false, false);
        }
        if (nullptr == hSignalEvent) {
            return ZE_RESULT_SUCCESS;
        }
        executionTarget->appendSignalEvent(hSignalEvent, false);
    } else {
        UNRECOVERABLE_IF(this->orderedCommands->empty())
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

    if (this->externalCbEventStorage.externalCbEventsPresent()) {
        this->externalCbEventStorage.attachExternalCbEventsToExecutableGraph();
    }

    auto segmentIt = this->myOrderedSegments.find(segmentStart);
    if (segmentIt == this->myOrderedSegments.end()) {
        return ZE_RESULT_SUCCESS; // part of preceeding segment
    }
    ze_command_list_handle_t hCmdList = segmentIt->second;
    executionTarget->setPatchingPreamble(this->usePatchingPreamble);
    auto res = executionTarget->appendCommandLists(1, &hCmdList, hSignalEvent, numWaitEvents, phWaitEvents);
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

            signalFromCmdList->getCaptureTarget()->forkTo(srcCmdList, captureTarget, *potentialForkEvent);
        }
    }
}

void recordHandleSignalEventFromPreviousCommand(L0::CommandList &srcCmdList, Graph &captureTarget, ze_event_handle_t event) {
    if (nullptr == event) {
        return;
    }

    captureTarget.registerSignallingEventFromPreviousCommand(*L0::Event::fromHandle(event));
}

bool isCapturingAllowed(const L0::CommandList &srcCmdList) {
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

} // namespace L0
