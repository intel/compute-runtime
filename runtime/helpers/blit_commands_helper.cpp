/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper.h"

#include "core/helpers/timestamp_packet.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/context/context.h"
#include "runtime/memory_manager/surface.h"

#include "CL/cl.h"

namespace NEO {
BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                     CommandStreamReceiver &commandStreamReceiver,
                                                                     GraphicsAllocation *memObjAllocation,
                                                                     GraphicsAllocation *preallocatedHostAllocation,
                                                                     void *hostPtr, uint64_t memObjGpuVa,
                                                                     uint64_t hostAllocGpuVa, size_t hostPtrOffset,
                                                                     size_t copyOffset, uint64_t copySize) {

    GraphicsAllocation *hostAllocation = nullptr;

    if (preallocatedHostAllocation) {
        hostAllocation = preallocatedHostAllocation;
        UNRECOVERABLE_IF(hostAllocGpuVa == 0);
    } else {
        HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize), true);
        bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
        UNRECOVERABLE_IF(!success);
        hostAllocation = hostPtrSurface.getAllocation();
        hostAllocGpuVa = hostAllocation->getGpuAddress();
    }

    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
        return {
            nullptr,                       // outputTimestampPacket
            blitDirection,                 // blitDirection
            {},                            // csrDependencies
            AuxTranslationDirection::None, // auxTranslationDirection
            memObjAllocation,              // dstAllocation
            hostAllocation,                // srcAllocation
            memObjGpuVa,                   // dstGpuAddress
            hostAllocGpuVa,                // srcGpuAddress
            copySize,                      // copySize
            copyOffset,                    // dstOffset
            hostPtrOffset};                // srcOffset
    } else {
        return {
            nullptr,                       // outputTimestampPacket
            blitDirection,                 // blitDirection
            {},                            // csrDependencies
            AuxTranslationDirection::None, // auxTranslationDirection
            hostAllocation,                // dstAllocation
            memObjAllocation,              // srcAllocation
            hostAllocGpuVa,                // dstGpuAddress
            memObjGpuVa,                   // srcGpuAddress
            copySize,                      // copySize
            hostPtrOffset,                 // dstOffset
            copyOffset};                   // srcOffset
    }
}

BlitProperties BlitProperties::constructProperties(BlitterConstants::BlitDirection blitDirection,
                                                   CommandStreamReceiver &commandStreamReceiver,
                                                   const BuiltinOpParams &builtinOpParams) {

    if (BlitterConstants::BlitDirection::BufferToBuffer == blitDirection) {
        auto dstOffset = builtinOpParams.dstOffset.x + builtinOpParams.dstMemObj->getOffset();
        auto srcOffset = builtinOpParams.srcOffset.x + builtinOpParams.srcMemObj->getOffset();

        return constructPropertiesForCopyBuffer(builtinOpParams.dstMemObj->getGraphicsAllocation(),
                                                builtinOpParams.srcMemObj->getGraphicsAllocation(),
                                                dstOffset, srcOffset, builtinOpParams.size.x);
    }

    GraphicsAllocation *gpuAllocation = nullptr;
    size_t copyOffset = 0;

    void *hostPtr = nullptr;
    size_t hostPtrOffset = 0;

    uint64_t memObjGpuVa = 0;
    uint64_t hostAllocGpuVa = 0;

    GraphicsAllocation *hostAllocation = builtinOpParams.transferAllocation;

    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
        // write buffer
        hostPtr = builtinOpParams.srcPtr;
        hostPtrOffset = builtinOpParams.srcOffset.x;
        copyOffset = builtinOpParams.dstOffset.x;

        memObjGpuVa = castToUint64(builtinOpParams.dstPtr);
        hostAllocGpuVa = castToUint64(builtinOpParams.srcPtr);

        if (builtinOpParams.dstSvmAlloc) {
            gpuAllocation = builtinOpParams.dstSvmAlloc;
            hostAllocation = builtinOpParams.srcSvmAlloc;
        } else {
            gpuAllocation = builtinOpParams.dstMemObj->getGraphicsAllocation();
            memObjGpuVa = (gpuAllocation->getGpuAddress() + builtinOpParams.dstMemObj->getOffset());
        }
    }

    if (BlitterConstants::BlitDirection::BufferToHostPtr == blitDirection) {
        // read buffer
        hostPtr = builtinOpParams.dstPtr;

        hostPtrOffset = builtinOpParams.dstOffset.x;
        copyOffset = builtinOpParams.srcOffset.x;

        memObjGpuVa = castToUint64(builtinOpParams.srcPtr);
        hostAllocGpuVa = castToUint64(builtinOpParams.dstPtr);

        if (builtinOpParams.srcSvmAlloc) {
            gpuAllocation = builtinOpParams.srcSvmAlloc;
            hostAllocation = builtinOpParams.dstSvmAlloc;
        } else {
            gpuAllocation = builtinOpParams.srcMemObj->getGraphicsAllocation();
            memObjGpuVa = (gpuAllocation->getGpuAddress() + builtinOpParams.srcMemObj->getOffset());
        }
    }

    UNRECOVERABLE_IF(BlitterConstants::BlitDirection::HostPtrToBuffer != blitDirection &&
                     BlitterConstants::BlitDirection::BufferToHostPtr != blitDirection);

    return constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, gpuAllocation,
                                                 hostAllocation, hostPtr, memObjGpuVa, hostAllocGpuVa,
                                                 hostPtrOffset, copyOffset, builtinOpParams.size.x);
}

