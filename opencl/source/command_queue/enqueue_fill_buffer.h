/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "opencl/source/built_ins/built_ins.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

#include <new>

namespace NEO {

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueFillBuffer(
    Buffer *buffer,
    const void *pattern,
    size_t patternSize,
    size_t offset,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto memoryManager = getDevice().getMemoryManager();
    DEBUG_BREAK_IF(nullptr == memoryManager);

    auto patternAllocation = memoryManager->allocateGraphicsMemoryWithProperties({getDevice().getRootDeviceIndex(), alignUp(patternSize, MemoryConstants::cacheLineSize), GraphicsAllocation::AllocationType::FILL_PATTERN});

    if (patternSize == 1) {
        int patternInt = (uint32_t)((*(uint8_t *)pattern << 24) | (*(uint8_t *)pattern << 16) | (*(uint8_t *)pattern << 8) | *(uint8_t *)pattern);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(int), &patternInt, sizeof(int));
    } else if (patternSize == 2) {
        int patternInt = (uint32_t)((*(uint16_t *)pattern << 16) | *(uint16_t *)pattern);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(int), &patternInt, sizeof(int));
    } else {
        memcpy_s(patternAllocation->getUnderlyingBuffer(), patternSize, pattern, patternSize);
    }

    auto eBuiltInOps = EBuiltInOps::FillBuffer;
    if (forceStateless(buffer->getSize())) {
        eBuiltInOps = EBuiltInOps::FillBufferStateless;
    }

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(eBuiltInOps,
                                                                            this->getDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    BuiltinOpParams dc;
    MemObj patternMemObj(this->context, 0, {}, 0, 0, alignUp(patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), patternAllocation, false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};

    MultiDispatchInfo dispatchInfo;
    builder.buildDispatchInfos(dispatchInfo, dc);

    MemObjSurface s1(buffer);
    GeneralSurface s2(patternAllocation);
    Surface *surfaces[] = {&s1, &s2};

    enqueueHandler<CL_COMMAND_FILL_BUFFER>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    auto storageForAllocation = getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    storageForAllocation->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(patternAllocation), TEMPORARY_ALLOCATION, taskCount);

    return CL_SUCCESS;
}
} // namespace NEO
