/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper.h"

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/surface.h"

namespace NEO {
BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection blitDirection,
                                                                     CommandStreamReceiver &commandStreamReceiver,
                                                                     GraphicsAllocation *memObjAllocation,
                                                                     GraphicsAllocation *preallocatedHostAllocation,
                                                                     void *hostPtr, uint64_t memObjGpuVa,
                                                                     uint64_t hostAllocGpuVa, Vec3<size_t> hostPtrOffset,
                                                                     Vec3<size_t> copyOffset, Vec3<size_t> copySize,
                                                                     size_t hostRowPitch, size_t hostSlicePitch,
                                                                     size_t gpuRowPitch, size_t gpuSlicePitch) {
    GraphicsAllocation *hostAllocation = nullptr;

    if (preallocatedHostAllocation) {
        hostAllocation = preallocatedHostAllocation;
        UNRECOVERABLE_IF(hostAllocGpuVa == 0);
    } else {
        HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize.x), true);
        bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
        UNRECOVERABLE_IF(!success);
        hostAllocation = hostPtrSurface.getAllocation();
        hostAllocGpuVa = hostAllocation->getGpuAddress();
    }

    copySize.y = copySize.y ? copySize.y : 1;
    copySize.z = copySize.z ? copySize.z : 1;

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
            hostPtrOffset,                 // srcOffset
            gpuRowPitch,                   //dstRowPitch
            gpuSlicePitch,                 //dstSlicePitch
            hostRowPitch,                  //srcRowPitch
            hostSlicePitch};               //srcSlicePitch

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
            copyOffset,                    // srcOffset
            hostRowPitch,                  // dstRowPitch
            hostSlicePitch,                // dstSlicePitch
            gpuRowPitch,                   // srcRowPitch
            gpuSlicePitch};                // srcSlicePitch
    };
}

BlitProperties BlitProperties::constructPropertiesForCopyBuffer(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                                size_t dstOffset, size_t srcOffset, size_t copySize) {

    return {
        nullptr,                                         // outputTimestampPacket
        BlitterConstants::BlitDirection::BufferToBuffer, // blitDirection
        {},                                              // csrDependencies
        AuxTranslationDirection::None,                   // auxTranslationDirection
        dstAllocation,                                   // dstAllocation
        srcAllocation,                                   // srcAllocation
        dstAllocation->getGpuAddress(),                  // dstGpuAddress
        srcAllocation->getGpuAddress(),                  // srcGpuAddress
        {copySize, 1, 1},                                // copySize
        {dstOffset, 0, 0},                               // dstOffset
        {srcOffset, 0, 0}};                              // srcOffset
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
        {allocationSize, 1, 1},                          // copySize
        0,                                               // dstOffset
        0                                                // srcOffset
    };
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
    blitPropertiesContainer[numObjects].csrDependencies.push_back(&timestampPacketDependencies.cacheFlushNodes);
    blitPropertiesContainer[numObjects].csrDependencies.push_back(&kernelTimestamps);
}

} // namespace NEO
