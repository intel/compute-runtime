/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdqueue.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandQueueCreate(
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *desc,
    ze_command_queue_handle_t *phCommandQueue) {
    return L0::Device::fromHandle(hDevice)->createCommandQueue(desc, phCommandQueue);
}

__zedllexport ze_result_t __zecall
zeCommandQueueDestroy(
    ze_command_queue_handle_t hCommandQueue) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->destroy();
}

__zedllexport ze_result_t __zecall
zeCommandQueueExecuteCommandLists(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->executeCommandLists(numCommandLists, phCommandLists, hFence, true);
}

__zedllexport ze_result_t __zecall
zeCommandQueueSynchronize(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t timeout) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->synchronize(timeout);
}

} // extern "C"
