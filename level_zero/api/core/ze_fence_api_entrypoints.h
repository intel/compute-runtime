/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/fence/fence.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence) {
    return L0::CommandQueue::fromHandle(hCommandQueue)->createFence(desc, phFence);
}

ze_result_t zeFenceDestroy(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->destroy();
}

ze_result_t zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint64_t timeout) {
    return L0::Fence::fromHandle(hFence)->hostSynchronize(timeout);
}

ze_result_t zeFenceQueryStatus(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->queryStatus();
}

ze_result_t zeFenceReset(
    ze_fence_handle_t hFence) {
    return L0::Fence::fromHandle(hFence)->reset(false);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence) {
    return L0::zeFenceCreate(
        hCommandQueue,
        desc,
        phFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceDestroy(
    ze_fence_handle_t hFence) {
    return L0::zeFenceDestroy(
        hFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint64_t timeout) {
    return L0::zeFenceHostSynchronize(
        hFence,
        timeout);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceQueryStatus(
    ze_fence_handle_t hFence) {
    return L0::zeFenceQueryStatus(
        hFence);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceReset(
    ze_fence_handle_t hFence) {
    return L0::zeFenceReset(
        hFence);
}
}
