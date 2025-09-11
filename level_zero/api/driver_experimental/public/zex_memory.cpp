/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/driver_experimental/zex_api.h"

namespace L0 {

ze_result_t ZE_APICALL
zexMemGetIpcHandles(
    ze_context_handle_t hContext,
    const void *ptr,
    uint32_t *numIpcHandles,
    ze_ipc_mem_handle_t *pIpcHandles) {
    return L0::Context::fromHandle(toInternalType(hContext))->getIpcMemHandles(ptr, numIpcHandles, pIpcHandles);
}

ze_result_t ZE_APICALL
zexMemOpenIpcHandles(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    uint32_t numIpcHandles,
    ze_ipc_mem_handle_t *pIpcHandles,
    ze_ipc_memory_flags_t flags,
    void **pptr) {
    return L0::Context::fromHandle(toInternalType(hContext))->openIpcMemHandles(toInternalType(hDevice), numIpcHandles, pIpcHandles, flags, pptr);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexMemGetIpcHandles(
    ze_context_handle_t hContext,
    const void *ptr,
    uint32_t *numIpcHandles,
    ze_ipc_mem_handle_t *pIpcHandles) {
    return L0::zexMemGetIpcHandles(hContext, ptr, numIpcHandles, pIpcHandles);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexMemOpenIpcHandles(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    uint32_t numIpcHandles,
    ze_ipc_mem_handle_t *pIpcHandles,
    ze_ipc_memory_flags_t flags,
    void **pptr) {
    return L0::zexMemOpenIpcHandles(hContext, hDevice, numIpcHandles, pIpcHandles, flags, pptr);
}
} // extern "C"
