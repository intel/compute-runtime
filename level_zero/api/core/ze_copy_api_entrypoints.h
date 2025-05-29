/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t zeCommandListAppendMemoryCopy(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendMemoryFill(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendMemoryFill(ptr, pattern, patternSize, size, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendMemoryCopyRegion(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const ze_copy_region_t *dstRegion,
    uint32_t dstPitch,
    uint32_t dstSlicePitch,
    const void *srcptr,
    const ze_copy_region_t *srcRegion,
    uint32_t srcPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendMemoryCopyRegion(dstptr, dstRegion, dstPitch, dstSlicePitch, srcptr, srcRegion, srcPitch, srcSlicePitch, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendImageCopy(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};

    return L0::CommandList::fromHandle(hCommandList)->appendImageCopy(hDstImage, hSrcImage, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendImageCopyRegion(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pDstRegion,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendImageCopyToMemory(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendImageCopyToMemory(dstptr, hSrcImage, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendImageCopyFromMemory(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendImageCopyFromMemory(hDstImage, srcptr, pDstRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendImageCopyToMemoryExt(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    uint32_t destRowPitch,
    uint32_t destSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendImageCopyToMemoryExt(dstptr, hSrcImage, pSrcRegion, destRowPitch, destSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendImageCopyFromMemoryExt(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    uint32_t srcRowPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    return L0::CommandList::fromHandle(hCommandList)->appendImageCopyFromMemoryExt(hDstImage, srcptr, pDstRegion, srcRowPitch, srcSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t zeCommandListAppendMemoryPrefetch(
    ze_command_list_handle_t hCommandList,
    const void *ptr,
    size_t size) {
    return L0::CommandList::fromHandle(hCommandList)->appendMemoryPrefetch(ptr, size);
}

ze_result_t zeCommandListAppendMemAdvise(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_advice_t advice) {
    return L0::CommandList::fromHandle(hCommandList)->appendMemAdvise(hDevice, ptr, size, advice);
}

ze_result_t zeCommandListAppendMemoryCopyFromContext(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_context_handle_t hContextSrc,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::CommandList::fromHandle(hCommandList)->appendMemoryCopyFromContext(dstptr, hContextSrc, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, false);
}

} // namespace L0

extern "C" {
ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryCopy(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendMemoryCopy(
        hCommandList,
        dstptr,
        srcptr,
        size,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryFill(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendMemoryFill(
        hCommandList,
        ptr,
        pattern,
        patternSize,
        size,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryCopyRegion(
    ze_command_list_handle_t hCommandList,
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
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendMemoryCopyRegion(
        hCommandList,
        dstptr,
        dstRegion,
        dstPitch,
        dstSlicePitch,
        srcptr,
        srcRegion,
        srcPitch,
        srcSlicePitch,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryCopyFromContext(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_context_handle_t hContextSrc,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendMemoryCopyFromContext(
        hCommandList,
        dstptr,
        hContextSrc,
        srcptr,
        size,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendImageCopy(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendImageCopy(
        hCommandList,
        hDstImage,
        hSrcImage,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendImageCopyRegion(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pDstRegion,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendImageCopyRegion(
        hCommandList,
        hDstImage,
        hSrcImage,
        pDstRegion,
        pSrcRegion,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendImageCopyToMemory(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendImageCopyToMemory(
        hCommandList,
        dstptr,
        hSrcImage,
        pSrcRegion,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendImageCopyFromMemory(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendImageCopyFromMemory(
        hCommandList,
        hDstImage,
        srcptr,
        pDstRegion,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendImageCopyToMemoryExt(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    uint32_t destRowPitch,
    uint32_t destSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendImageCopyToMemoryExt(
        hCommandList,
        dstptr,
        hSrcImage,
        pSrcRegion,
        destRowPitch,
        destSlicePitch,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendImageCopyFromMemoryExt(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    uint32_t srcRowPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendImageCopyFromMemoryExt(
        hCommandList,
        hDstImage,
        srcptr,
        pDstRegion,
        srcRowPitch,
        srcSlicePitch,
        hSignalEvent,
        numWaitEvents,
        phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemoryPrefetch(
    ze_command_list_handle_t hCommandList,
    const void *ptr,
    size_t size) {
    return L0::zeCommandListAppendMemoryPrefetch(
        hCommandList,
        ptr,
        size);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListAppendMemAdvise(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_advice_t advice) {
    return L0::zeCommandListAppendMemAdvise(
        hCommandList,
        hDevice,
        ptr,
        size,
        advice);
}
}
