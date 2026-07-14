/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_graph.h"

#include "level_zero/api/core/ze_graph_api_entrypoints.h"
#include "level_zero/api/internal/l0_graph.h"

namespace L0 {

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
    auto ret = L0::zeCommandListIsGraphCaptureEnabledExt(hCommandList);
    switch (ret) {
    case ZE_RESULT_QUERY_TRUE:
        return ZE_RESULT_QUERY_TRUE_EXP;
    case ZE_RESULT_QUERY_FALSE:
        return ZE_RESULT_QUERY_FALSE_EXP;
    case ZE_RESULT_ERROR_INVALID_GRAPH:
        return ZE_RESULT_ERROR_INVALID_GRAPH_EXP;
    default:
        return ret;
    }
}

ze_result_t ZE_APICALL zeGraphIsEmptyExp(ze_graph_handle_t hGraph) {
    auto ret = L0::zeGraphIsEmptyExt(hGraph);
    switch (ret) {
    case ZE_RESULT_QUERY_TRUE:
        return ZE_RESULT_QUERY_TRUE_EXP;
    case ZE_RESULT_QUERY_FALSE:
        return ZE_RESULT_QUERY_FALSE_EXP;
    case ZE_RESULT_ERROR_INVALID_GRAPH:
        return ZE_RESULT_ERROR_INVALID_GRAPH_EXP;
    default:
        return ret;
    }
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