BlitProperties BlitProperties::constructPropertiesForCopyBuffer(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                                size_t dstOffset, size_t srcOffset, uint64_t copySize) {

    return {
        nullptr,                                         // outputTimestampPacket
        BlitterConstants::BlitDirection::BufferToBuffer, // blitDirection
        {},                                              // csrDependencies
        AuxTranslationDirection::None,                   // auxTranslationDirection
        dstAllocation,                                   // dstAllocation
        srcAllocation,                                   // srcAllocation
        dstAllocation->getGpuAddress(),                  // dstGpuAddress
        srcAllocation->getGpuAddress(),                  // srcGpuAddress
        copySize,                                        // copySize
        dstOffset,                                       // dstOffset
        srcOffset};                                      // srcOffset
}

BlitProperties BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                                    GraphicsAllocation *allocation) {

    auto allocationSize = allocation->getUnderlyingBufferSize();
    return {
        nullptr,                                         // outputTimestampPacket
        BlitterConstants::BlitDirection::BufferToBuffer, // blitDirection
        {},                                              // csrDependencies
        auxTranslationDirection,                         // auxTranslationDirection
        allocation,                                      // dstAllocation
        allocation,                                      // srcAllocation
        allocation->getGpuAddress(),                     // dstGpuAddress
        allocation->getGpuAddress(),                     // srcGpuAddress
        allocationSize,                                  // copySize
        0,                                               // dstOffset
        0                                                // srcOffset
    };
}

BlitterConstants::BlitDirection BlitProperties::obtainBlitDirection(uint32_t commandType) {
    if (CL_COMMAND_WRITE_BUFFER == commandType) {
        return BlitterConstants::BlitDirection::HostPtrToBuffer;
    } else if (CL_COMMAND_READ_BUFFER == commandType) {
        return BlitterConstants::BlitDirection::BufferToHostPtr;
    } else {
        UNRECOVERABLE_IF(CL_COMMAND_COPY_BUFFER != commandType);
        return BlitterConstants::BlitDirection::BufferToBuffer;
    }
}

void BlitProperties::setupDependenciesForAuxTranslation(BlitPropertiesContainer &blitPropertiesContainer, TimestampPacketDependencies &timestampPacketDependencies,
                                                        TimestampPacketContainer &kernelTimestamps, const CsrDependencies &depsFromEvents,
                                                        CommandStreamReceiver &gpguCsr, CommandStreamReceiver &bcsCsr) {
    auto numObjects = blitPropertiesContainer.size() / 2;

    for (size_t i = 0; i < numObjects; i++) {
        blitPropertiesContainer[i].outputTimestampPacket = timestampPacketDependencies.auxToNonAuxNodes.peekNodes()[i];
        blitPropertiesContainer[i + numObjects].outputTimestampPacket = timestampPacketDependencies.nonAuxToAuxNodes.peekNodes()[i];
    }

    gpguCsr.requestStallingPipeControlOnNextFlush();
    auto nodesAllocator = gpguCsr.getTimestampPacketAllocator();
    timestampPacketDependencies.barrierNodes.add(nodesAllocator->getTag());

    // wait for barrier and events before AuxToNonAux
    blitPropertiesContainer[0].csrDependencies.push_back(&timestampPacketDependencies.barrierNodes);

    for (auto dep : depsFromEvents) {
        blitPropertiesContainer[0].csrDependencies.push_back(dep);
    }

    // wait for NDR before NonAuxToAux
    blitPropertiesContainer[numObjects].csrDependencies.push_back(&kernelTimestamps);
}

} // namespace NEO
