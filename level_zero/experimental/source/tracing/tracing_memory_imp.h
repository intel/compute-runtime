/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeDriverAllocSharedMem_Tracing(ze_driver_handle_t hDriver,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               const ze_host_mem_alloc_desc_t *hostDesc,
                               size_t size,
                               size_t alignment,
                               ze_device_handle_t hDevice,
                               void **pptr);

__zedllexport ze_result_t __zecall
zeDriverAllocDeviceMem_Tracing(ze_driver_handle_t hDriver,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment,
                               ze_device_handle_t hDevice,
                               void **pptr);

__zedllexport ze_result_t __zecall
zeDriverAllocHostMem_Tracing(ze_driver_handle_t hDriver,
                             const ze_host_mem_alloc_desc_t *hostDesc,
                             size_t size,
                             size_t alignment,
                             void **pptr);

__zedllexport ze_result_t __zecall
zeDriverFreeMem_Tracing(ze_driver_handle_t hDriver,
                        void *ptr);

__zedllexport ze_result_t __zecall
zeDriverGetMemAllocProperties_Tracing(ze_driver_handle_t hDriver,
                                      const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice);

__zedllexport ze_result_t __zecall
zeDriverGetMemAddressRange_Tracing(ze_driver_handle_t hDriver,
                                   const void *ptr,
                                   void **pBase,
                                   size_t *pSize);

__zedllexport ze_result_t __zecall
zeDriverGetMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                const void *ptr,
                                ze_ipc_mem_handle_t *pIpcHandle);

__zedllexport ze_result_t __zecall
zeDriverOpenMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                 ze_device_handle_t hDevice,
                                 ze_ipc_mem_handle_t handle,
                                 ze_ipc_memory_flag_t flags,
                                 void **pptr);

__zedllexport ze_result_t __zecall
zeDriverCloseMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                  const void *ptr);
}
