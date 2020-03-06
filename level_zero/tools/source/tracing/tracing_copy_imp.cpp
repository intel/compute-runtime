/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/tracing/tracing_imp.h"

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryCopy_Tracing(ze_command_list_handle_t hCommandList,
                                      void *dstptr,
                                      const void *srcptr,
                                      size_t size,
                                      ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopy,
                               hCommandList,
                               dstptr,
                               srcptr,
                               size,
                               hEvent);

    ze_command_list_append_memory_copy_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pdstptr = &dstptr;
    tracerParams.psrcptr = &srcptr;
    tracerParams.psize = &size;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendMemoryCopyCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendMemoryCopyCb_t, CommandList, pfnAppendMemoryCopyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopy,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pdstptr,
                                   *tracerParams.psrcptr,
                                   *tracerParams.psize,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryFill_Tracing(ze_command_list_handle_t hCommandList,
                                      void *ptr,
                                      const void *pattern,
                                      size_t patternSize,
                                      size_t size,
                                      ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryFill,
                               hCommandList,
                               ptr,
                               pattern,
                               patternSize,
                               size,
                               hEvent);

    ze_command_list_append_memory_fill_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pptr = &ptr;
    tracerParams.ppattern = &pattern;
    tracerParams.ppattern_size = &patternSize;
    tracerParams.psize = &size;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendMemoryFillCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendMemoryFillCb_t, CommandList, pfnAppendMemoryFillCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryFill,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pptr,
                                   *tracerParams.ppattern,
                                   *tracerParams.ppattern_size,
                                   *tracerParams.psize,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryCopyRegion_Tracing(ze_command_list_handle_t hCommandList,
                                            void *dstptr,
                                            const ze_copy_region_t *dstRegion,
                                            uint32_t dstPitch,
                                            uint32_t dstSlicePitch,
                                            const void *srcptr,
                                            const ze_copy_region_t *srcRegion,
                                            uint32_t srcPitch,
                                            uint32_t srcSlicePitch,
                                            ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyRegion,
                               hCommandList,
                               dstptr,
                               dstRegion,
                               dstPitch,
                               dstSlicePitch,
                               srcptr,
                               srcRegion,
                               srcPitch,
                               srcSlicePitch,
                               hEvent);

    ze_command_list_append_memory_copy_region_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pdstptr = &dstptr;
    tracerParams.pdstRegion = &dstRegion;
    tracerParams.pdstPitch = &dstPitch;
    tracerParams.pdstSlicePitch = &dstSlicePitch;
    tracerParams.psrcptr = &srcptr;
    tracerParams.psrcRegion = &srcRegion;
    tracerParams.psrcPitch = &srcPitch;
    tracerParams.psrcSlicePitch = &srcSlicePitch;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendMemoryCopyRegionCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendMemoryCopyRegionCb_t, CommandList, pfnAppendMemoryCopyRegionCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyRegion,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pdstptr,
                                   *tracerParams.pdstRegion,
                                   *tracerParams.pdstPitch,
                                   *tracerParams.pdstSlicePitch,
                                   *tracerParams.psrcptr,
                                   *tracerParams.psrcRegion,
                                   *tracerParams.psrcPitch,
                                   *tracerParams.psrcSlicePitch,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopy_Tracing(ze_command_list_handle_t hCommandList,
                                     ze_image_handle_t hDstImage,
                                     ze_image_handle_t hSrcImage,
                                     ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopy,
                               hCommandList,
                               hDstImage,
                               hSrcImage,
                               hEvent);

    ze_command_list_append_image_copy_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phDstImage = &hDstImage;
    tracerParams.phSrcImage = &hSrcImage;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendImageCopyCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendImageCopyCb_t, CommandList, pfnAppendImageCopyCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopy,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phDstImage,
                                   *tracerParams.phSrcImage,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyRegion_Tracing(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pDstRegion,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyRegion,
                               hCommandList,
                               hDstImage,
                               hSrcImage,
                               pDstRegion,
                               pSrcRegion,
                               hEvent);

    ze_command_list_append_image_copy_region_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phDstImage = &hDstImage;
    tracerParams.phSrcImage = &hSrcImage;
    tracerParams.ppDstRegion = &pDstRegion;
    tracerParams.ppSrcRegion = &pSrcRegion;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendImageCopyRegionCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendImageCopyRegionCb_t, CommandList, pfnAppendImageCopyRegionCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyRegion,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phDstImage,
                                   *tracerParams.phSrcImage,
                                   *tracerParams.ppDstRegion,
                                   *tracerParams.ppSrcRegion,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyToMemory_Tracing(ze_command_list_handle_t hCommandList,
                                             void *dstptr,
                                             ze_image_handle_t hSrcImage,
                                             const ze_image_region_t *pSrcRegion,
                                             ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyToMemory,
                               hCommandList,
                               dstptr,
                               hSrcImage,
                               pSrcRegion,
                               hEvent);

    ze_command_list_append_image_copy_to_memory_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pdstptr = &dstptr;
    tracerParams.phSrcImage = &hSrcImage;
    tracerParams.ppSrcRegion = &pSrcRegion;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendImageCopyToMemoryCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendImageCopyToMemoryCb_t, CommandList, pfnAppendImageCopyToMemoryCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyToMemory,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pdstptr,
                                   *tracerParams.phSrcImage,
                                   *tracerParams.ppSrcRegion,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyFromMemory_Tracing(ze_command_list_handle_t hCommandList,
                                               ze_image_handle_t hDstImage,
                                               const void *srcptr,
                                               const ze_image_region_t *pDstRegion,
                                               ze_event_handle_t hEvent) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyFromMemory,
                               hCommandList,
                               hDstImage,
                               srcptr,
                               pDstRegion,
                               hEvent);

    ze_command_list_append_image_copy_from_memory_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phDstImage = &hDstImage;
    tracerParams.psrcptr = &srcptr;
    tracerParams.ppDstRegion = &pDstRegion;
    tracerParams.phEvent = &hEvent;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendImageCopyFromMemoryCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendImageCopyFromMemoryCb_t, CommandList, pfnAppendImageCopyFromMemoryCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyFromMemory,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phDstImage,
                                   *tracerParams.psrcptr,
                                   *tracerParams.ppDstRegion,
                                   *tracerParams.phEvent);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryPrefetch_Tracing(ze_command_list_handle_t hCommandList,
                                          const void *ptr,
                                          size_t size) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryPrefetch,
                               hCommandList,
                               ptr,
                               size);

    ze_command_list_append_memory_prefetch_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendMemoryPrefetchCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendMemoryPrefetchCb_t, CommandList, pfnAppendMemoryPrefetchCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryPrefetch,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.pptr,
                                   *tracerParams.psize);
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemAdvise_Tracing(ze_command_list_handle_t hCommandList,
                                     ze_device_handle_t hDevice,
                                     const void *ptr,
                                     size_t size,
                                     ze_memory_advice_t advice) {

    ZE_HANDLE_TRACER_RECURSION(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemAdvise,
                               hCommandList,
                               hDevice,
                               ptr,
                               size,
                               advice);

    ze_command_list_append_mem_advise_params_t tracerParams;
    tracerParams.phCommandList = &hCommandList;
    tracerParams.phDevice = &hDevice;
    tracerParams.pptr = &ptr;
    tracerParams.psize = &size;
    tracerParams.padvice = &advice;

    L0::APITracerCallbackDataImp<ze_pfnCommandListAppendMemAdviseCb_t> api_callbackData;

    ZE_GEN_PER_API_CALLBACK_STATE(api_callbackData, ze_pfnCommandListAppendMemAdviseCb_t, CommandList, pfnAppendMemAdviseCb);

    return L0::APITracerWrapperImp(driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemAdvise,
                                   &tracerParams,
                                   api_callbackData.apiOrdinal,
                                   api_callbackData.prologCallbacks,
                                   api_callbackData.epilogCallbacks,
                                   *tracerParams.phCommandList,
                                   *tracerParams.phDevice,
                                   *tracerParams.pptr,
                                   *tracerParams.psize,
                                   *tracerParams.padvice);
}