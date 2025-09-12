/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeContextCreate(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    ze_context_handle_t *phContext) {
    return L0::DriverHandle::fromHandle(hDriver)->createContext(desc, 0u, nullptr, phContext);
}

ze_result_t zeContextCreateEx(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_context_handle_t *phContext) {
    return L0::DriverHandle::fromHandle(hDriver)->createContext(desc, numDevices, phDevices, phContext);
}

ze_result_t zeContextDestroy(ze_context_handle_t hContext) {
    return L0::Context::fromHandle(hContext)->destroy();
}

ze_result_t zeContextGetStatus(ze_context_handle_t hContext) {
    return L0::Context::fromHandle(hContext)->getStatus();
}

ze_result_t zeVirtualMemReserve(
    ze_context_handle_t hContext,
    const void *pStart,
    size_t size,
    void **pptr) {
    return L0::Context::fromHandle(hContext)->reserveVirtualMem(pStart, size, pptr);
}

ze_result_t zeVirtualMemFree(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size) {
    return L0::Context::fromHandle(hContext)->freeVirtualMem(ptr, size);
}

ze_result_t zeVirtualMemQueryPageSize(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    size_t size,
    size_t *pagesize) {
    return L0::Context::fromHandle(hContext)->queryVirtualMemPageSize(hDevice, size, pagesize);
}

ze_result_t zePhysicalMemCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_physical_mem_desc_t *desc,
    ze_physical_mem_handle_t *phPhysicalMemory) {
    return L0::Context::fromHandle(hContext)->createPhysicalMem(hDevice, desc, phPhysicalMemory);
}

ze_result_t zePhysicalMemDestroy(
    ze_context_handle_t hContext,
    ze_physical_mem_handle_t hPhysicalMemory) {
    return L0::Context::fromHandle(hContext)->destroyPhysicalMem(hPhysicalMemory);
}

ze_result_t zeVirtualMemMap(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size,
    ze_physical_mem_handle_t hPhysicalMemory,
    size_t offset,
    ze_memory_access_attribute_t access) {
    return L0::Context::fromHandle(hContext)->mapVirtualMem(ptr, size, hPhysicalMemory, offset, access);
}

ze_result_t zeVirtualMemUnmap(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size) {
    return L0::Context::fromHandle(hContext)->unMapVirtualMem(ptr, size);
}

ze_result_t zeVirtualMemSetAccessAttribute(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size,
    ze_memory_access_attribute_t access) {
    return L0::Context::fromHandle(hContext)->setVirtualMemAccessAttribute(ptr, size, access);
}

ze_result_t zeVirtualMemGetAccessAttribute(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size,
    ze_memory_access_attribute_t *access,
    size_t *outSize) {
    return L0::Context::fromHandle(hContext)->getVirtualMemAccessAttribute(ptr, size, access, outSize);
}

ze_result_t zeContextSystemBarrier(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t zeContextMakeMemoryResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::Context::fromHandle(hContext)->makeMemoryResident(hDevice, ptr, size);
}

ze_result_t zeContextEvictMemory(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::Context::fromHandle(hContext)->evictMemory(hDevice, ptr, size);
}

ze_result_t zeContextMakeImageResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::Context::fromHandle(hContext)->makeImageResident(hDevice, hImage);
}

ze_result_t zeContextEvictImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::Context::fromHandle(hContext)->evictImage(hDevice, hImage);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeContextCreate(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    ze_context_handle_t *phContext) {
    return L0::zeContextCreate(
        hDriver,
        desc,
        phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextCreateEx(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_context_handle_t *phContext) {
    return L0::zeContextCreateEx(
        hDriver,
        desc,
        numDevices,
        phDevices,
        phContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextDestroy(
    ze_context_handle_t hContext) {
    return L0::zeContextDestroy(
        hContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextGetStatus(
    ze_context_handle_t hContext) {
    return L0::zeContextGetStatus(
        hContext);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextMakeMemoryResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::zeContextMakeMemoryResident(hContext, hDevice, ptr, size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextEvictMemory(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size) {
    return L0::zeContextEvictMemory(hContext, hDevice, ptr, size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextMakeImageResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::zeContextMakeImageResident(hContext, hDevice, hImage);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextEvictImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage) {
    return L0::zeContextEvictImage(hContext, hDevice, hImage);
}
}
