/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"

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

Closure<CaptureApi::zeCommandListAppendMemoryCopy>::Closure(const ApiArgs &apiArgs)
    : apiArgs(apiArgs) {
    this->indirectArgs.waitEvents.reserve(apiArgs.numWaitEvents);
    for (uint32_t i = 0; i < apiArgs.numWaitEvents; ++i) {
        this->indirectArgs.waitEvents.push_back(apiArgs.phWaitEvents[i]);
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopy>::instantiateTo(L0::CommandList &executionTarget) const {
    return zeCommandListAppendMemoryCopy(&executionTarget, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, apiArgs.hSignalEvent, apiArgs.numWaitEvents, const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.waitEvents)));
}

Closure<CaptureApi::zeCommandListAppendBarrier>::Closure(const ApiArgs &apiArgs)
    : apiArgs(apiArgs) {
    this->indirectArgs.waitEvents.reserve(apiArgs.numWaitEvents);
    for (uint32_t i = 0; i < apiArgs.numWaitEvents; ++i) {
        this->indirectArgs.waitEvents.push_back(apiArgs.phWaitEvents[i]);
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendBarrier>::instantiateTo(L0::CommandList &executionTarget) const {
    return zeCommandListAppendBarrier(&executionTarget, apiArgs.hSignalEvent, apiArgs.numWaitEvents, const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.waitEvents)));
}

Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::Closure(const ApiArgs &apiArgs)
    : apiArgs(apiArgs) {
    this->indirectArgs.waitEvents.reserve(apiArgs.numEvents);
    for (uint32_t i = 0; i < apiArgs.numEvents; ++i) {
        this->indirectArgs.waitEvents.push_back(apiArgs.phEvents[i]);
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendWaitOnEvents>::instantiateTo(L0::CommandList &executionTarget) const {
    return zeCommandListAppendWaitOnEvents(&executionTarget, apiArgs.numEvents, const_cast<ze_event_handle_t *>(getOptionalData(indirectArgs.waitEvents)));
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
#define RR_CAPTURED_API(X)                                                             \
    case CaptureApi::X:                                                                \
        std::get<static_cast<size_t>(CaptureApi::X)>(cmd).instantiateTo(*currCmdList); \
        DEBUG_BREAK_IF(err != ZE_RESULT_SUCCESS);                                      \
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
            auto res = executionTarget->appendCommandLists(1, &hCmdList, currSignalEvent, numWaitEvents, phWaitEvents);
            if (ZE_RESULT_SUCCESS != res) {
                return res;
            }
        }

        for (size_t submissioNodeId = 1; submissioNodeId < this->submissionChain.size(); ++submissioNodeId) {
            if (L0::CommandList **cmdList = std::get_if<L0::CommandList *>(&this->submissionChain[submissioNodeId])) {
                auto currSignalEvent = (myLastCommandList == *cmdList) ? hSignalEvent : nullptr;
                ze_command_list_handle_t hCmdList = *cmdList;
                auto res = executionTarget->appendCommandLists(1, &hCmdList, currSignalEvent, numWaitEvents, phWaitEvents);
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

void recordHandleWaitEventsFromNextCommand(L0::CommandList &srcCmdList, Graph *&captureTarget, NEO::Range<ze_event_handle_t> events) {
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