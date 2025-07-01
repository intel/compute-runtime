/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/graph/graph.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"

namespace L0 {

Graph::~Graph() {
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
    this->captureSrc = nullptr;
}

Closure<CaptureApi::zeCommandListAppendMemoryCopy>::Closure(const ApiArgs &apiArgs)
    : apiArgs(apiArgs) {
    this->indirectArgs.waitEvents.reserve(apiArgs.numWaitEvents);
    for (uint32_t i = 0; i < apiArgs.numWaitEvents; ++i) {
        this->indirectArgs.waitEvents.push_back(apiArgs.phWaitEvents[i]);
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendMemoryCopy>::instantiateTo(L0::CommandList &executionTarget) const {
    return zeCommandListAppendMemoryCopy(&executionTarget, apiArgs.dstptr, apiArgs.srcptr, apiArgs.size, apiArgs.hSignalEvent, apiArgs.numWaitEvents, apiArgs.numWaitEvents ? const_cast<ze_event_handle_t *>(indirectArgs.waitEvents.data()) : nullptr);
}

Closure<CaptureApi::zeCommandListAppendBarrier>::Closure(const ApiArgs &apiArgs)
    : apiArgs(apiArgs) {
    this->indirectArgs.waitEvents.reserve(apiArgs.numWaitEvents);
    for (uint32_t i = 0; i < apiArgs.numWaitEvents; ++i) {
        this->indirectArgs.waitEvents.push_back(apiArgs.phWaitEvents[i]);
    }
}

ze_result_t Closure<CaptureApi::zeCommandListAppendBarrier>::instantiateTo(L0::CommandList &executionTarget) const {
    return zeCommandListAppendBarrier(&executionTarget, apiArgs.hSignalEvent, apiArgs.numWaitEvents, apiArgs.numWaitEvents ? const_cast<ze_event_handle_t *>(indirectArgs.waitEvents.data()) : nullptr);
}

ExecutableGraph::~ExecutableGraph() = default;

void ExecutableGraph::instantiateFrom(Graph &graph) {
    this->src = &graph;
    this->executionTarget = graph.getExecutionTarget();

    if (graph.empty() == false) {
        [[maybe_unused]] ze_result_t err = ZE_RESULT_SUCCESS;
        ze_command_list_handle_t cmdListHandle = nullptr;
        src->getContext()->createCommandList(src->getCaptureTargetDesc().hDevice, &src->getCaptureTargetDesc().desc, &cmdListHandle);
        L0::CommandList *hwCommands = L0::CommandList::fromHandle(cmdListHandle);
        UNRECOVERABLE_IF(nullptr == hwCommands);
        this->hwCommands.reset(hwCommands);

        for (const CapturedCommand &cmd : src->getCapturedCommands()) {
            switch (static_cast<CaptureApi>(cmd.index())) {
            default:
                break;
#define RR_CAPTURED_API(X)                                                            \
    case CaptureApi::X:                                                               \
        std::get<static_cast<size_t>(CaptureApi::X)>(cmd).instantiateTo(*hwCommands); \
        DEBUG_BREAK_IF(err != ZE_RESULT_SUCCESS);                                     \
        break;
                RR_CAPTURED_APIS()
#undef RR_CAPTURED_API
            }
        }
        hwCommands->close();
    }

    this->subGraphs.reserve(graph.getSubgraphs().size());
    for (auto &srcSubgraph : graph.getSubgraphs()) {
        auto execSubGraph = std::make_unique<ExecutableGraph>();
        execSubGraph->instantiateFrom(*srcSubgraph);
        this->subGraphs.push_back(std::move(execSubGraph));
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
        auto commands = this->hwCommands.get();
        ze_command_list_handle_t graphCmdList = commands;
        auto res = executionTarget->appendCommandLists(1, &graphCmdList, hSignalEvent, numWaitEvents, phWaitEvents);
        if (ZE_RESULT_SUCCESS != res) {
            return res;
        }
    }
    for (auto &subGraph : this->subGraphs) {
        auto res = subGraph->execute(nullptr, pNext, nullptr, 0, nullptr);
        if (ZE_RESULT_SUCCESS != res) {
            return res;
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0