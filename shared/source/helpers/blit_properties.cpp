/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_properties.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/surface.h"

namespace NEO {

BlitProperties BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection blitDirection,
                                                               CommandStreamReceiver &commandStreamReceiver,
                                                               GraphicsAllocation *memObjAllocation,
                                                               GraphicsAllocation *preallocatedHostAllocation,
                                                               const void *hostPtr, uint64_t memObjGpuVa,
                                                               uint64_t hostAllocGpuVa, const Vec3<size_t> &hostPtrOffset,
                                                               const Vec3<size_t> &copyOffset, Vec3<size_t> copySize,
                                                               size_t hostRowPitch, size_t hostSlicePitch,
                                                               size_t gpuRowPitch, size_t gpuSlicePitch) {
    GraphicsAllocation *hostAllocation = nullptr;
    auto clearColorAllocation = commandStreamReceiver.getClearColorAllocation();

    copySize.y = copySize.y ? copySize.y : 1;
    copySize.z = copySize.z ? copySize.z : 1;

    if (preallocatedHostAllocation) {
        hostAllocation = preallocatedHostAllocation;
        UNRECOVERABLE_IF(hostAllocGpuVa == 0);
    } else {
        HostPtrSurface hostPtrSurface(hostPtr, static_cast<size_t>(copySize.x * copySize.y * copySize.z), true);
        bool success = commandStreamReceiver.createAllocationForHostSurface(hostPtrSurface, false);
        UNRECOVERABLE_IF(!success);
        hostAllocation = hostPtrSurface.getAllocation();
        hostAllocGpuVa = hostAllocation->getGpuAddress();
    }

    if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection ||
        BlitterConstants::BlitDirection::HostPtrToImage == blitDirection) {
        return {
            nullptr,                       // outputTimestampPacket
            blitDirection,                 // blitDirection
            {},                            // csrDependencies
            AuxTranslationDirection::None, // auxTranslationDirection
            memObjAllocation,              // dstAllocation
            hostAllocation,                // srcAllocation
            clearColorAllocation,          // clearColorAllocation
            memObjGpuVa,                   // dstGpuAddress
            hostAllocGpuVa,                // srcGpuAddress
            copySize,                      // copySize
            copyOffset,                    // dstOffset
            hostPtrOffset,                 // srcOffset
            true,
            gpuRowPitch,    // dstRowPitch
            gpuSlicePitch,  // dstSlicePitch
            hostRowPitch,   // srcRowPitch
            hostSlicePitch, // srcSlicePitch
            copySize,       // dstSize
            copySize        // srcSize
        };

    } else {
        return {
            nullptr,                       // outputTimestampPacket
            blitDirection,                 // blitDirection
            {},                            // csrDependencies
            AuxTranslationDirection::None, // auxTranslationDirection
            hostAllocation,                // dstAllocation
            memObjAllocation,              // srcAllocation
            clearColorAllocation,          // clearColorAllocation
            hostAllocGpuVa,                // dstGpuAddress
            memObjGpuVa,                   // srcGpuAddress
            copySize,                      // copySize
            hostPtrOffset,                 // dstOffset
            copyOffset,                    // srcOffset
            true,
            hostRowPitch,   // dstRowPitch
            hostSlicePitch, // dstSlicePitch
            gpuRowPitch,    // srcRowPitch
            gpuSlicePitch,  // srcSlicePitch
            copySize,       // dstSize
            copySize        // srcSize
        };
    };
}

