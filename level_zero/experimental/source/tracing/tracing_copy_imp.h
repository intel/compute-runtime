/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryCopyTracing(ze_command_list_handle_t hCommandList,
                                     void *dstptr,
                                     const void *srcptr,
                                     size_t size,
                                     ze_event_handle_t hSignalEvent,
                                     uint32_t numWaitEvents,
                                     ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryFillTracing(ze_command_list_handle_t hCommandList,
                                     void *ptr,
                                     const void *pattern,
                                     size_t patternSize,
                                     size_t size,
                                     ze_event_handle_t hSignalEvent,
                                     uint32_t numWaitEvents,
                                     ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryCopyRegionTracing(ze_command_list_handle_t hCommandList,
                                           void *dstptr,
                                           const ze_copy_region_t *dstRegion,
                                           uint32_t dstPitch,
                                           uint32_t dstSlicePitch,
                                           const void *srcptr,
                                           const ze_copy_region_t *srcRegion,
                                           uint32_t srcPitch,
                                           uint32_t srcSlicePitch,
                                           ze_event_handle_t hSignalEvent,
                                           uint32_t numWaitEvents,
                                           ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryCopyFromContextTracing(ze_command_list_handle_t hCommandList,
                                                void *dstptr,
                                                ze_context_handle_t hContextSrc,
                                                const void *srcptr,
                                                size_t size,
                                                ze_event_handle_t hSignalEvent,
                                                uint32_t numWaitEvents,
                                                ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendImageCopyTracing(ze_command_list_handle_t hCommandList,
                                    ze_image_handle_t hDstImage,
                                    ze_image_handle_t hSrcImage,
                                    ze_event_handle_t hSignalEvent,
                                    uint32_t numWaitEvents,
                                    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendImageCopyRegionTracing(ze_command_list_handle_t hCommandList,
                                          ze_image_handle_t hDstImage,
                                          ze_image_handle_t hSrcImage,
                                          const ze_image_region_t *pDstRegion,
                                          const ze_image_region_t *pSrcRegion,
                                          ze_event_handle_t hSignalEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendImageCopyToMemoryTracing(ze_command_list_handle_t hCommandList,
                                            void *dstptr,
                                            ze_image_handle_t hSrcImage,
                                            const ze_image_region_t *pSrcRegion,
                                            ze_event_handle_t hSignalEvent,
                                            uint32_t numWaitEvents,
                                            ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendImageCopyFromMemoryTracing(ze_command_list_handle_t hCommandList,
                                              ze_image_handle_t hDstImage,
                                              const void *srcptr,
                                              const ze_image_region_t *pDstRegion,
                                              ze_event_handle_t hSignalEvent,
                                              uint32_t numWaitEvents,
                                              ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryPrefetchTracing(ze_command_list_handle_t hCommandList,
                                         const void *ptr,
                                         size_t size);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemAdviseTracing(ze_command_list_handle_t hCommandList,
                                    ze_device_handle_t hDevice,
                                    const void *ptr,
                                    size_t size,
                                    ze_memory_advice_t advice);

} // extern "C"
