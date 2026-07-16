/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeContextCreate(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    ze_context_handle_t *phContext);

ze_result_t ZE_APICALL zeContextCreateEx(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_context_handle_t *phContext);

ze_result_t ZE_APICALL zeContextDestroy(ze_context_handle_t hContext);

ze_result_t ZE_APICALL zeContextGetStatus(ze_context_handle_t hContext);

ze_result_t ZE_APICALL zeVirtualMemReserve(
    ze_context_handle_t hContext,
    const void *pStart,
    size_t size,
    void **pptr);

ze_result_t ZE_APICALL zeVirtualMemFree(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size);

ze_result_t ZE_APICALL zeVirtualMemQueryPageSize(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    size_t size,
    size_t *pagesize);

ze_result_t ZE_APICALL zePhysicalMemCreate(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_physical_mem_desc_t *desc,
    ze_physical_mem_handle_t *phPhysicalMemory);

ze_result_t ZE_APICALL zePhysicalMemDestroy(
    ze_context_handle_t hContext,
    ze_physical_mem_handle_t hPhysicalMemory);

ze_result_t ZE_APICALL zePhysicalMemGetProperties(
    ze_context_handle_t hContext,
    ze_physical_mem_handle_t hPhysicalMem,
    ze_physical_mem_properties_t *pMemProperties);

ze_result_t ZE_APICALL zeVirtualMemMap(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size,
    ze_physical_mem_handle_t hPhysicalMemory,
    size_t offset,
    ze_memory_access_attribute_t access);

ze_result_t ZE_APICALL zeVirtualMemUnmap(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size);

ze_result_t ZE_APICALL zeVirtualMemSetAccessAttribute(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size,
    ze_memory_access_attribute_t access);

ze_result_t ZE_APICALL zeVirtualMemGetAccessAttribute(
    ze_context_handle_t hContext,
    const void *ptr,
    size_t size,
    ze_memory_access_attribute_t *access,
    size_t *outSize);

ze_result_t ZE_APICALL zeContextSystemBarrier(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice);

ze_result_t ZE_APICALL zeContextMakeMemoryResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size);

ze_result_t ZE_APICALL zeContextEvictMemory(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size);

ze_result_t ZE_APICALL zeContextMakeImageResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage);

ze_result_t ZE_APICALL zeContextEvictImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextCreate(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    ze_context_handle_t *phContext);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextCreateEx(
    ze_driver_handle_t hDriver,
    const ze_context_desc_t *desc,
    uint32_t numDevices,
    ze_device_handle_t *phDevices,
    ze_context_handle_t *phContext);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextDestroy(
    ze_context_handle_t hContext);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextGetStatus(
    ze_context_handle_t hContext);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextMakeMemoryResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextEvictMemory(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    void *ptr,
    size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextMakeImageResident(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage);

ZE_APIEXPORT ze_result_t ZE_APICALL zeContextEvictImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    ze_image_handle_t hImage);

ZE_APIEXPORT ze_result_t ZE_APICALL zePhysicalMemGetProperties(
    ze_context_handle_t hContext,
    ze_physical_mem_handle_t hPhysicalMem,
    ze_physical_mem_properties_t *pMemProperties);

} // extern "C"
