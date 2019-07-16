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
                                                                     GraphicsAllocation *memObjAllocation, void *hostPtr, bool blocking,
                                                                     size_t offset, uint64_t copySize) {

    HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize), true);
    bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
    UNRECOVERABLE_IF(!success);
    auto hostPtrAllocation = hostPtrSurface.getAllocation();

    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
        return {nullptr, blitDirection, {}, AuxTranslationDirection::None, memObjAllocation, hostPtrAllocation, hostPtr, blocking, offset, 0, copySize};
    } else {
        return {nullptr, blitDirection, {}, AuxTranslationDirection::None, hostPtrAllocation, memObjAllocation, hostPtr, blocking, 0, offset, copySize};
    }
}

BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                     CommandStreamReceiver &commandStreamReceiver,
                                                                     const BuiltinOpParams &builtinOpParams,
                                                                     bool blocking) {
    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
        return constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, builtinOpParams.dstMemObj->getGraphicsAllocation(),
                                                     builtinOpParams.srcPtr, blocking, builtinOpParams.dstOffset.x,
                                                     builtinOpParams.size.x);
    } else {
        return constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, builtinOpParams.srcMemObj->getGraphicsAllocation(),
                                                     builtinOpParams.dstPtr, blocking, builtinOpParams.srcOffset.x,
                                                     builtinOpParams.size.x);
    }
}

BlitProperties BlitProperties::constructPropertiesForCopyBuffer(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                                bool blocking, size_t dstOffset, size_t srcOffset, uint64_t copySize) {

    return {nullptr, BlitterConstants::BlitDirection::BufferToBuffer, {}, AuxTranslationDirection::None, dstAllocation, srcAllocation, nullptr, blocking, dstOffset, srcOffset, copySize};
}

BlitProperties BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                                    GraphicsAllocation *allocation) {
    auto allocationSize = allocation->getUnderlyingBufferSize();
    return {nullptr, BlitterConstants::BlitDirection::BufferToBuffer, {}, auxTranslationDirection, allocation, allocation, nullptr, false, 0, 0, allocationSize};
}

BlitterConstants::BlitDirection BlitProperties::obtainBlitDirection(uint32_t commandType) {
    return (CL_COMMAND_WRITE_BUFFER == commandType) ? BlitterConstants::BlitDirection::HostPtrToBuffer
                                                    : BlitterConstants::BlitDirection::BufferToHostPtr;
}

} // namespace NEO
