/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fence.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->createFence(desc, phFence);
}

__zedllexport ze_result_t __zecall
zeFenceDestroy(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->destroy();
}

__zedllexport ze_result_t __zecall
zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint32_t timeout) {
    return L0::Fence::fromHandle(hFence)->hostSynchronize(timeout);
}

__zedllexport ze_result_t __zecall
zeFenceQueryStatus(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->queryStatus();
}

__zedllexport ze_result_t __zecall
zeFenceReset(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->reset();
}

} // extern "C"
