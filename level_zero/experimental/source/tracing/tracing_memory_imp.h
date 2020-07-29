/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocShared_Tracing(ze_context_handle_t hContext,
                         const ze_device_mem_alloc_desc_t *deviceDesc,
                         const ze_host_mem_alloc_desc_t *hostDesc,
                         size_t size,
                         size_t alignment,
                         ze_device_handle_t hDevice,
                         void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocDevice_Tracing(ze_context_handle_t hContext,
                         const ze_device_mem_alloc_desc_t *deviceDesc,
                         size_t size,
                         size_t alignment,
                         ze_device_handle_t hDevice,
                         void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocHost_Tracing(ze_context_handle_t hContext,
                       const ze_host_mem_alloc_desc_t *hostDesc,
                       size_t size,
                       size_t alignment,
                       void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemFree_Tracing(ze_context_handle_t hContext,
                  void *ptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAllocProperties_Tracing(ze_context_handle_t hContext,
                                const void *ptr,
                                ze_memory_allocation_properties_t *pMemAllocProperties,
                                ze_device_handle_t *phDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAddressRange_Tracing(ze_context_handle_t hContext,
                             const void *ptr,
                             void **pBase,
                             size_t *pSize);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetIpcHandle_Tracing(ze_context_handle_t hContext,
                          const void *ptr,
                          ze_ipc_mem_handle_t *pIpcHandle);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemOpenIpcHandle_Tracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           ze_ipc_mem_handle_t handle,
                           ze_ipc_memory_flags_t flags,
                           void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemCloseIpcHandle_Tracing(ze_context_handle_t hContext,
                            const void *ptr);
}
