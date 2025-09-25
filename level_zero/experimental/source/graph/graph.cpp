/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"

#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel_imp.h"

#include <unordered_map>

namespace L0 {

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
    captureSrc.getDeviceHandle(&this->captureTargetDesc.hDevice);
    this->captureTargetDesc.desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    this->captureTargetDesc.desc.pNext = nullptr;
    this->captureTargetDesc.desc.flags = captureSrc.getCmdListFlags();
    captureSrc.getOrdinal(&this->captureTargetDesc.desc.commandQueueGroupOrdinal);
    if (isSubGraph) {
        this->executionTarget = &captureSrc;
    }
}

void Graph::stopCapturing() {
    this->unregisterSignallingEvents();
    this->captureSrc = nullptr;
    this->wasCapturingStopped = true;
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
    forkJoinInfo.forkDestiny = childCmdList.releaseCaptureTarget();
    forkJoinInfo.forkDestiny->stopCapturing();
    this->joinedForks[forkInfo->second.forkCommandId] = forkJoinInfo;

    this->unjoinedForks.erase(forkInfo);
}

void Graph::forkTo(L0::CommandList &childCmdList, Graph *&child, L0::Event &forkEvent) {
    UNRECOVERABLE_IF(child || childCmdList.getCaptureTarget()); // should not be capturing already
    ze_context_handle_t ctx = nullptr;
    childCmdList.getContextHandle(&ctx);
    child = new Graph(L0::Context::fromHandle(ctx), false);
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
    if (event && event->isGraphExternalEvent()) {
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

ExecutableGraph::~ExecutableGraph() = default;

L0::CommandList *ExecutableGraph::allocateAndAddCommandListSubmissionNode() {
    ze_command_list_handle_t newCmdListHandle = nullptr;
    src->getContext()->createCommandList(src->getCaptureTargetDesc().hDevice, &src->getCaptureTargetDesc().desc, &newCmdListHandle);
    L0::CommandList *newCmdList = L0::CommandList::fromHandle(newCmdListHandle);
    UNRECOVERABLE_IF(nullptr == newCmdList);
    this->myCommandLists.emplace_back(newCmdList);
    this->submissionChain.emplace_back(newCmdList);
    return newCmdList;
}

void ExecutableGraph::addSubGraphSubmissionNode(ExecutableGraph *subGraph) {
    this->submissionChain.emplace_back(subGraph);
}

ze_result_t ExecutableGraph::instantiateFrom(Graph &graph, const GraphInstatiateSettings &settings) {
    ze_result_t err = ZE_RESULT_SUCCESS;
    this->src = &graph;
    this->executionTarget = graph.getExecutionTarget();
    auto device = Device::fromHandle(src->getCaptureTargetDesc().hDevice);
    this->multiEngineGraph = device->getGfxCoreHelper().getContextGroupContextsCount() > 1;

    std::unordered_map<Graph *, ExecutableGraph *> executableSubGraphMap;
    executableSubGraphMap.reserve(graph.getSubgraphs().size());
    this->subGraphs.reserve(graph.getSubgraphs().size());
    for (auto &srcSubgraph : graph.getSubgraphs()) {
        auto execSubGraph = std::make_unique<ExecutableGraph>();
        err = execSubGraph->instantiateFrom(*srcSubgraph, settings);
        if (err != ZE_RESULT_SUCCESS) {
            return err;
        }
        executableSubGraphMap[srcSubgraph] = execSubGraph.get();
        this->subGraphs.push_back(std::move(execSubGraph));
    }

    if (graph.empty() == false) {
        L0::CommandList *currCmdList = nullptr;

        const auto &allCommands = src->getCapturedCommands();
        for (CapturedCommandId cmdId = 0; cmdId < static_cast<uint32_t>(allCommands.size()); ++cmdId) {
            auto &cmd = src->getCapturedCommands()[cmdId];
            if (nullptr == currCmdList) {
                currCmdList = this->allocateAndAddCommandListSubmissionNode();
            }
            switch (static_cast<CaptureApi>(cmd.index())) {
            default:
                break;
#define RR_CAPTURED_API(X)                                                                                                                                        \
    case CaptureApi::X:                                                                                                                                           \
        err = std::get<static_cast<size_t>(CaptureApi::X)>(cmd).instantiateTo(*currCmdList, graph.getExternalStorage(), this->getExternalCbEventInfoContainer()); \
        break;
                RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
            }
            if (err != ZE_RESULT_SUCCESS) {
                return err;
            }

            auto *forkTarget = graph.getJoinedForkTarget(cmdId);
            if (nullptr != forkTarget) {
                auto execSubGraph = executableSubGraphMap.find(forkTarget);
                UNRECOVERABLE_IF(executableSubGraphMap.end() == execSubGraph);

                if (settings.forkPolicy == GraphInstatiateSettings::ForkPolicySplitLevels) {
                    // interleave
                    currCmdList->close();
                    currCmdList = nullptr;
                    this->addSubGraphSubmissionNode(execSubGraph->second);
                } else {
                    // submit after current
                    UNRECOVERABLE_IF(settings.forkPolicy != GraphInstatiateSettings::ForkPolicyMonolythicLevels)
                    this->addSubGraphSubmissionNode(execSubGraph->second);
                }
            }
        }
        UNRECOVERABLE_IF(nullptr == currCmdList);
        currCmdList->close();
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
        this->externalCbEventStorage.attachExternalCbEventsToExecutableGraph();
        L0::CommandList *const myLastCommandList = this->myCommandLists.rbegin()->get();
        {
            // first submission node
            L0::CommandList **cmdList = std::get_if<L0::CommandList *>(&this->submissionChain[0]);
            UNRECOVERABLE_IF(nullptr == cmdList);

            auto currSignalEvent = (myLastCommandList == *cmdList) ? hSignalEvent : nullptr;

            ze_command_list_handle_t hCmdList = *cmdList;
            executionTarget->setPatchingPreamble(true, this->multiEngineGraph);
            auto res = executionTarget->appendCommandLists(1, &hCmdList, currSignalEvent, numWaitEvents, phWaitEvents);
            executionTarget->setPatchingPreamble(false, false);
            if (ZE_RESULT_SUCCESS != res) {
                return res;
            }
        }

        for (size_t submissioNodeId = 1; submissioNodeId < this->submissionChain.size(); ++submissioNodeId) {
            if (L0::CommandList **cmdList = std::get_if<L0::CommandList *>(&this->submissionChain[submissioNodeId])) {
                auto currSignalEvent = (myLastCommandList == *cmdList) ? hSignalEvent : nullptr;
                ze_command_list_handle_t hCmdList = *cmdList;
                executionTarget->setPatchingPreamble(true, false);
                auto res = executionTarget->appendCommandLists(1, &hCmdList, currSignalEvent, 0, nullptr);
                executionTarget->setPatchingPreamble(false, false);
                if (ZE_RESULT_SUCCESS != res) {
                    return res;
                }
            } else {
                L0::ExecutableGraph **subGraph = std::get_if<L0::ExecutableGraph *>(&this->submissionChain[submissioNodeId]);
                UNRECOVERABLE_IF(nullptr == subGraph);
                auto res = (*subGraph)->execute(nullptr, pNext, nullptr, 0, nullptr);
                if (ZE_RESULT_SUCCESS != res) {
                    return res;
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
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

} // namespace L0