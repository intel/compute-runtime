/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist_hw.h"

namespace L0 {

struct EventPool;
struct Event;

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamilyImmediate : public CommandListCoreFamily<gfxCoreFamily> {
    using BaseClass = CommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::executeCommandListImmediate;

    using BaseClass::BaseClass;
    CommandListCoreFamilyImmediate() {}
    CommandListCoreFamilyImmediate(uint32_t numIddsPerBlock) : CommandListCoreFamily<gfxCoreFamily>(numIddsPerBlock) {}

    ze_result_t appendLaunchFunction(
        ze_kernel_handle_t hFunction, const ze_group_count_t *pThreadGroupDimensions,
        ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendLaunchFunctionIndirect(
        ze_kernel_handle_t hFunction, const ze_group_count_t *pDispatchArgumentsBuffer,
        ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent,
                              uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryCopy(void *dstptr,
                                 const void *srcptr,
                                 size_t size,
                                 ze_event_handle_t hSignalEvent,
                                 uint32_t numWaitEvents,
                                 ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendMemoryCopyRegion(void *dstPtr,
                                       const ze_copy_region_t *dstRegion,
                                       uint32_t dstPitch,
                                       uint32_t dstSlicePitch,
                                       const void *srcPtr,
                                       const ze_copy_region_t *srcRegion,
                                       uint32_t srcPitch,
                                       uint32_t srcSlicePitch,
                                       ze_event_handle_t hSignalEvent) override;

    ze_result_t appendMemoryFill(void *ptr, const void *pattern,
                                 size_t patternSize, size_t size,
                                 ze_event_handle_t hEvent) override;

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent) override;

    ze_result_t appendEventReset(ze_event_handle_t hEvent) override;

    ze_result_t appendPageFaultCopy(NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr, size_t size, bool flushHost) override;

    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent) override;

    ze_result_t appendImageCopyFromMemory(ze_image_handle_t hDstImage,
                                          const void *srcPtr,
                                          const ze_image_region_t *pDstRegion,
                                          ze_event_handle_t hEvent,
                                          uint32_t numWaitEvents,
                                          ze_event_handle_t *phWaitEvents) override;

    ze_result_t appendImageCopyToMemory(void *dstPtr,
                                        ze_image_handle_t hSrcImage,
                                        const ze_image_region_t *pSrcRegion,
                                        ze_event_handle_t hEvent,
                                        uint32_t numWaitEvents,
                                        ze_event_handle_t *phWaitEvents) override;
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListImmediateProductFamily;

} // namespace L0
