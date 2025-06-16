/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/mem_obj_surface.h"

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

    auto commandStreamReceieverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
    auto storageWithAllocations = getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    auto allocationType = AllocationType::fillPattern;
    auto patternAllocation = storageWithAllocations->obtainReusableAllocation(patternSize, allocationType).release();
    commandStreamReceieverOwnership.unlock();

    if (!patternAllocation) {
        patternAllocation = memoryManager->allocateGraphicsMemoryWithProperties({getDevice().getRootDeviceIndex(), alignUp(patternSize, MemoryConstants::cacheLineSize), AllocationType::fillPattern, getDevice().getDeviceBitfield()});
    }

    if (patternSize == 1) {
        auto patternValue = *reinterpret_cast<const uint8_t *>(pattern);
        int patternInt = static_cast<uint32_t>((patternValue << 24) | (patternValue << 16) | (patternValue << 8) | patternValue);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(uint32_t), &patternInt, sizeof(uint32_t));
    } else if (patternSize == 2) {
        auto patternValue = *reinterpret_cast<const uint16_t *>(pattern);
        int patternInt = static_cast<uint32_t>((patternValue << 16) | patternValue);
        memcpy_s(patternAllocation->getUnderlyingBuffer(), sizeof(uint32_t), &patternInt, sizeof(uint32_t));
    } else {
        memcpy_s(patternAllocation->getUnderlyingBuffer(), patternSize, pattern, patternSize);
    }

    const bool useStateless = forceStateless(buffer->getSize());
    const bool useHeapless = this->getHeaplessModeEnabled();
    auto builtInType = EBuiltInOps::adjustBuiltinType<EBuiltInOps::fillBuffer>(useStateless, useHeapless);

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInType,
                                                                            this->getClDevice());

    BuiltInOwnershipWrapper builtInLock(builder, this->context);

    BuiltinOpParams dc;
    auto multiGraphicsAllocation = MultiGraphicsAllocation(getDevice().getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(patternAllocation);

    MemObj patternMemObj(this->context, 0, {}, 0, 0, alignUp(patternSize, 4), patternAllocation->getUnderlyingBuffer(),
                         patternAllocation->getUnderlyingBuffer(), std::move(multiGraphicsAllocation), false, false, true);
    dc.srcMemObj = &patternMemObj;
    dc.dstMemObj = buffer;
    dc.dstOffset = {offset, 0, 0};
    dc.size = {size, 0, 0};

    MultiDispatchInfo dispatchInfo(dc);
    builder.buildDispatchInfos(dispatchInfo);

    MemObjSurface s1(buffer);
    GeneralSurface s2(patternAllocation);
    Surface *surfaces[] = {&s1, &s2};

    const auto enqueueResult = enqueueHandler<CL_COMMAND_FILL_BUFFER>(
        surfaces,
        false,
        dispatchInfo,
        numEventsInWaitList,
        eventWaitList,
        event);

    auto storageForAllocation = getGpgpuCommandStreamReceiver().getInternalAllocationStorage();
    storageForAllocation->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(patternAllocation), REUSABLE_ALLOCATION, taskCount);

    return enqueueResult;
}
} // namespace NEO
