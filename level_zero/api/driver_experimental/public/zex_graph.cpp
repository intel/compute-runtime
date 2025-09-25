/*
 * Copyright (C) 2025 Intel Corporation
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

ze_result_t ZE_APICALL zeGraphCreateExp(ze_context_handle_t hContext, ze_graph_handle_t *phGraph, void *pNext) {
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

ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExp(ze_command_list_handle_t hCommandList, void *pNext) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (cmdList->getCaptureTarget() != nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_context_handle_t hCtx = nullptr;
    cmdList->getContextHandle(&hCtx);
    auto ctx = L0::Context::fromHandle(hCtx);

    auto graph = new Graph(ctx, false);
    cmdList->setCaptureTarget(graph);
    graph->startCapturingFrom(*cmdList, false);

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, void *pNext) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (cmdList->getCaptureTarget() != nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    cmdList->setCaptureTarget(graph);
    graph->startCapturingFrom(*cmdList, false);

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph, void *pNext) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (nullptr == cmdList->getCaptureTarget()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    cmdList->getCaptureTarget()->stopCapturing();

    if (nullptr == phGraph) {
        if (cmdList->getCaptureTarget()->wasPreallocated()) {
            cmdList->setCaptureTarget(nullptr);
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    } else {
        *phGraph = cmdList->getCaptureTarget();
        cmdList->setCaptureTarget(nullptr);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListInstantiateGraphExp(ze_graph_handle_t hGraph, ze_executable_graph_handle_t *phExecutableGraph, void *pNext) {
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
    GraphInstatiateSettings settings{pNext};
    auto ret = execGraph->instantiateFrom(*virtualGraph, settings);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }
    *phExecutableGraph = execGraph.release();

    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListAppendGraphExp(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, void *pNext,
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

ze_result_t ZE_APICALL zeGraphDestroyExp(ze_graph_handle_t hGraph) {
    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    delete graph;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeExecutableGraphDestroyExp(ze_executable_graph_handle_t hGraph) {
    auto graph = L0::ExecutableGraph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    delete graph;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExp(ze_command_list_handle_t hCommandList) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    if (nullptr == cmdList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return cmdList->getCaptureTarget() ? ZE_RESULT_QUERY_TRUE : ZE_RESULT_QUERY_FALSE;
}

ze_result_t ZE_APICALL zeGraphIsEmptyExp(ze_graph_handle_t hGraph) {
    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (false == graph->valid()) {
        return ZE_RESULT_ERROR_INVALID_GRAPH;
    }
    return graph->empty() ? ZE_RESULT_QUERY_TRUE : ZE_RESULT_QUERY_FALSE;
}

ze_result_t ZE_APICALL zeGraphDumpContentsExp(ze_graph_handle_t hGraph, const char *filePath, void *pNext) {
    if (nullptr != pNext) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto graph = L0::Graph::fromHandle(hGraph);
    if (nullptr == graph) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (nullptr == filePath) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    L0::GraphDotExporter exporter{};
    return exporter.exportToFile(*graph, filePath);
}

} // namespace L0

#if defined(__cplusplus)
extern "C" {
#endif

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphCreateExp(ze_context_handle_t hContext, ze_graph_handle_t *phGraph, void *pNext) {
    return L0::zeGraphCreateExp(hContext, phGraph, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExp(ze_command_list_handle_t hCommandList, void *pNext) {
    return L0::zeCommandListBeginGraphCaptureExp(hCommandList, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, void *pNext) {
    return L0::zeCommandListBeginCaptureIntoGraphExp(hCommandList, hGraph, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph, void *pNext) {
    return L0::zeCommandListEndGraphCaptureExp(hCommandList, phGraph, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListInstantiateGraphExp(ze_graph_handle_t hGraph, ze_executable_graph_handle_t *phExecutableGraph, void *pNext) {
    return L0::zeCommandListInstantiateGraphExp(hGraph, phExecutableGraph, pNext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendGraphExp(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, void *pNext,
                                                                ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendGraphExp(hCommandList, hGraph, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphDestroyExp(ze_graph_handle_t hGraph) {
    return L0::zeGraphDestroyExp(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeExecutableGraphDestroyExp(ze_executable_graph_handle_t hGraph) {
    return L0::zeExecutableGraphDestroyExp(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExp(ze_command_list_handle_t hCommandList) {
    return L0::zeCommandListIsGraphCaptureEnabledExp(hCommandList);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphIsEmptyExp(ze_graph_handle_t hGraph) {
    return L0::zeGraphIsEmptyExp(hGraph);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphDumpContentsExp(ze_graph_handle_t hGraph, const char *filePath, void *pNext) {
    return L0::zeGraphDumpContentsExp(hGraph, filePath, pNext);
}

#if defined(__cplusplus)
} // extern "C"
#endif
