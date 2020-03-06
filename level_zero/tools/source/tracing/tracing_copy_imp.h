/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryCopy_Tracing(ze_command_list_handle_t hCommandList,
                                      void *dstptr,
                                      const void *srcptr,
                                      size_t size,
                                      ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryFill_Tracing(ze_command_list_handle_t hCommandList,
                                      void *ptr,
                                      const void *pattern,
                                      size_t patternSize,
                                      size_t size,
                                      ze_event_handle_t hEvent);

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
                                            ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopy_Tracing(ze_command_list_handle_t hCommandList,
                                     ze_image_handle_t hDstImage,
                                     ze_image_handle_t hSrcImage,
                                     ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyRegion_Tracing(ze_command_list_handle_t hCommandList,
                                           ze_image_handle_t hDstImage,
                                           ze_image_handle_t hSrcImage,
                                           const ze_image_region_t *pDstRegion,
                                           const ze_image_region_t *pSrcRegion,
                                           ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyToMemory_Tracing(ze_command_list_handle_t hCommandList,
                                             void *dstptr,
                                             ze_image_handle_t hSrcImage,
                                             const ze_image_region_t *pSrcRegion,
                                             ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendImageCopyFromMemory_Tracing(ze_command_list_handle_t hCommandList,
                                               ze_image_handle_t hDstImage,
                                               const void *srcptr,
                                               const ze_image_region_t *pDstRegion,
                                               ze_event_handle_t hEvent);

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryPrefetch_Tracing(ze_command_list_handle_t hCommandList,
                                          const void *ptr,
                                          size_t size);

__zedllexport ze_result_t __zecall
zeCommandListAppendMemAdvise_Tracing(ze_command_list_handle_t hCommandList,
                                     ze_device_handle_t hDevice,
                                     const void *ptr,
                                     size_t size,
                                     ze_memory_advice_t advice);

} // extern "C"
