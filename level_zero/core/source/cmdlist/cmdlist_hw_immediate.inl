/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernel(
    ze_kernel_handle_t hKernel, const ze_group_count_t *pThreadGroupDimensions,
    ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(hKernel, pThreadGroupDimensions,
                                                                        hEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernelIndirect(
    ze_kernel_handle_t hKernel, const ze_group_count_t *pDispatchArgumentsBuffer,
    ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelIndirect(hKernel, pDispatchArgumentsBuffer,
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
    bool isTimestampEvent = false;
    for (uint32_t i = 0; i < numWaitEvents; i++) {
        auto event = Event::fromHandle(phWaitEvents[i]);
        isTimestampEvent |= (event->isTimestampEvent) ? true : false;
    }
    if (hSignalEvent) {
        auto signalEvent = Event::fromHandle(hSignalEvent);
        isTimestampEvent |= signalEvent->isTimestampEvent;
    }
    if (isSyncModeQueue || isTimestampEvent) {
        auto ret = CommandListCoreFamily<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);
        if (ret == ZE_RESULT_SUCCESS) {
            executeCommandListImmediate(true);
        }
        return ret;
    } else {
        auto ret = appendWaitOnEvents(numWaitEvents, phWaitEvents);
        if (!hSignalEvent) {
            NEO::PipeControlArgs args;
            auto cmdQueueImp = static_cast<CommandQueueImp *>(this->cmdQImmediate);
            NEO::CommandStreamReceiver *csr = cmdQueueImp->getCsr();
            csr->flushNonKernelTask(nullptr, 0, 0, args, false, false, false);
        } else {
            ret = appendSignalEvent(hSignalEvent);
        }
        return ret;
    }
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
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, dstRegion, dstPitch, dstSlicePitch,
                                                                            srcPtr, srcRegion, srcPitch, srcSlicePitch,
                                                                            hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryFill(void *ptr, const void *pattern,
                                                                            size_t patternSize, size_t size,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(ptr, pattern, patternSize, size, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hEvent) {
    auto event = Event::fromHandle(hEvent);
    if (isSyncModeQueue || event->isTimestampEvent) {
        auto ret = CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hEvent);
        if (ret == ZE_RESULT_SUCCESS) {
            executeCommandListImmediate(true);
        }
        return ret;
    } else {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = (!event->signalScope) ? false : true;
        auto cmdQueueImp = static_cast<CommandQueueImp *>(this->cmdQImmediate);
        NEO::CommandStreamReceiver *csr = cmdQueueImp->getCsr();
        csr->flushNonKernelTask(&event->getAllocation(this->device), event->getGpuAddress(this->device), Event::STATE_SIGNALED, args, false, false, false);
        event->updateTaskCountEnabled = true;
        return ZE_RESULT_SUCCESS;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendEventReset(ze_event_handle_t hEvent) {
    auto event = Event::fromHandle(hEvent);
    if (isSyncModeQueue || event->isTimestampEvent) {
        auto ret = CommandListCoreFamily<gfxCoreFamily>::appendEventReset(hEvent);
        if (ret == ZE_RESULT_SUCCESS) {
            executeCommandListImmediate(true);
        }
        return ret;
    } else {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = (!event->signalScope) ? false : true;
        auto cmdQueueImp = static_cast<CommandQueueImp *>(this->cmdQImmediate);
        NEO::CommandStreamReceiver *csr = cmdQueueImp->getCsr();
        csr->flushNonKernelTask(&event->getAllocation(this->device), event->getGpuAddress(this->device), Event::STATE_CLEARED, args, false, false, false);
        event->updateTaskCountEnabled = true;
        return ZE_RESULT_SUCCESS;
    }
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
    bool isTimestampEvent = false;
    for (uint32_t i = 0; i < numEvents; i++) {
        auto event = Event::fromHandle(phEvent[i]);
        isTimestampEvent |= (event->isTimestampEvent) ? true : false;
    }
    if (isSyncModeQueue || isTimestampEvent) {
        auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numEvents, phEvent);
        if (ret == ZE_RESULT_SUCCESS) {
            executeCommandListImmediate(true);
        }
        return ret;
    } else {
        bool dcFlushRequired = false;
        for (uint32_t i = 0; i < numEvents; i++) {
            auto event = Event::fromHandle(phEvent[i]);
            dcFlushRequired |= (!event->waitScope) ? false : true;
        }

        auto cmdQueueImp = static_cast<CommandQueueImp *>(this->cmdQImmediate);
        NEO::CommandStreamReceiver *csr = cmdQueueImp->getCsr();
        NEO::PipeControlArgs args;
        args.dcFlushEnable = dcFlushRequired;
        for (uint32_t i = 0; i < numEvents; i++) {
            auto event = Event::fromHandle(phEvent[i]);
            bool isStartOfDispatch = (i == 0);
            bool isEndOfDispatch = (i == numEvents - 1);
            csr->flushNonKernelTask(&event->getAllocation(this->device), event->getGpuAddress(this->device), Event::STATE_CLEARED, args, true, isStartOfDispatch, isEndOfDispatch);
            event->updateTaskCountEnabled = true;
        }
        return ZE_RESULT_SUCCESS;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWriteGlobalTimestamp(
    uint64_t *dstptr, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        executeCommandListImmediate(true);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyFromContext(
    void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
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
