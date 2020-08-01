/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverAllocSharedMem_Tracing(ze_driver_handle_t hDriver,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               const ze_host_mem_alloc_desc_t *hostDesc,
                               size_t size,
                               size_t alignment,
                               ze_device_handle_t hDevice,
                               void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverAllocDeviceMem_Tracing(ze_driver_handle_t hDriver,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment,
                               ze_device_handle_t hDevice,
                               void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverAllocHostMem_Tracing(ze_driver_handle_t hDriver,
                             const ze_host_mem_alloc_desc_t *hostDesc,
                             size_t size,
                             size_t alignment,
                             void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverFreeMem_Tracing(ze_driver_handle_t hDriver,
                        void *ptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetMemAllocProperties_Tracing(ze_driver_handle_t hDriver,
                                      const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetMemAddressRange_Tracing(ze_driver_handle_t hDriver,
                                   const void *ptr,
                                   void **pBase,
                                   size_t *pSize);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverGetMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                const void *ptr,
                                ze_ipc_mem_handle_t *pIpcHandle);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverOpenMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                 ze_device_handle_t hDevice,
                                 ze_ipc_mem_handle_t handle,
                                 ze_ipc_memory_flag_t flags,
                                 void **pptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverCloseMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                  const void *ptr);
}
