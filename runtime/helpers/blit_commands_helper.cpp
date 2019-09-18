/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper.h"

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/context/context.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/memory_manager/surface.h"

#include "CL/cl.h"

namespace NEO {
BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                     CommandStreamReceiver &commandStreamReceiver,
                                                                     GraphicsAllocation *memObjAllocation, size_t memObjOffset,
                                                                     GraphicsAllocation *mapAllocation,
                                                                     void *hostPtr, size_t hostPtrOffset,
                                                                     bool blocking, size_t copyOffset, uint64_t copySize) {

    GraphicsAllocation *hostAllocation = nullptr;

    if (mapAllocation) {
        hostAllocation = mapAllocation;
        hostPtrOffset += ptrDiff(hostPtr, mapAllocation->getGpuAddress());
    } else {
        HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize), true);
        bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
        UNRECOVERABLE_IF(!success);
        hostAllocation = hostPtrSurface.getAllocation();
    }

    auto offset = copyOffset + memObjOffset;
    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
        return {nullptr, blitDirection, {}, AuxTranslationDirection::None, memObjAllocation, hostAllocation, blocking, offset, hostPtrOffset, copySize};
    } else {
        return {nullptr, blitDirection, {}, AuxTranslationDirection::None, hostAllocation, memObjAllocation, blocking, hostPtrOffset, offset, copySize};
    }
}

BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                     CommandStreamReceiver &commandStreamReceiver,
                                                                     const BuiltinOpParams &builtinOpParams,
                                                                     bool blocking) {
    auto mapAllocation = builtinOpParams.mapAllocation;
    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
        return constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, builtinOpParams.dstMemObj->getGraphicsAllocation(),
                                                     builtinOpParams.dstMemObj->getOffset(), mapAllocation, builtinOpParams.srcPtr,
                                                     builtinOpParams.srcOffset.x, blocking, builtinOpParams.dstOffset.x,
                                                     builtinOpParams.size.x);
    } else {
        return constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, builtinOpParams.srcMemObj->getGraphicsAllocation(),
                                                     builtinOpParams.srcMemObj->getOffset(), mapAllocation, builtinOpParams.dstPtr,
                                                     builtinOpParams.dstOffset.x, blocking, builtinOpParams.srcOffset.x,
                                                     builtinOpParams.size.x);
    }
}

BlitProperties BlitProperties::constructPropertiesForCopyBuffer(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                                bool blocking, size_t dstOffset, size_t srcOffset, uint64_t copySize) {

    return {nullptr, BlitterConstants::BlitDirection::BufferToBuffer, {}, AuxTranslationDirection::None, dstAllocation, srcAllocation, blocking, dstOffset, srcOffset, copySize};
}

BlitProperties BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                                    GraphicsAllocation *allocation) {
    auto allocationSize = allocation->getUnderlyingBufferSize();
    return {nullptr, BlitterConstants::BlitDirection::BufferToBuffer, {}, auxTranslationDirection, allocation, allocation, false, 0, 0, allocationSize};
}

BlitterConstants::BlitDirection BlitProperties::obtainBlitDirection(uint32_t commandType) {
    return (CL_COMMAND_WRITE_BUFFER == commandType) ? BlitterConstants::BlitDirection::HostPtrToBuffer
                                                    : BlitterConstants::BlitDirection::BufferToHostPtr;
}

} // namespace NEO
