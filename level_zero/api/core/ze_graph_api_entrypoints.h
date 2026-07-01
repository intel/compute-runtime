/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/internal/l0_graph.h"

namespace L0 {

ze_result_t ZE_APICALL zeGraphCreateExt(ze_context_handle_t hContext, const void *pNext, ze_graph_handle_t *phGraph);
ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext);
ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, const void *pNext);
ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext, ze_graph_handle_t *phGraph);
ze_result_t ZE_APICALL zeGraphInstantiateExt(ze_graph_handle_t hGraph, const void *pNext, ze_executable_graph_handle_t *phExecutableGraph);
ze_result_t ZE_APICALL zeCommandListAppendGraphExt(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, const void *pNext,
                                                   ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t ZE_APICALL zeGraphDestroyExt(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeExecutableGraphDestroyExt(ze_executable_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExt(ze_command_list_handle_t hCommandList);
ze_result_t ZE_APICALL zeGraphIsEmptyExt(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeGraphDumpContentsExt(ze_graph_handle_t hGraph, const char *filePath, const void *pNext);
ze_result_t ZE_APICALL zeCommandListGetGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph);
ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExt(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, const void *pNext);
ze_result_t ZE_APICALL zeExecutableGraphGetSourceGraphExt(ze_executable_graph_handle_t hGraph, ze_graph_handle_t *phSourceGraph);
ze_result_t ZE_APICALL zeGraphGetPrimaryCommandListExt(ze_graph_handle_t hGraph, ze_command_list_handle_t *phCommandList);
ze_result_t ZE_APICALL zeGraphPauseCaptureExt(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeGraphResumeCaptureExt(ze_graph_handle_t hGraph);
ze_result_t ZE_APICALL zeGraphGetIdExt(ze_graph_handle_t hGraph, uint64_t *pGraphId);

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphCreateExt(ze_context_handle_t hContext, const void *pNext, ze_graph_handle_t *phGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListBeginGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext);
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListBeginCaptureIntoGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t hGraph, const void *pNext);
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListEndGraphCaptureExt(ze_command_list_handle_t hCommandList, const void *pNext, ze_graph_handle_t *phGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphInstantiateExt(ze_graph_handle_t hGraph, const void *pNext, ze_executable_graph_handle_t *phExecutableGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendGraphExt(ze_command_list_handle_t hCommandList, ze_executable_graph_handle_t hGraph, const void *pNext,
                                                                ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphDestroyExt(ze_graph_handle_t hGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeExecutableGraphDestroyExt(ze_executable_graph_handle_t hGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListIsGraphCaptureEnabledExt(ze_command_list_handle_t hCommandList);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphIsEmptyExt(ze_graph_handle_t hGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphDumpContentsExt(ze_graph_handle_t hGraph, const char *filePath, const void *pNext);
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListGetGraphExt(ze_command_list_handle_t hCommandList, ze_graph_handle_t *phGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphSetDestructionCallbackExt(ze_graph_handle_t hGraph, zex_mem_graph_free_callback_fn_t pfnCallback, void *pUserData, const void *pNext);
ZE_APIEXPORT ze_result_t ZE_APICALL zeExecutableGraphGetSourceGraphExt(ze_executable_graph_handle_t hGraph, ze_graph_handle_t *phSourceGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphGetPrimaryCommandListExt(ze_graph_handle_t hGraph, ze_command_list_handle_t *phCommandList);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphPauseCaptureExt(ze_graph_handle_t hGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphResumeCaptureExt(ze_graph_handle_t hGraph);
ZE_APIEXPORT ze_result_t ZE_APICALL zeGraphGetIdExt(ze_graph_handle_t hGraph, uint64_t *pGraphId);
}
