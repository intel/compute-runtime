/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchFunction(
    ze_kernel_handle_t hFunction, const ze_group_count_t *pThreadGroupDimensions,
    ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(hFunction, pThreadGroupDimensions,
                                                                          hEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchFunctionIndirect(
    ze_kernel_handle_t hFunction, const ze_group_count_t *pDispatchArgumentsBuffer,
    ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunctionIndirect(hFunction, pDispatchArgumentsBuffer,
                                                                                  hEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendBarrier(
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent,
                                                                      numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyRegion(
    void *dstPtr,
    const ze_copy_region_t *dstRegion,
    uint32_t dstPitch,
    uint32_t dstSlicePitch,
    const void *srcPtr,
    const ze_copy_region_t *srcRegion,
    uint32_t srcPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, dstRegion, dstPitch, dstSlicePitch,
                                                                            srcPtr, srcRegion, srcPitch, srcSlicePitch,
                                                                            hSignalEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryFill(void *ptr, const void *pattern,
                                                                            size_t patternSize, size_t size,
                                                                            ze_event_handle_t hEvent) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(ptr, pattern, patternSize, size, hEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hEvent) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendEventReset(ze_event_handle_t hEvent) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendEventReset(hEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendPageFaultCopy(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr, size_t size, bool flushHost) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(dstptr, srcptr, size, flushHost);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(false);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numEvents, phEvent);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyFromMemory(
    ze_image_handle_t hDstImage,
    const void *srcPtr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemory(hDstImage, srcPtr, pDstRegion, hEvent,
                                                                               numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyToMemory(
    void *dstPtr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(dstPtr, hSrcImage, pSrcRegion, hEvent,
                                                                             numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}
} // namespace L0
