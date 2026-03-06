/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/zex_api.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL
zexMemGetIpcHandles(
    ze_context_handle_t hContext,
    const void *ptr,
    uint32_t *numIpcHandles,
    ze_ipc_mem_handle_t *pIpcHandles);

ze_result_t ZE_APICALL
zexMemOpenIpcHandles(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    uint32_t numIpcHandles,
    ze_ipc_mem_handle_t *pIpcHandles,
    ze_ipc_memory_flags_t flags,
    void **pptr);

ze_result_t ZE_APICALL
zeIntelMemMapDeviceMemToHost(
    ze_context_handle_t hContext,
    const void *ptr,
    void **pptr,
    void *pNext);

} // namespace L0
