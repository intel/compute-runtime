/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueCreateTracing(ze_context_handle_t hContext,
                            ze_device_handle_t hDevice,
                            const ze_command_queue_desc_t *desc,
                            ze_command_queue_handle_t *phCommandQueue);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueDestroyTracing(ze_command_queue_handle_t hCommandQueue);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueExecuteCommandListsTracing(ze_command_queue_handle_t hCommandQueue,
                                         uint32_t numCommandLists,
                                         ze_command_list_handle_t *phCommandLists,
                                         ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueSynchronizeTracing(ze_command_queue_handle_t hCommandQueue,
                                 uint64_t timeout);
}
