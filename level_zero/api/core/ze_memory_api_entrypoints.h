/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeMemAllocShared(
    ze_context_handle_t hContext,
    const ze_device_mem_alloc_desc_t *deviceDesc,
    const ze_host_mem_alloc_desc_t *hostDesc,
    size_t size,
    size_t alignment,
    ze_device_handle_t hDevice,
    void **pptr) {
    return L0::Context::fromHandle(hContext)->allocSharedMem(hDevice, deviceDesc, hostDesc, size, alignment, pptr);
}

ze_result_t zeMemAllocDevice(
    ze_context_handle_t hContext,
    const ze_device_mem_alloc_desc_t *deviceDesc,
    size_t size,
    size_t alignment,
    ze_device_handle_t hDevice,
    void **pptr) {
    return L0::Context::fromHandle(hContext)->allocDeviceMem(hDevice, deviceDesc, size, alignment, pptr);
}

ze_result_t zeMemAllocHost(
    ze_context_handle_t hContext,
    const ze_host_mem_alloc_desc_t *hostDesc,
    size_t size,
    size_t alignment,
    void **pptr) {
    return L0::Context::fromHandle(hContext)->allocHostMem(hostDesc, size, alignment, pptr);
}

ze_result_t zeMemFree(
    ze_context_handle_t hContext,
    void *ptr) {
    return L0::Context::fromHandle(hContext)->freeMem(ptr);
}

ze_result_t zeMemFreeExt(
    ze_context_handle_t hContext,
    const ze_memory_free_ext_desc_t *pMemFreeDesc,
    void *ptr) {
    return L0::Context::fromHandle(hContext)->freeMemExt(pMemFreeDesc, ptr);
}

ze_result_t zeMemGetAllocProperties(
    ze_context_handle_t hContext,
    const void *ptr,
    ze_memory_allocation_properties_t *pMemAllocProperties,
    ze_device_handle_t *phDevice) {
    return L0::Context::fromHandle(hContext)->getMemAllocProperties(ptr, pMemAllocProperties, phDevice);
}

ze_result_t zeMemGetAddressRange(
    ze_context_handle_t hContext,
    const void *ptr,
    void **pBase,
    size_t *pSize) {
    return L0::Context::fromHandle(hContext)->getMemAddressRange(ptr, pBase, pSize);
}

ze_result_t zeMemGetIpcHandle(
    ze_context_handle_t hContext,
    const void *ptr,
    ze_ipc_mem_handle_t *pIpcHandle) {
    return L0::Context::fromHandle(hContext)->getIpcMemHandle(ptr, pIpcHandle);
}

ze_result_t zeMemPutIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_mem_handle_t ipcHandle) {
    return L0::Context::fromHandle(hContext)->putIpcMemHandle(ipcHandle);
}

ze_result_t zeMemOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_ipc_mem_handle_t handle,
    ze_ipc_memory_flags_t flags,
    void **pptr) {
    return L0::Context::fromHandle(hContext)->openIpcMemHandle(hDevice, handle, flags, pptr);
}

ze_result_t zeMemCloseIpcHandle(
    ze_context_handle_t hContext,
    const void *ptr) {
    return L0::Context::fromHandle(hContext)->closeIpcMemHandle(ptr);
}

ze_result_t zeMemGetIpcHandleFromFileDescriptorExp(ze_context_handle_t hContext, uint64_t handle, ze_ipc_mem_handle_t *pIpcHandle) {
    return L0::Context::fromHandle(hContext)->getIpcHandleFromFd(handle, pIpcHandle);
}

ze_result_t zeMemGetFileDescriptorFromIpcHandleExp(ze_context_handle_t hContext, ze_ipc_mem_handle_t ipcHandle, uint64_t *pHandle) {
    return L0::Context::fromHandle(hContext)->getFdFromIpcHandle(ipcHandle, pHandle);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemAllocShared(
    ze_context_handle_t hContext,
    const ze_device_mem_alloc_desc_t *deviceDesc,
    const ze_host_mem_alloc_desc_t *hostDesc,
    size_t size,
    size_t alignment,
    ze_device_handle_t hDevice,
    void **pptr) {
    return L0::zeMemAllocShared(
        hContext,
        deviceDesc,
        hostDesc,
        size,
        alignment,
        hDevice,
        pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemAllocDevice(
    ze_context_handle_t hContext,
    const ze_device_mem_alloc_desc_t *deviceDesc,
    size_t size,
    size_t alignment,
    ze_device_handle_t hDevice,
    void **pptr) {
    return L0::zeMemAllocDevice(
        hContext,
        deviceDesc,
        size,
        alignment,
        hDevice,
        pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemAllocHost(
    ze_context_handle_t hContext,
    const ze_host_mem_alloc_desc_t *hostDesc,
    size_t size,
    size_t alignment,
    void **pptr) {
    return L0::zeMemAllocHost(
        hContext,
        hostDesc,
        size,
        alignment,
        pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemFree(
    ze_context_handle_t hContext,
    void *ptr) {
    return L0::zeMemFree(
        hContext,
        ptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemGetAllocProperties(
    ze_context_handle_t hContext,
    const void *ptr,
    ze_memory_allocation_properties_t *pMemAllocProperties,
    ze_device_handle_t *phDevice) {
    return L0::zeMemGetAllocProperties(
        hContext,
        ptr,
        pMemAllocProperties,
        phDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemGetAddressRange(
    ze_context_handle_t hContext,
    const void *ptr,
    void **pBase,
    size_t *pSize) {
    return L0::zeMemGetAddressRange(
        hContext,
        ptr,
        pBase,
        pSize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemGetIpcHandle(
    ze_context_handle_t hContext,
    const void *ptr,
    ze_ipc_mem_handle_t *pIpcHandle) {
    return L0::zeMemGetIpcHandle(
        hContext,
        ptr,
        pIpcHandle);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemOpenIpcHandle(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_ipc_mem_handle_t handle,
    ze_ipc_memory_flags_t flags,
    void **pptr) {
    return L0::zeMemOpenIpcHandle(
        hContext,
        hDevice,
        handle,
        flags,
        pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemPutIpcHandle(
    ze_context_handle_t hContext,
    ze_ipc_mem_handle_t ipcHandle) {
    return L0::zeMemPutIpcHandle(
        hContext,
        ipcHandle);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemCloseIpcHandle(
    ze_context_handle_t hContext,
    const void *ptr) {
    return L0::zeMemCloseIpcHandle(
        hContext,
        ptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemGetIpcHandleFromFileDescriptorExp(
    ze_context_handle_t hContext,
    uint64_t handle,
    ze_ipc_mem_handle_t *pIpcHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemGetFileDescriptorFromIpcHandleExp(
    ze_context_handle_t hContext,
    ze_ipc_mem_handle_t ipcHandle,
    uint64_t *pHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
}
