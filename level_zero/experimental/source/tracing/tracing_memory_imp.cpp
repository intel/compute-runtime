/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocShared_Tracing(ze_context_handle_t hContext,
                         const ze_device_mem_alloc_desc_t *deviceDesc,
                         const ze_host_mem_alloc_desc_t *hostDesc,
                         size_t size,
                         size_t alignment,
                         ze_device_handle_t hDevice,
                         void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnAllocShared,
                               hContext,
                               deviceDesc,
                               hostDesc,
                               size,
                               alignment,
                               hDevice,
                               pptr);

    ze_mem_alloc_shared_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pdevice_desc = &deviceDesc;
    tracerParams.phost_desc = &hostDesc;
    tracerParams.psize = &size;
    tracerParams.palignment = &alignment;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnMemAllocSharedCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemAllocSharedCb_t, Mem, pfnAllocSharedCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnAllocShared,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pdevice_desc,
                                   *tracerParams.phost_desc,
                                   *tracerParams.psize,
                                   *tracerParams.palignment,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocDevice_Tracing(ze_context_handle_t hContext,
                         const ze_device_mem_alloc_desc_t *deviceDesc,
                         size_t size,
                         size_t alignment,
                         ze_device_handle_t hDevice,
                         void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnAllocDevice,
                               hContext,
                               deviceDesc,
                               size,
                               alignment,
                               hDevice,
                               pptr);

    ze_mem_alloc_device_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pdevice_desc = &deviceDesc;
    tracerParams.psize = &size;
    tracerParams.palignment = &alignment;
    tracerParams.phDevice = &hDevice;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnMemAllocDeviceCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemAllocDeviceCb_t, Mem, pfnAllocDeviceCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnAllocDevice,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pdevice_desc,
                                   *tracerParams.psize,
                                   *tracerParams.palignment,
                                   *tracerParams.phDevice,
                                   *tracerParams.ppptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocHost_Tracing(ze_context_handle_t hContext,
                       const ze_host_mem_alloc_desc_t *hostDesc,
                       size_t size,
                       size_t alignment,
                       void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnAllocHost,
                               hContext,
                               hostDesc,
                               size,
                               alignment,
                               pptr);

    ze_mem_alloc_host_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phost_desc = &hostDesc;
    tracerParams.psize = &size;
    tracerParams.palignment = &alignment;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnMemAllocHostCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemAllocHostCb_t, Mem, pfnAllocHostCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnAllocHost,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phost_desc,
                                   *tracerParams.psize,
                                   *tracerParams.palignment,
                                   *tracerParams.ppptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemFree_Tracing(ze_context_handle_t hContext,
                  void *ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnFree,
                               hContext,
                               ptr);

    ze_mem_free_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnMemFreeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemFreeCb_t, Mem, pfnFreeCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnFree,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAllocProperties_Tracing(ze_context_handle_t hContext,
                                const void *ptr,
                                ze_memory_allocation_properties_t *pMemAllocProperties,
                                ze_device_handle_t *phDevice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnGetAllocProperties,
                               hContext,
                               ptr,
                               pMemAllocProperties,
                               phDevice);

    ze_mem_get_alloc_properties_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.ppMemAllocProperties = &pMemAllocProperties;
    tracerParams.pphDevice = &phDevice;

    L0::APITracerCallbackDataImp<ze_pfnMemGetAllocPropertiesCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemGetAllocPropertiesCb_t, Mem, pfnGetAllocPropertiesCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnGetAllocProperties,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.ppMemAllocProperties,
                                   *tracerParams.pphDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAddressRange_Tracing(ze_context_handle_t hContext,
                             const void *ptr,
                             void **pBase,
                             size_t *pSize) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnGetAddressRange,
                               hContext,
                               ptr,
                               pBase,
                               pSize);

    ze_mem_get_address_range_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.ppBase = &pBase;
    tracerParams.ppSize = &pSize;

    L0::APITracerCallbackDataImp<ze_pfnMemGetAddressRangeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemGetAddressRangeCb_t, Mem, pfnGetAddressRangeCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnGetAddressRange,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.ppBase,
                                   *tracerParams.ppSize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetIpcHandle_Tracing(ze_context_handle_t hContext,
                          const void *ptr,
                          ze_ipc_mem_handle_t *pIpcHandle) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnGetIpcHandle,
                               hContext,
                               ptr,
                               pIpcHandle);

    ze_mem_get_ipc_handle_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.ppIpcHandle = &pIpcHandle;

    L0::APITracerCallbackDataImp<ze_pfnMemGetIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemGetIpcHandleCb_t, Mem, pfnGetIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnGetIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.ppIpcHandle);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemOpenIpcHandle_Tracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           ze_ipc_mem_handle_t handle,
                           ze_ipc_memory_flags_t flags,
                           void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnOpenIpcHandle,
                               hContext,
                               hDevice,
                               handle,
                               flags,
                               pptr);

    ze_mem_open_ipc_handle_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.phandle = &handle;
    tracerParams.pflags = &flags;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnMemOpenIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemOpenIpcHandleCb_t, Mem, pfnOpenIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnOpenIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.phandle,
                                   *tracerParams.pflags,
                                   *tracerParams.ppptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemCloseIpcHandle_Tracing(ze_context_handle_t hContext,
                            const void *ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnCloseIpcHandle,
                               hContext,
                               ptr);

    ze_mem_close_ipc_handle_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnMemCloseIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemCloseIpcHandleCb_t, Mem, pfnCloseIpcHandleCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnCloseIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr);
}
