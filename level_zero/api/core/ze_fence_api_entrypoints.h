/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence);

ze_result_t ZE_APICALL zeFenceDestroy(
    ze_fence_handle_t hFence);

ze_result_t ZE_APICALL zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint64_t timeout);

ze_result_t ZE_APICALL zeFenceQueryStatus(
    ze_fence_handle_t hFence);

ze_result_t ZE_APICALL zeFenceReset(
    ze_fence_handle_t hFence);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence);

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceDestroy(
    ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint64_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceQueryStatus(
    ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL zeFenceReset(
    ze_fence_handle_t hFence);

} // extern "C"
