/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceCreate_Tracing(ze_command_queue_handle_t hCommandQueue,
                      const ze_fence_desc_t *desc,
                      ze_fence_handle_t *phFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceDestroy_Tracing(ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceHostSynchronize_Tracing(ze_fence_handle_t hFence,
                               uint64_t timeout);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceQueryStatus_Tracing(ze_fence_handle_t hFence);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFenceReset_Tracing(ze_fence_handle_t hFence);
}
