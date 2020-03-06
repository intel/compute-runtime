/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeDriverAllocSharedMem_Tracing(ze_driver_handle_t hDriver,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               const ze_host_mem_alloc_desc_t *hostDesc,
                               size_t size,
                               size_t alignment,
                               ze_device_handle_t hDevice,
                               void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnAllocSharedMem,
                               hDriver,
                               deviceDesc,
                               hostDesc,
                               size,
                               alignment,
                               hDevice,
                               pptr);

    ze_driver_alloc_shared_mem_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pdevice_desc = &deviceDesc;
    tracerParams.phost_desc = &hostDesc;
    tracerParams.psize = &size;
    tracerParams.palignment = &alignment;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnDriverAllocSharedMemCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverAllocSharedMemCb_t, Driver, pfnAllocSharedMemCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnAllocSharedMem,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pdevice_desc,
                                   *tracerParams.phost_desc,
                                   *tracerParams.psize,
                                   *tracerParams.palignment,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppptr);
}

__zedllexport ze_result_t __zecall
zeDriverAllocDeviceMem_Tracing(ze_driver_handle_t hDriver,
                               const ze_device_mem_alloc_desc_t *deviceDesc,
                               size_t size,
                               size_t alignment,
                               ze_device_handle_t hDevice,
                               void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnAllocDeviceMem,
                               hDriver,
                               deviceDesc,
                               size,
                               alignment,
                               hDevice,
                               pptr);

    ze_driver_alloc_device_mem_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pdevice_desc = &deviceDesc;
    tracerParams.psize = &size;
    tracerParams.palignment = &alignment;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnDriverAllocDeviceMemCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverAllocDeviceMemCb_t, Driver, pfnAllocDeviceMemCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnAllocDeviceMem,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pdevice_desc,
                                   *tracerParams.psize,
                                   *tracerParams.palignment,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppptr);
}

__zedllexport ze_result_t __zecall
zeDriverAllocHostMem_Tracing(ze_driver_handle_t hDriver,
                             const ze_host_mem_alloc_desc_t *hostDesc,
                             size_t size,
                             size_t alignment,
                             void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnAllocHostMem,
                               hDriver,
                               hostDesc,
                               size,
                               alignment,
                               pptr);

    ze_driver_alloc_host_mem_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.phost_desc = &hostDesc;
    tracerParams.psize = &size;
    tracerParams.palignment = &alignment;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnDriverAllocHostMemCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverAllocHostMemCb_t, Driver, pfnAllocHostMemCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnAllocHostMem,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.phost_desc,
                                   *tracerParams.psize,
                                   *tracerParams.palignment,
                                   *tracerParams.ppptr);
}

__zedllexport ze_result_t __zecall
zeDriverFreeMem_Tracing(ze_driver_handle_t hDriver,
                        void *ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnFreeMem,
                               hDriver,
                               ptr);

    ze_driver_free_mem_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnDriverFreeMemCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverFreeMemCb_t, Driver, pfnFreeMemCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnFreeMem,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pptr);
}

__zedllexport ze_result_t __zecall
zeDriverGetMemAllocProperties_Tracing(ze_driver_handle_t hDriver,
                                      const void *ptr,
                                      ze_memory_allocation_properties_t *pMemAllocProperties,
                                      ze_device_handle_t *phDevice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetMemAllocProperties,
                               hDriver,
                               ptr,
                               pMemAllocProperties,
                               phDevice);

    ze_driver_get_mem_alloc_properties_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pptr = &ptr;
    tracerParams.ppMemAllocProperties = &pMemAllocProperties;
    tracerParams.pphDevice = &phDevice;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetMemAllocPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetMemAllocPropertiesCb_t, Driver, pfnGetMemAllocPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetMemAllocProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pptr,
                                   *tracerParams.ppMemAllocProperties,
                                   *tracerParams.pphDevice);
}

__zedllexport ze_result_t __zecall
zeDriverGetMemAddressRange_Tracing(ze_driver_handle_t hDriver,
                                   const void *ptr,
                                   void **pBase,
                                   size_t *pSize) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetMemAddressRange,
                               hDriver,
                               ptr,
                               pBase,
                               pSize);

    ze_driver_get_mem_address_range_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pptr = &ptr;
    tracerParams.ppBase = &pBase;
    tracerParams.ppSize = &pSize;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetMemAddressRangeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetMemAddressRangeCb_t, Driver, pfnGetMemAddressRangeCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetMemAddressRange,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pptr,
                                   *tracerParams.ppBase,
                                   *tracerParams.ppSize);
}

__zedllexport ze_result_t __zecall
zeDriverGetMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                const void *ptr,
                                ze_ipc_mem_handle_t *pIpcHandle) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnGetMemIpcHandle,
                               hDriver,
                               ptr,
                               pIpcHandle);

    ze_driver_get_mem_ipc_handle_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pptr = &ptr;
    tracerParams.ppIpcHandle = &pIpcHandle;

    L0::APITracerCallbackDataImp<ze_pfnDriverGetMemIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverGetMemIpcHandleCb_t, Driver, pfnGetMemIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnGetMemIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pptr,
                                   *tracerParams.ppIpcHandle);
}

__zedllexport ze_result_t __zecall
zeDriverOpenMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                 ze_device_handle_t hDevice,
                                 ze_ipc_mem_handle_t handle,
                                 ze_ipc_memory_flag_t flags,
                                 void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnOpenMemIpcHandle, hDriver, hDevice, handle, flags, pptr);

    ze_driver_open_mem_ipc_handle_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.phDevice = &hDevice;
    tracerParams.phandle = &handle;
    tracerParams.pflags = &flags;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnDriverOpenMemIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverOpenMemIpcHandleCb_t, Driver, pfnOpenMemIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnOpenMemIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.phDevice,
                                   *tracerParams.phandle,
                                   *tracerParams.pflags,
                                   *tracerParams.ppptr);
}

__zedllexport ze_result_t __zecall
zeDriverCloseMemIpcHandle_Tracing(ze_driver_handle_t hDriver,
                                  const void *ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Driver.pfnCloseMemIpcHandle,
                               hDriver,
                               ptr);

    ze_driver_close_mem_ipc_handle_params_t tracerParams;
    tracerParams.phDriver = &hDriver;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnDriverCloseMemIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnDriverCloseMemIpcHandleCb_t, Driver, pfnCloseMemIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Driver.pfnCloseMemIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phDriver,
                                   *tracerParams.pptr);
}
