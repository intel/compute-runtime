/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeFenceCreate_Tracing(ze_command_queue_handle_t hCommandQueue,
                      const ze_fence_desc_t *desc,
                      ze_fence_handle_t *phFence);

__zedllexport ze_result_t __zecall
zeFenceDestroy_Tracing(ze_fence_handle_t hFence);

__zedllexport ze_result_t __zecall
zeFenceHostSynchronize_Tracing(ze_fence_handle_t hFence,
                               uint32_t timeout);

__zedllexport ze_result_t __zecall
zeFenceQueryStatus_Tracing(ze_fence_handle_t hFence);

__zedllexport ze_result_t __zecall
zeFenceReset_Tracing(ze_fence_handle_t hFence);
}
