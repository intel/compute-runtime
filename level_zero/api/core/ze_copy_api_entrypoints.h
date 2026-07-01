/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/basic_math.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/experimental/source/graph/graph_captured_apis.h"
#include <level_zero/ze_api.h>

namespace L0 {
ze_result_t ZE_APICALL zeCommandListAppendMemoryCopy(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemoryCopy>(hCommandList, dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendMemoryFill(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemoryFill>(hCommandList, ptr, pattern, patternSize, size, hEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }
    if (false == Math::isPow2(patternSize)) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendMemoryFill(ptr, pattern, patternSize, size, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendMemoryCopyRegion(
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
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemoryCopyRegion>(hCommandList, dstptr, dstRegion, dstPitch, dstSlicePitch, srcptr, srcRegion, srcPitch, srcSlicePitch, hEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendMemoryCopyRegion(dstptr, dstRegion, dstPitch, dstSlicePitch, srcptr, srcRegion, srcPitch, srcSlicePitch, hEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendImageCopy(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendImageCopy>(hCommandList, hDstImage, hSrcImage, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendImageCopy(hDstImage, hSrcImage, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendImageCopyRegion(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pDstRegion,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendImageCopyRegion>(hCommandList, hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendImageCopyToMemory(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendImageCopyToMemory>(hCommandList, dstptr, hSrcImage, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendImageCopyToMemory(dstptr, hSrcImage, pSrcRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendImageCopyFromMemory(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendImageCopyFromMemory>(hCommandList, hDstImage, srcptr, pDstRegion, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendImageCopyFromMemory(hDstImage, srcptr, pDstRegion, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendImageCopyToMemoryExt(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    uint32_t destRowPitch,
    uint32_t destSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendImageCopyToMemoryExt>(hCommandList, dstptr, hSrcImage, pSrcRegion, destRowPitch, destSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendImageCopyToMemoryExt(dstptr, hSrcImage, pSrcRegion, destRowPitch, destSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendImageCopyFromMemoryExt(
    ze_command_list_handle_t hCommandList,
    ze_image_handle_t hDstImage,
    const void *srcptr,
    const ze_image_region_t *pDstRegion,
    uint32_t srcRowPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendImageCopyFromMemoryExt>(hCommandList, hDstImage, srcptr, pDstRegion, srcRowPitch, srcSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    CmdListMemoryCopyParams memoryCopyParams = {};
    return cmdList->appendImageCopyFromMemoryExt(hDstImage, srcptr, pDstRegion, srcRowPitch, srcSlicePitch, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

ze_result_t ZE_APICALL zeCommandListAppendMemoryPrefetch(
    ze_command_list_handle_t hCommandList,
    const void *ptr,
    size_t size) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemoryPrefetch>(hCommandList, ptr, size);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendMemoryPrefetch(ptr, size);
}

ze_result_t ZE_APICALL zeCommandListAppendMemAdvise(
    ze_command_list_handle_t hCommandList,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_advice_t advice) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemAdvise>(hCommandList, hDevice, ptr, size, advice);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendMemAdvise(hDevice, ptr, size, advice);
}

ze_result_t ZE_APICALL zeCommandListAppendMemoryCopyFromContext(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    ze_context_handle_t hContextSrc,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    auto cmdList = L0::CommandList::fromHandle(hCommandList);
    auto ret = cmdList->capture<CaptureApi::zeCommandListAppendMemoryCopyFromContext>(hCommandList, dstptr, hContextSrc, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret != ZE_RESULT_ERROR_NOT_AVAILABLE) {
        return ret;
    }

    return cmdList->appendMemoryCopyFromContext(dstptr, hContextSrc, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, false);
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
