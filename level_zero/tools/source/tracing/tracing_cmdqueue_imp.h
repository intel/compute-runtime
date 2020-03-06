/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandQueueCreate_Tracing(ze_device_handle_t hDevice,
                             const ze_command_queue_desc_t *desc,
                             ze_command_queue_handle_t *phCommandQueue);

__zedllexport ze_result_t __zecall
zeCommandQueueDestroy_Tracing(ze_command_queue_handle_t hCommandQueue);

__zedllexport ze_result_t __zecall
zeCommandQueueExecuteCommandLists_Tracing(ze_command_queue_handle_t hCommandQueue,
                                          uint32_t numCommandLists,
                                          ze_command_list_handle_t *phCommandLists,
                                          ze_fence_handle_t hFence);

__zedllexport ze_result_t __zecall
zeCommandQueueSynchronize_Tracing(ze_command_queue_handle_t hCommandQueue,
                                  uint32_t timeout);
}
