/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"

#include "shared/source/utilities/io_functions.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/kernel/kernel_imp.h"

#include <optional>
#include <sstream>
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

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopy>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemoryCopy(&executionTarget, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendBarrier>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendBarrier(&executionTarget, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendWaitOnEvents(&executionTarget, apiArgs.numEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWriteGlobalTimestamp>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendWriteGlobalTimestamp(&executionTarget, apiArgs.dstptr, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryRangesBarrier>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemoryRangesBarrier(&executionTarget, apiArgs.numRanges, getOptionalData(indirectArgs.rangeSizes), const_cast<const void **>(getOptionalData(indirectArgs.ranges)),
                                                  apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryFill>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemoryFill(&executionTarget, apiArgs.ptr, getOptionalData(indirectArgs.pattern), apiArgs.patternSize, apiArgs.size,
                                         apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyRegion>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemoryCopyRegion(&executionTarget, apiArgs.dstptr, externalStorage.getCopyRegion(indirectArgs.dstRegion), apiArgs.dstPitch, apiArgs.dstSlicePitch,
                                               apiArgs.srcptr, externalStorage.getCopyRegion(indirectArgs.srcRegion), apiArgs.srcPitch, apiArgs.srcSlicePitch,
                                               apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopyFromContext>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemoryCopyFromContext(&executionTarget, apiArgs.dstptr, apiArgs.hContextSrc, apiArgs.srcptr, apiArgs.size,
                                                    apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopy>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendImageCopy(&executionTarget, apiArgs.hDstImage, apiArgs.hSrcImage,
                                        apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyRegion>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendImageCopyRegion(&executionTarget, apiArgs.hDstImage, apiArgs.hSrcImage, externalStorage.getImageRegion(indirectArgs.dstRegion), externalStorage.getImageRegion(indirectArgs.srcRegion),
                                              apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemory>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendImageCopyToMemory(&executionTarget,
                                                apiArgs.dstptr,
                                                apiArgs.hSrcImage,
                                                externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemory>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendImageCopyFromMemory(&executionTarget,
                                                  apiArgs.hDstImage,
                                                  apiArgs.srcptr,
                                                  externalStorage.getImageRegion(indirectArgs.dstRegion),
                                                  apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryPrefetch>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemoryPrefetch(&executionTarget,
                                             apiArgs.ptr,
                                             apiArgs.size);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemAdvise>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendMemAdvise(&executionTarget,
                                        apiArgs.hDevice,
                                        apiArgs.ptr,
                                        apiArgs.size,
                                        apiArgs.advice);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalEvent>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendSignalEvent(&executionTarget,
                                          apiArgs.hEvent);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendEventReset>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendEventReset(&executionTarget, apiArgs.hEvent);
}

ze_result_t Closure<CaptureApi::zeCommandListAppendQueryKernelTimestamps>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendQueryKernelTimestamps(&executionTarget,
                                                    apiArgs.numEvents,
                                                    const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.events)),
                                                    apiArgs.dstptr,
                                                    getOptionalData(indirectArgs.offsets),
                                                    apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendSignalExternalSemaphoreExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendSignalExternalSemaphoreExt(&executionTarget,
                                                         apiArgs.numSemaphores,
                                                         const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
                                                         const_cast<ze_external_semaphore_signal_params_ext_t *>(&indirectArgs.signalParams),
                                                         apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitExternalSemaphoreExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendWaitExternalSemaphoreExt(&executionTarget,
                                                       apiArgs.numSemaphores,
                                                       const_cast<ze_external_semaphore_ext_handle_t *>(getOptionalData(indirectArgs.semaphores)),
                                                       const_cast<ze_external_semaphore_wait_params_ext_t *>(&indirectArgs.waitParams),
                                                       apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendImageCopyToMemoryExt(&executionTarget,
                                                   apiArgs.dstptr,
                                                   apiArgs.hSrcImage,
                                                   externalStorage.getImageRegion(indirectArgs.srcRegion),
                                                   apiArgs.destRowPitch,
                                                   apiArgs.destSlicePitch,
                                                   apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    return zeCommandListAppendImageCopyFromMemoryExt(&executionTarget,
                                                     apiArgs.hDstImage,
                                                     apiArgs.srcptr,
                                                     externalStorage.getImageRegion(indirectArgs.dstRegion),
                                                     apiArgs.srcRowPitch,
                                                     apiArgs.srcSlicePitch,
                                                     apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

Closure<CaptureApi::zeCommandListAppendLaunchKernel>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->launchKernelArgs = *apiArgs.launchKernelArgs;

    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    L0::KernelMutableState stateSnapshot = kernel->getMutableState();
    this->kernelStateId = externalStorage.registerKernelState(std::move(stateSnapshot));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernel>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    auto *kernelOrig = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    DEBUG_BREAK_IF(nullptr == kernelOrig);

    auto kernelClone = kernelOrig->cloneWithStateOverride(externalStorage.getKernelMutableState(this->indirectArgs.kernelStateId));

    return zeCommandListAppendLaunchKernel(&executionTarget, kernelClone.get(), &indirectArgs.launchKernelArgs, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    this->launchKernelArgs = *apiArgs.launchKernelArgs;

    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    L0::KernelMutableState stateSnapshot = kernel->getMutableState();
    this->kernelStateId = externalStorage.registerKernelState(std::move(stateSnapshot));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchCooperativeKernel>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    auto *kernelOrig = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    DEBUG_BREAK_IF(nullptr == kernelOrig);

    auto kernelClone = kernelOrig->cloneWithStateOverride(externalStorage.getKernelMutableState(this->indirectArgs.kernelStateId));

    return zeCommandListAppendLaunchCooperativeKernel(&executionTarget, kernelClone.get(), &indirectArgs.launchKernelArgs, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    L0::KernelMutableState stateSnapshot = kernel->getMutableState();
    this->kernelStateId = externalStorage.registerKernelState(std::move(stateSnapshot));
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchKernelIndirect>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    auto *kernelOrig = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.kernelHandle));
    DEBUG_BREAK_IF(nullptr == kernelOrig);

    auto kernelClone = kernelOrig->cloneWithStateOverride(externalStorage.getKernelMutableState(this->indirectArgs.kernelStateId));

    return zeCommandListAppendLaunchKernelIndirect(&executionTarget, kernelClone.get(), apiArgs.launchArgsBuffer, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
}

Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::IndirectArgs::IndirectArgs(const ApiArgs &apiArgs, ClosureExternalStorage &externalStorage) : IndirectArgsWithWaitEvents(apiArgs, externalStorage) {
    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        auto kernel = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.phKernels[i]));
        L0::KernelMutableState stateSnapshot = kernel->getMutableState();
        const auto id = externalStorage.registerKernelState(std::move(stateSnapshot));
        if (i == 0U) {
            this->firstKernelStateId = id;
        }
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendLaunchMultipleKernelsIndirect>::instantiateTo(L0::CommandList &executionTarget, ClosureExternalStorage &externalStorage) const {
    std::vector<decltype(std::declval<KernelImp *>()->cloneWithStateOverride(nullptr))> kernelClonesOwner(apiArgs.numKernels);
    std::vector<ze_kernel_handle_t> phKernelClones(apiArgs.numKernels);

    for (uint32_t i{0U}; i < apiArgs.numKernels; ++i) {
        auto *kernelOrig = static_cast<KernelImp *>(Kernel::fromHandle(apiArgs.phKernels[i]));
        DEBUG_BREAK_IF(nullptr == kernelOrig);
        const auto kernelStateId = this->indirectArgs.firstKernelStateId + i;
        kernelClonesOwner[i] = kernelOrig->cloneWithStateOverride(externalStorage.getKernelMutableState(kernelStateId));
        phKernelClones[i] = kernelClonesOwner[i].get();
    }

    return zeCommandListAppendLaunchMultipleKernelsIndirect(&executionTarget, apiArgs.numKernels, phKernelClones.data(), apiArgs.pCountBuffer, apiArgs.launchArgsBuffer, apiArgs.hSignalEvent, apiArgs.numWaitEvents, externalStorage.getEventsList(indirectArgs.waitEvents));
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

void ExecutableGraph::instantiateFrom(Graph &graph, const GraphInstatiateSettings &settings) {
    this->src = &graph;
    this->executionTarget = graph.getExecutionTarget();

    std::unordered_map<Graph *, ExecutableGraph *> executableSubGraphMap;
    executableSubGraphMap.reserve(graph.getSubgraphs().size());
    this->subGraphs.reserve(graph.getSubgraphs().size());
    for (auto &srcSubgraph : graph.getSubgraphs()) {
        auto execSubGraph = std::make_unique<ExecutableGraph>();
        execSubGraph->instantiateFrom(*srcSubgraph, settings);
        executableSubGraphMap[srcSubgraph] = execSubGraph.get();
        this->subGraphs.push_back(std::move(execSubGraph));
    }

    if (graph.empty() == false) {
        [[maybe_unused]] ze_result_t err = ZE_RESULT_SUCCESS;
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
#define RR_CAPTURED_API(X)                                                                                         \
    case CaptureApi::X:                                                                                            \
        std::get<static_cast<size_t>(CaptureApi::X)>(cmd).instantiateTo(*currCmdList, graph.getExternalStorage()); \
        DEBUG_BREAK_IF(err != ZE_RESULT_SUCCESS);                                                                  \
        break;
                RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
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
        L0::CommandList *const myLastCommandList = this->myCommandLists.rbegin()->get();
        {
            // first submission node
            L0::CommandList **cmdList = std::get_if<L0::CommandList *>(&this->submissionChain[0]);
            UNRECOVERABLE_IF(nullptr == cmdList);

            auto currSignalEvent = (myLastCommandList == *cmdList) ? hSignalEvent : nullptr;

            ze_command_list_handle_t hCmdList = *cmdList;
            executionTarget->setPatchingPreamble(true);
            auto res = executionTarget->appendCommandLists(1, &hCmdList, currSignalEvent, numWaitEvents, phWaitEvents);
            executionTarget->setPatchingPreamble(false);
            if (ZE_RESULT_SUCCESS != res) {
                return res;
            }
        }

        for (size_t submissioNodeId = 1; submissioNodeId < this->submissionChain.size(); ++submissioNodeId) {
            if (L0::CommandList **cmdList = std::get_if<L0::CommandList *>(&this->submissionChain[submissioNodeId])) {
                auto currSignalEvent = (myLastCommandList == *cmdList) ? hSignalEvent : nullptr;
                ze_command_list_handle_t hCmdList = *cmdList;
                executionTarget->setPatchingPreamble(true);
                auto res = executionTarget->appendCommandLists(1, &hCmdList, currSignalEvent, 0, nullptr);
                executionTarget->setPatchingPreamble(false);
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

ze_result_t GraphDotExporter::exportToFile(const Graph &graph, const char *filePath) const {
    if (nullptr == filePath) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    FILE *file = NEO::IoFunctions::fopenPtr(filePath, "w");
    if (nullptr == file) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::string dotContent = exportToString(graph);
    size_t bytesWritten = NEO::IoFunctions::fwritePtr(dotContent.c_str(), 1, dotContent.size(), file);
    NEO::IoFunctions::fclosePtr(file);

    if (bytesWritten != dotContent.size()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

std::string GraphDotExporter::exportToString(const Graph &graph) const {
    std::ostringstream dot;

    writeHeader(dot);
    writeNodes(dot, graph, 0, 0);
    writeEdges(dot, graph, 0, 0);
    writeSubgraphs(dot, graph, 0);

    dot << "}\n";
    return dot.str();
}

void GraphDotExporter::writeHeader(std::ostringstream &dot) const {
    dot << "digraph \"graph\" {\n";
    dot << "  rankdir=TB;\n";
    dot << "  nodesep=1;\n";
    dot << "  ranksep=1;\n";
    dot << "  node [shape=box, style=filled];\n";
    dot << "  edge [color=black];\n\n";
}

void GraphDotExporter::writeNodes(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');
    dot << indent << "// Command nodes:\n";

    const auto &commands = graph.getCapturedCommands();
    for (CapturedCommandId cmdId = 0; cmdId < static_cast<uint32_t>(commands.size()); ++cmdId) {
        const std::string nodeId = generateNodeId(level, subgraphId, cmdId);
        const std::string label = getCommandNodeLabel(graph, cmdId);
        const std::string attributes = getCommandNodeAttributes(graph, cmdId);

        dot << indent << nodeId << " [label=\"" << label << "\"" << attributes << "];\n";
    }
    dot << "\n";
}

void GraphDotExporter::writeEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    writeSequentialEdges(dot, graph, level, subgraphId);
    writeForkJoinEdges(dot, graph, level, subgraphId);
    writeUnjoinedForkEdges(dot, graph, level, subgraphId);

    dot << "\n";
}

void GraphDotExporter::writeSequentialEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &commands = graph.getCapturedCommands();
    dot << indent << "// Sequential edges:\n";

    for (CapturedCommandId cmdId = 1; cmdId < static_cast<uint32_t>(commands.size()); ++cmdId) {
        const std::string fromNode = generateNodeId(level, subgraphId, cmdId - 1);
        const std::string toNode = generateNodeId(level, subgraphId, cmdId);
        dot << indent << fromNode << " -> " << toNode << ";\n";
    }
}

void GraphDotExporter::writeForkJoinEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &joinedForks = graph.getJoinedForks();
    const auto &subGraphs = graph.getSubgraphs();

    dot << "\n"
        << indent << "// Fork/Join edges:\n";

    for (const auto &[forkCmdId, forkJoinInfo] : joinedForks) {
        const auto subgraphIndex = findSubgraphIndex(subGraphs, forkJoinInfo.forkDestiny);
        if (subgraphIndex && !forkJoinInfo.forkDestiny->getCapturedCommands().empty()) {
            const auto &subgraphCommands = forkJoinInfo.forkDestiny->getCapturedCommands();
            const std::string forkNode = generateNodeId(level, subgraphId, forkJoinInfo.forkCommandId);
            const std::string subgraphFirstNode = generateNodeId(level + 1, *subgraphIndex, 0);
            const std::string subgraphLastNode = generateNodeId(level + 1, *subgraphIndex, static_cast<uint32_t>(subgraphCommands.size()) - 1);
            const std::string joinNode = generateNodeId(level, subgraphId, forkJoinInfo.joinCommandId);

            dot << indent << forkNode << " -> " << subgraphFirstNode << ";\n";
            dot << indent << subgraphLastNode << " -> " << joinNode << ";\n";
        }
    }
}

void GraphDotExporter::writeUnjoinedForkEdges(std::ostringstream &dot, const Graph &graph, uint32_t level, uint32_t subgraphId) const {
    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');

    const auto &unjoinedForks = graph.getUnjoinedForks();
    const auto &subGraphs = graph.getSubgraphs();

    dot << "\n"
        << indent << "// Unjoined forks:\n";

    for (const auto &[cmdList, forkInfo] : unjoinedForks) {
        const auto subgraphIndex = findSubgraphIndexByCommandList(subGraphs, cmdList);
        if (subgraphIndex && !subGraphs[*subgraphIndex]->getCapturedCommands().empty()) {
            const std::string forkNode = generateNodeId(level, subgraphId, forkInfo.forkCommandId);
            const std::string subgraphFirstNode = generateNodeId(level + 1, *subgraphIndex, 0);
            dot << indent << forkNode << " -> " << subgraphFirstNode << " [color=red, label=\"unjoined fork\"];\n";
        }
    }
}

std::optional<uint32_t> GraphDotExporter::findSubgraphIndex(const StackVec<Graph *, 16> &subGraphs, const Graph *targetGraph) const {
    for (uint32_t i = 0; i < static_cast<uint32_t>(subGraphs.size()); ++i) {
        if (subGraphs[i] == targetGraph) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> GraphDotExporter::findSubgraphIndexByCommandList(const StackVec<Graph *, 16> &subGraphs, const L0::CommandList *cmdList) const {
    for (uint32_t i = 0; i < static_cast<uint32_t>(subGraphs.size()); ++i) {
        if (subGraphs[i]->getExecutionTarget() == cmdList) {
            return i;
        }
    }
    return std::nullopt;
}

void GraphDotExporter::writeSubgraphs(std::ostringstream &dot, const Graph &graph, uint32_t level) const {
    const auto &subGraphs = graph.getSubgraphs();
    if (subGraphs.empty()) {
        return;
    }

    const std::string indent(static_cast<size_t>(level + 1) * 2, ' ');
    dot << indent << "// Subgraphs:\n";

    for (uint32_t subgraphId = 0; subgraphId < static_cast<uint32_t>(subGraphs.size()); ++subgraphId) {
        const std::string clusterName = "cluster_" + generateSubgraphId(level + 1, subgraphId);

        dot << indent << "subgraph " << clusterName << " {\n";
        dot << indent << "  label=\"Subgraph " << (level + 1) << "-" << subgraphId << "\";\n";
        dot << indent << "  style=filled;\n";
        dot << indent << "  fillcolor=" << getSubgraphFillColor(level + 1) << ";\n\n";

        writeNodes(dot, *subGraphs[subgraphId], level + 1, subgraphId);
        writeEdges(dot, *subGraphs[subgraphId], level + 1, subgraphId);
        writeSubgraphs(dot, *subGraphs[subgraphId], level + 1);

        dot << indent << "  }\n\n";
    }
}

std::string GraphDotExporter::getCommandNodeLabel(const Graph &graph, CapturedCommandId cmdId) const {
    const auto &commands = graph.getCapturedCommands();
    const auto &cmd = commands[cmdId];

    std::string baseLabel;
    switch (static_cast<CaptureApi>(cmd.index())) {
#define RR_CAPTURED_API(X) \
    case CaptureApi::X:    \
        baseLabel = #X;    \
        break;

        RR_CAPTURED_APIS()
#undef RR_CAPTURED_API

    default:
        baseLabel = "Unknown";
        break;
    }

    return baseLabel;
}

std::string GraphDotExporter::getCommandNodeAttributes(const Graph &graph, CapturedCommandId cmdId) const {
    const auto &commands = graph.getCapturedCommands();
    const auto &cmd = commands[cmdId];

    switch (static_cast<CaptureApi>(cmd.index())) {
    case CaptureApi::zeCommandListAppendMemoryCopy:
    case CaptureApi::zeCommandListAppendMemoryCopyRegion:
    case CaptureApi::zeCommandListAppendMemoryCopyFromContext:
    case CaptureApi::zeCommandListAppendMemoryFill:
        return ", fillcolor=lightblue";

    case CaptureApi::zeCommandListAppendBarrier:
    case CaptureApi::zeCommandListAppendMemoryRangesBarrier:
        return ", fillcolor=orange";

    case CaptureApi::zeCommandListAppendSignalEvent:
    case CaptureApi::zeCommandListAppendWaitOnEvents:
    case CaptureApi::zeCommandListAppendEventReset:
        return ", fillcolor=yellow";

    case CaptureApi::zeCommandListAppendImageCopy:
    case CaptureApi::zeCommandListAppendImageCopyRegion:
    case CaptureApi::zeCommandListAppendImageCopyToMemory:
    case CaptureApi::zeCommandListAppendImageCopyFromMemory:
    case CaptureApi::zeCommandListAppendImageCopyToMemoryExt:
    case CaptureApi::zeCommandListAppendImageCopyFromMemoryExt:
        return ", fillcolor=lightgreen";

    case CaptureApi::zeCommandListAppendWriteGlobalTimestamp:
    case CaptureApi::zeCommandListAppendQueryKernelTimestamps:
        return ", fillcolor=pink";

    default:
        return ", fillcolor=aliceblue";
    }
}

std::string GraphDotExporter::generateNodeId(uint32_t level, uint32_t subgraphId, CapturedCommandId cmdId) const {
    std::ostringstream oss;
    oss << "L" << level << "_S" << subgraphId << "_C" << cmdId;
    return oss.str();
}

std::string GraphDotExporter::generateSubgraphId(uint32_t level, uint32_t subgraphId) const {
    std::ostringstream oss;
    oss << "L" << level << "_S" << subgraphId;
    return oss.str();
}

std::string GraphDotExporter::getSubgraphFillColor(uint32_t level) const {
    const std::vector<std::string> colors = {
        "grey90", // Level 1
        "grey80", // Level 2
        "grey70", // Level 3
        "grey60", // Level 4
        "grey50"  // Level 5+
    };

    size_t colorIndex = static_cast<size_t>(level) - 1;
    if (colorIndex >= colors.size()) {
        colorIndex = colors.size() - 1;
    }

    return colors[colorIndex];
}

} // namespace L0