BlitProperties BlitProperties::constructPropertiesForCopy(GraphicsAllocation *dstAllocation, GraphicsAllocation *srcAllocation,
                                                          const Vec3<size_t> &dstOffset, const Vec3<size_t> &srcOffset, Vec3<size_t> copySize,
                                                          size_t srcRowPitch, size_t srcSlicePitch,
                                                          size_t dstRowPitch, size_t dstSlicePitch, GraphicsAllocation *clearColorAllocation) {
    copySize.y = copySize.y ? copySize.y : 1;
    copySize.z = copySize.z ? copySize.z : 1;

    return {
        nullptr,                                         // outputTimestampPacket
        BlitterConstants::BlitDirection::BufferToBuffer, // blitDirection
        {},                                              // csrDependencies
        AuxTranslationDirection::None,                   // auxTranslationDirection
        dstAllocation,                                   // dstAllocation
        srcAllocation,                                   // srcAllocation
        clearColorAllocation,                            // clearColorAllocation
        dstAllocation->getGpuAddress(),                  // dstGpuAddress
        srcAllocation->getGpuAddress(),                  // srcGpuAddress
        copySize,                                        // copySize
        dstOffset,                                       // dstOffset
        srcOffset,                                       // srcOffset
        MemoryPoolHelper::isSystemMemoryPool(dstAllocation->getMemoryPool(), srcAllocation->getMemoryPool()),
        dstRowPitch,    // dstRowPitch
        dstSlicePitch,  // dstSlicePitch
        srcRowPitch,    // srcRowPitch
        srcSlicePitch}; // srcSlicePitch
}

BlitProperties BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection auxTranslationDirection,
                                                                    GraphicsAllocation *allocation, GraphicsAllocation *clearColorAllocation) {

    auto allocationSize = allocation->getUnderlyingBufferSize();
    return {
        nullptr,                                         // outputTimestampPacket
        BlitterConstants::BlitDirection::BufferToBuffer, // blitDirection
        {},                                              // csrDependencies
        auxTranslationDirection,                         // auxTranslationDirection
        allocation,                                      // dstAllocation
        allocation,                                      // srcAllocation
        clearColorAllocation,                            // clearColorAllocation
        allocation->getGpuAddress(),                     // dstGpuAddress
        allocation->getGpuAddress(),                     // srcGpuAddress
        {allocationSize, 1, 1},                          // copySize
        0,                                               // dstOffset
        0,                                               // srcOffset
        MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool())};
}

void BlitProperties::setupDependenciesForAuxTranslation(BlitPropertiesContainer &blitPropertiesContainer, TimestampPacketDependencies &timestampPacketDependencies,
                                                        TimestampPacketContainer &kernelTimestamps, const CsrDependencies &depsFromEvents,
                                                        CommandStreamReceiver &gpguCsr, CommandStreamReceiver &bcsCsr) {
    auto numObjects = blitPropertiesContainer.size() / 2;

    for (size_t i = 0; i < numObjects; i++) {
        blitPropertiesContainer[i].outputTimestampPacket = timestampPacketDependencies.auxToNonAuxNodes.peekNodes()[i];
        blitPropertiesContainer[i + numObjects].outputTimestampPacket = timestampPacketDependencies.nonAuxToAuxNodes.peekNodes()[i];
    }

    gpguCsr.requestStallingCommandsOnNextFlush();
    auto nodesAllocator = gpguCsr.getTimestampPacketAllocator();
    timestampPacketDependencies.barrierNodes.add(nodesAllocator->getTag());

    // wait for barrier and events before AuxToNonAux
    blitPropertiesContainer[0].csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.barrierNodes);

    for (auto dep : depsFromEvents.timestampPacketContainer) {
        blitPropertiesContainer[0].csrDependencies.timestampPacketContainer.push_back(dep);
    }

    // wait for NDR before NonAuxToAux
    blitPropertiesContainer[numObjects].csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.cacheFlushNodes);
    blitPropertiesContainer[numObjects].csrDependencies.timestampPacketContainer.push_back(&kernelTimestamps);
}

bool BlitProperties::isImageOperation() const {
    return blitDirection == BlitterConstants::BlitDirection::HostPtrToImage ||
           blitDirection == BlitterConstants::BlitDirection::ImageToHostPtr ||
           blitDirection == BlitterConstants::BlitDirection::ImageToImage;
}

} // namespace NEO