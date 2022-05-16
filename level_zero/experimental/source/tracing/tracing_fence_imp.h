/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceCreateTracing(ze_command_queue_handle_t hCommandQueue,
                     const ze_fence_desc_t *desc,
                     ze_fence_handle_t *phFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceDestroyTracing(ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceHostSynchronizeTracing(ze_fence_handle_t hFence,
                              uint64_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceQueryStatusTracing(ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceResetTracing(ze_fence_handle_t hFence);
}
