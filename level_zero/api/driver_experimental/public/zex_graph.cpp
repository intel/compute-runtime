/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_graph.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/experimental/source/graph/graph_export.h"

namespace L0 {

ze_result_t ZE_APICALL zeGraphCreateExt(ze_context_handle_t hContext, const void *pNext, ze_graph_handle_t *phGraph) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto ctx = L0::Context::fromHandle(hContext);
    if (nullptr == ctx) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (nullptr == phGraph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *phGraph = new Graph(ctx, true);

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (cmdList->isCapturingGraph() || !cmdList->isImmediateType() || cmdList->isInSynchronousMode()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_context_handle_t hCtx = nullptr;
    cmdList->getContextHandle(&hCtx);
    auto ctx = L0::Context::fromHandle(hCtx);

    auto graph = new Graph(ctx, false);
    cmdList->setGraphCaptureTarget(graph);
    graph->startCapturingFrom(*cmdList, false);

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, const void *pNext) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (cmdList->isCapturingGraph()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph || !graph->empty()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    cmdList->setGraphCaptureTarget(graph);
    graph->startCapturingFrom(*cmdList, false);

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext, ze_graph_handle_t *phGraph) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto *graph = cmdList->getGraphCaptureTarget();
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if ((nullptr == phGraph) && (false == graph->wasPreallocated())) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    cmdList->getGraphCaptureTarget()->stopCapturing();

    if (nullptr == phGraph) {
        UNRECOVERABLE_IF(false == graph->wasPreallocated());
        cmdList->setGraphCaptureTarget(nullptr);
        return ZE_RESULT_SUCCESS;
    } else {
        *phGraph = graph->toHandle();
        cmdList->setGraphCaptureTarget(nullptr);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeGraphInstantiateExt(ze_graph_handle_t hGraph, const void *pNext, ze_executable_graph_handle_t *phExecutableGraph) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto virtualGraph = L0::Graph::fromHandle(hGraph);
    if (nullptr == virtualGraph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (nullptr == phExecutableGraph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (false == virtualGraph->validForInstantiation()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto execGraph = std::make_unique<ExecutableGraph>();
    GraphInstatiateSettings settings{const_cast<void *>(pNext), virtualGraph->isMultiEngineGraph()};
    auto ret = execGraph->instantiateFrom(*virtualGraph, settings);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }
    *phExecutableGraph = execGraph.release();

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListAppendGraphExt(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, const void *pNext,
                                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto graph = L0::ExecutableGraph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return graph->execute(cmdList, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t ZE_APICALL zeGraphDestroyExt(ze_graph_handle_t hGraph) {
    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    delete graph;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeExecutableGraphDestroyExt(ze_executable_graph_handle_t hGraph) {
    auto graph = L0::ExecutableGraph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    delete graph;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExt(ze_command_list_handle_t hCommandList) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return cmdList->getGraphCaptureTarget() ? ZE_RESULT_QUERY_TRUE : ZE_RESULT_QUERY_FALSE;
}

ze_result_t ZE_APICALL zeCommandListGetGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if ((nullptr == cmdList) || (nullptr == phGraph) || (nullptr == cmdList->getGraphCaptureTarget())) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    auto rootGraph = cmdList->getGraphCaptureTarget();

    if (rootGraph) {
        while (rootGraph->getParentGraph() != nullptr) {
            rootGraph = rootGraph->getParentGraph();
        }
    }

    *phGraph = rootGraph;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeGraphIsEmptyExt(ze_graph_handle_t hGraph) {
    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (false == graph->valid()) {
        return ZE_RESULT_ERROR_INVALID_GRAPH;
    }
    return graph->empty() ? ZE_RESULT_QUERY_TRUE : ZE_RESULT_QUERY_FALSE;
}

ze_result_t ZE_APICALL zeGraphDumpContentsExt(ze_graph_handle_t hGraph, const char *filePath, const void *pNext) {
    auto *graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (nullptr == filePath) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    L0::GraphExportStyle exportStyle = L0::GraphExportStyle::detailed;
    const ze_base_desc_t *desc = reinterpret_cast<const ze_base_desc_t *>(pNext);

    if (desc != nullptr) {
        if ((desc->stype == ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXP_DUMP_DESC) || (desc->stype == ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXT_DUMP_DESC)) {
            const auto *dumpDesc = reinterpret_cast<const ze_record_replay_graph_exp_dump_desc_t *>(desc);
            if (dumpDesc->mode == ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_SIMPLE) {
                exportStyle = L0::GraphExportStyle::simple;
            } else if (dumpDesc->mode == ZE_RECORD_REPLAY_GRAPH_EXP_DUMP_MODE_DETAILED) {
                exportStyle = L0::GraphExportStyle::detailed;
            } else {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Could not recognize provided graph dump mode, mode: 0x%x.\n",
                             dumpDesc->mode);
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        } else {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Could not recognize provided extension, stype: 0x%x.\n",
                         desc->stype);
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    L0::GraphDotExporter exporter{exportStyle};
    return exporter.exportToFile(*graph, filePath);
}

ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExt(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, const void *pNext) {
    if ((nullptr == hGraph) || (nullptr == pfnCallback)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    auto *graph = L0::Graph::fromHandle(hGraph);
    return graph->addDestructorCallback(pfnCallback, pUserData, const_cast<void *>(pNext));
}

ze_result_t ZE_APICALL zeExecutableGraphGetSourceGraphExt(ze_executable_graph_handle_t hGraph, ze_graph_handle_t *phSourceGraph) {
    auto *graph = L0::ExecutableGraph::fromHandle(hGraph);
    if ((nullptr == graph) || (nullptr == phSourceGraph)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto *sourceGraph = graph->getSourceGraph();
    if (nullptr == sourceGraph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *phSourceGraph = sourceGraph->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeGraphGetPrimaryCommandListExt(ze_graph_handle_t hGraph, ze_command_list_handle_t *phCommandList) {
    auto *graph = L0::Graph::fromHandle(hGraph);
    if ((nullptr == graph) || (nullptr == phCommandList)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto *rootGraph = graph;
    while (rootGraph->getParentGraph() != nullptr) {
        rootGraph = rootGraph->getParentGraph();
    }

    auto *primaryCmdList = rootGraph->getPrimaryCaptureSource();
    if (nullptr == primaryCmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *phCommandList = primaryCmdList->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeGraphVisitExt(ze_graph_handle_t hGraph, const ze_visit_ext_desc_t *desc) {
    auto *graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    if (nullptr == desc) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    return graph->visit(desc);
}

ze_result_t ZE_APICALL zeGraphGetIdExt(ze_graph_handle_t hGraph, uint64_t *pGraphId) {
    const auto *graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    if (nullptr == pGraphId) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    *pGraphId = graph->getId();
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeGraphPauseCaptureExt(ze_graph_handle_t hGraph) {
    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    return graph->pauseCapturing();
}

ze_result_t ZE_APICALL zeGraphResumeCaptureExt(ze_graph_handle_t hGraph) {
    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    return graph->resumeCapturing();
}

// Legacy experimental entrypoints kept for backwards compatibility.
ze_result_t ZE_APICALL zeGraphCreateExp(ze_context_handle_t hContext, ze_graph_handle_t *phGraph, void *pNext) {
    return L0::zeGraphCreateExt(hContext, pNext, phGraph);
}

ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExp(ze_command_list_handle_t hCommandList, void *pNext) {
    return L0::zeCommandListBeginGraphCaptureExt(hCommandList, pNext);
}

ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, void *pNext) {
    return L0::zeCommandListBeginCaptureIntoGraphExt(hCommandList, hGraph, pNext);
}

ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph, void *pNext) {
    return L0::zeCommandListEndGraphCaptureExt(hCommandList, pNext, phGraph);
}

ze_result_t ZE_APICALL zeCommandListInstantiateGraphExp(ze_graph_handle_t hGraph, ze_executable_graph_handle_t *phExecutableGraph, void *pNext) {
    return L0::zeGraphInstantiateExt(hGraph, pNext, phExecutableGraph);
}

ze_result_t ZE_APICALL zeCommandListAppendGraphExp(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, void *pNext,
                                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendGraphExt(hCommandList, hGraph, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t ZE_APICALL zeGraphDestroyExp(ze_graph_handle_t hGraph) {
    return L0::zeGraphDestroyExt(hGraph);
}

ze_result_t ZE_APICALL zeExecutableGraphDestroyExp(ze_executable_graph_handle_t hGraph) {
    return L0::zeExecutableGraphDestroyExt(hGraph);
}

ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExp(ze_command_list_handle_t hCommandList) {
    return L0::zeCommandListIsGraphCaptureEnabledExt(hCommandList);
}

ze_result_t ZE_APICALL zeGraphIsEmptyExp(ze_graph_handle_t hGraph) {
    return L0::zeGraphIsEmptyExt(hGraph);
}

ze_result_t ZE_APICALL zeGraphDumpContentsExp(ze_graph_handle_t hGraph, const char *filePath, void *pNext) {
    return L0::zeGraphDumpContentsExt(hGraph, filePath, pNext);
}

ze_result_t ZE_APICALL zeCommandListGetGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph) {
    return L0::zeCommandListGetGraphExt(hCommandList, phGraph);
}

ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExp(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, void *pNext) {
    return L0::zeGraphSetDestructionCallbackExt(hGraph, pfnCallback, pUserData, pNext);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphCreateExt(ze_context_handle_t hContext, const void *pNext, ze_graph_handle_t *phGraph) {
    return L0::zeGraphCreateExt(hContext, pNext, phGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext) {
    return L0::zeCommandListBeginGraphCaptureExt(hCommandList, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, const void *pNext) {
    return L0::zeCommandListBeginCaptureIntoGraphExt(hCommandList, hGraph, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext, ze_graph_handle_t *phGraph) {
    return L0::zeCommandListEndGraphCaptureExt(hCommandList, pNext, phGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphInstantiateExt(ze_graph_handle_t hGraph, const void *pNext, ze_executable_graph_handle_t *phExecutableGraph) {
    return L0::zeGraphInstantiateExt(hGraph, pNext, phExecutableGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendGraphExt(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, const void *pNext,
                                                                ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendGraphExt(hCommandList, hGraph, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphDestroyExt(ze_graph_handle_t hGraph) {
    return L0::zeGraphDestroyExt(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeExecutableGraphDestroyExt(ze_executable_graph_handle_t hGraph) {
    return L0::zeExecutableGraphDestroyExt(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExt(ze_command_list_handle_t hCommandList) {
    return L0::zeCommandListIsGraphCaptureEnabledExt(hCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphIsEmptyExt(ze_graph_handle_t hGraph) {
    return L0::zeGraphIsEmptyExt(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphDumpContentsExt(ze_graph_handle_t hGraph, const char *filePath, const void *pNext) {
    return L0::zeGraphDumpContentsExt(hGraph, filePath, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph) {
    return L0::zeCommandListGetGraphExt(hCommandList, phGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExt(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, const void *pNext) {
    return L0::zeGraphSetDestructionCallbackExt(hGraph, pfnCallback, pUserData, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeExecutableGraphGetSourceGraphExt(ze_executable_graph_handle_t hGraph, ze_graph_handle_t *phSourceGraph) {
    return L0::zeExecutableGraphGetSourceGraphExt(hGraph, phSourceGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphGetPrimaryCommandListExt(ze_graph_handle_t hGraph, ze_command_list_handle_t *phCommandList) {
    return L0::zeGraphGetPrimaryCommandListExt(hGraph, phCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphPauseCaptureExt(ze_graph_handle_t hGraph) {
    return L0::zeGraphPauseCaptureExt(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphResumeCaptureExt(ze_graph_handle_t hGraph) {
    return L0::zeGraphResumeCaptureExt(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphGetIdExt(ze_graph_handle_t hGraph, uint64_t *pGraphId) {
    return L0::zeGraphGetIdExt(hGraph, pGraphId);
}

} // extern "C"
