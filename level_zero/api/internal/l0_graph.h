/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeGraphCreateExp(ze_context_handle_t hContext, ze_graph_handle_t *phGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExp(ze_command_list_handle_t hCommandList, void *pNext);
ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListInstantiateGraphExp(ze_graph_handle_t hGraph, ze_executable_graph_handle_t *phExecutableGraph, void *pNext);
ze_result_t ZE_APICALL zeCommandListAppendGraphExp(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, void *pNext,
                                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t ZE_APICALL zeGraphDestroyExp(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeExecutableGraphDestroyExp(ze_executable_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExp(ze_command_list_handle_t hCommandList);
ze_result_t ZE_APICALL zeGraphIsEmptyExp(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeGraphDumpContentsExp(ze_graph_handle_t hGraph, const char *filePath, void *pNext);
ze_result_t ZE_APICALL zeCommandListGetGraphExp(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph);
ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExp(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, void *pNext);
ze_result_t ZE_APICALL zeGraphGetNodesExp(ze_graph_handle_t hGraph, uint32_t *pGraphNodesCount, ze_graph_node_handle_t *phGraphNodes);
ze_result_t ZE_APICALL zeGraphNodeGetPropertiesExp(ze_graph_node_handle_t hNode, ze_graph_node_exp_properties_t *pProperties);
ze_result_t ZE_APICALL zeGraphNodesUpdateExp(ze_graph_handle_t hGraph, uint32_t numNodes, ze_graph_node_handle_t *phNodes, const void **ppUpdateDescs, const void *pNext);
ze_result_t ZE_APICALL zeExecutableGraphNodesUpdateExp(ze_executable_graph_handle_t hExecutableGraph, uint32_t numNodes, ze_graph_node_handle_t *phNodes, const void **ppUpdateDescs, const void *pNext);
ze_result_t ZE_APICALL zeExecutableGraphUpdateExp(ze_executable_graph_handle_t hExecutableGraph, ze_graph_handle_t hSourceGraph, const void *pNext);
} // namespace L0
