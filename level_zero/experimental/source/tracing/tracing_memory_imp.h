/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocSharedTracing(ze_context_handle_t hContext,
                        const ze_device_mem_alloc_desc_t *deviceDesc,
                        const ze_host_mem_alloc_desc_t *hostDesc,
                        size_t size,
                        size_t alignment,
                        ze_device_handle_t hDevice,
                        void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocDeviceTracing(ze_context_handle_t hContext,
                        const ze_device_mem_alloc_desc_t *deviceDesc,
                        size_t size,
                        size_t alignment,
                        ze_device_handle_t hDevice,
                        void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocHostTracing(ze_context_handle_t hContext,
                      const ze_host_mem_alloc_desc_t *hostDesc,
                      size_t size,
                      size_t alignment,
                      void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemFreeTracing(ze_context_handle_t hContext,
                 void *ptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAllocPropertiesTracing(ze_context_handle_t hContext,
                               const void *ptr,
                               ze_memory_allocation_properties_t *pMemAllocProperties,
                               ze_device_handle_t *phDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAddressRangeTracing(ze_context_handle_t hContext,
                            const void *ptr,
                            void **pBase,
                            size_t *pSize);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetIpcHandleTracing(ze_context_handle_t hContext,
                         const void *ptr,
                         ze_ipc_mem_handle_t *pIpcHandle);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemOpenIpcHandleTracing(ze_context_handle_t hContext,
                          ze_device_handle_t hDevice,
                          ze_ipc_mem_handle_t handle,
                          ze_ipc_memory_flags_t flags,
                          void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemCloseIpcHandleTracing(ze_context_handle_t hContext,
                           const void *ptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemReserveTracing(ze_context_handle_t hContext,
                           const void *pStart,
                           size_t size,
                           void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemFreeTracing(ze_context_handle_t hContext,
                        const void *ptr,
                        size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemQueryPageSizeTracing(ze_context_handle_t hContext,
                                 ze_device_handle_t hDevice,
                                 size_t size,
                                 size_t *pagesize);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemMapTracing(ze_context_handle_t hContext,
                       const void *ptr,
                       size_t size,
                       ze_physical_mem_handle_t hPhysicalMemory,
                       size_t offset,
                       ze_memory_access_attribute_t access);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemUnmapTracing(ze_context_handle_t hContext,
                         const void *ptr,
                         size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemSetAccessAttributeTracing(ze_context_handle_t hContext,
                                      const void *ptr,
                                      size_t size,
                                      ze_memory_access_attribute_t access);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemGetAccessAttributeTracing(ze_context_handle_t hContext,
                                      const void *ptr,
                                      size_t size,
                                      ze_memory_access_attribute_t *access,
                                      size_t *outSize);

ZE_APIEXPORT ze_result_t ZE_APICALL
zePhysicalMemCreateTracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           ze_physical_mem_desc_t *desc,
                           ze_physical_mem_handle_t *phPhysicalMemory);

ZE_APIEXPORT ze_result_t ZE_APICALL
zePhysicalMemDestroyTracing(ze_context_handle_t hContext,
                            ze_physical_mem_handle_t hPhysicalMemory);
}
