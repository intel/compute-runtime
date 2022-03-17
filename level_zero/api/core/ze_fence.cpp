/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fence/fence.h"
#include <level_zero/ze_api.h>

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->createFence(desc, phFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceDestroy(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->destroy();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint64_t timeout) {
    return L0::Fence::fromHandle(hFence)->hostSynchronize(timeout);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceQueryStatus(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->queryStatus();
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceReset(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->reset(false);
}
