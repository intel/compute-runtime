/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/tracing/tracing_imp.h"

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemAllocSharedTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnAllocShared,
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
zeMemAllocDeviceTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnAllocDevice,
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
zeMemAllocHostTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnAllocHost,
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
zeMemFreeTracing(ze_context_handle_t hContext,
                 void *ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnFree,
                               hContext,
                               ptr);

    ze_mem_free_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnMemFreeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemFreeCb_t, Mem, pfnFreeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnFree,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAllocPropertiesTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnGetAllocProperties,
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
zeMemGetAddressRangeTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnGetAddressRange,
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
zeMemGetIpcHandleTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnGetIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.ppIpcHandle);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemOpenIpcHandleTracing(ze_context_handle_t hContext,
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

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnOpenIpcHandle,
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
zeMemCloseIpcHandleTracing(ze_context_handle_t hContext,
                           const void *ptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.Mem.pfnCloseIpcHandle,
                               hContext,
                               ptr);

    ze_mem_close_ipc_handle_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;

    L0::APITracerCallbackDataImp<ze_pfnMemCloseIpcHandleCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnMemCloseIpcHandleCb_t, Mem, pfnCloseIpcHandleCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.Mem.pfnCloseIpcHandle,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemReserveTracing(ze_context_handle_t hContext,
                           const void *pStart,
                           size_t size,
                           void **pptr) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnReserve,
                               hContext,
                               pStart,
                               size,
                               pptr);

    ze_virtual_mem_reserve_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.ppStart = &pStart;
    tracerParams.psize = &size;
    tracerParams.ppptr = &pptr;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemReserveCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemReserveCb_t, VirtualMem, pfnReserveCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnReserve,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.ppStart,
                                   *tracerParams.psize,
                                   *tracerParams.ppptr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemFreeTracing(ze_context_handle_t hContext,
                        const void *ptr,
                        size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnFree,
                               hContext,
                               ptr,
                               size);

    ze_virtual_mem_free_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemFreeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemFreeCb_t, VirtualMem, pfnFreeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnFree,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemQueryPageSizeTracing(ze_context_handle_t hContext,
                                 ze_device_handle_t hDevice,
                                 size_t size,
                                 size_t *pagesize) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnQueryPageSize,
                               hContext,
                               hDevice,
                               size,
                               pagesize);

    ze_virtual_mem_query_page_size_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.psize = &size;
    tracerParams.ppagesize = &pagesize;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemQueryPageSizeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemQueryPageSizeCb_t, VirtualMem, pfnQueryPageSizeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnQueryPageSize,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.psize,
                                   *tracerParams.ppagesize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemMapTracing(ze_context_handle_t hContext,
                       const void *ptr,
                       size_t size,
                       ze_physical_mem_handle_t hPhysicalMemory,
                       size_t offset,
                       ze_memory_access_attribute_t access) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnMap,
                               hContext,
                               ptr,
                               size,
                               hPhysicalMemory,
                               offset,
                               access);

    ze_virtual_mem_map_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;
    tracerParams.phPhysicalMemory = &hPhysicalMemory;
    tracerParams.poffset = &offset;
    tracerParams.paccess = &access;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemMapCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemMapCb_t, VirtualMem, pfnMapCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnMap,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.psize,
                                   *tracerParams.phPhysicalMemory,
                                   *tracerParams.poffset,
                                   *tracerParams.paccess);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemUnmapTracing(ze_context_handle_t hContext,
                         const void *ptr,
                         size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnUnmap,
                               hContext,
                               ptr,
                               size);

    ze_virtual_mem_unmap_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemUnmapCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemUnmapCb_t, VirtualMem, pfnUnmapCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnUnmap,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemSetAccessAttributeTracing(ze_context_handle_t hContext,
                                      const void *ptr,
                                      size_t size,
                                      ze_memory_access_attribute_t access) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnSetAccessAttribute,
                               hContext,
                               ptr,
                               size,
                               access);

    ze_virtual_mem_set_access_attribute_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;
    tracerParams.paccess = &access;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemSetAccessAttributeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemSetAccessAttributeCb_t, VirtualMem, pfnSetAccessAttributeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnSetAccessAttribute,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.psize,
                                   *tracerParams.paccess);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeVirtualMemGetAccessAttributeTracing(ze_context_handle_t hContext,
                                      const void *ptr,
                                      size_t size,
                                      ze_memory_access_attribute_t *access,
                                      size_t *outSize) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.VirtualMem.pfnGetAccessAttribute,
                               hContext,
                               ptr,
                               size,
                               access,
                               outSize);

    ze_virtual_mem_get_access_attribute_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;
    tracerParams.paccess = &access;
    tracerParams.poutSize = &outSize;

    L0::APITracerCallbackDataImp<ze_pfnVirtualMemGetAccessAttributeCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnVirtualMemGetAccessAttributeCb_t, VirtualMem, pfnGetAccessAttributeCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.VirtualMem.pfnGetAccessAttribute,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.pptr,
                                   *tracerParams.psize,
                                   *tracerParams.paccess,
                                   *tracerParams.poutSize);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zePhysicalMemCreateTracing(ze_context_handle_t hContext,
                           ze_device_handle_t hDevice,
                           ze_physical_mem_desc_t *desc,
                           ze_physical_mem_handle_t *phPhysicalMemory) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.PhysicalMem.pfnCreate,
                               hContext,
                               hDevice,
                               desc,
                               phPhysicalMemory);

    ze_physical_mem_create_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phDevice = &hDevice;
    tracerParams.pdesc = &desc;
    tracerParams.pphPhysicalMemory = &phPhysicalMemory;

    L0::APITracerCallbackDataImp<ze_pfnPhysicalMemCreateCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnPhysicalMemCreateCb_t, PhysicalMem, pfnCreateCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.PhysicalMem.pfnCreate,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phDevice,
                                   *tracerParams.pdesc,
                                   *tracerParams.pphPhysicalMemory);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zePhysicalMemDestroyTracing(ze_context_handle_t hContext,
                            ze_physical_mem_handle_t hPhysicalMemory) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.PhysicalMem.pfnDestroy,
                               hContext,
                               hPhysicalMemory);

    ze_physical_mem_destroy_params_t tracerParams;
    tracerParams.phContext = &hContext;
    tracerParams.phPhysicalMemory = &hPhysicalMemory;

    L0::APITracerCallbackDataImp<ze_pfnPhysicalMemDestroyCb_t> apiCallbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(apiCallbackData, ze_pfnPhysicalMemDestroyCb_t, PhysicalMem, pfnDestroyCb);

    return L0::apiTracerWrapperImp(driver_ddiTable.core_ddiTable.PhysicalMem.pfnDestroy,
                                   &tracerParams,
                                   apiCallbackData.apiOrdinal,
                                   apiCallbackData.prologCallbacks,
                                   apiCallbackData.epilogCallbacks,
                                   *tracerParams.phContext,
                                   *tracerParams.phPhysicalMemory);
}
