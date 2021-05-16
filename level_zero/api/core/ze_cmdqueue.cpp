/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *desc,
    ze_command_queue_handle_t *phCommandQueue) {
    return L0::Context::fromHandle(hContext)->createCommandQueue(hDevice, desc, phCommandQueue);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueDestroy(
    ze_command_queue_handle_t hCommandQueue) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueExecuteCommandLists(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->executeCommandLists(numCommandLists, phCommandLists, hFence, true);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandQueueSynchronize(
    ze_command_queue_handle_t hCommandQueue,
    uint64_t timeout) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->synchronize(timeout);
}
