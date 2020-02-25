/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/blit_commands_helper.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"

#include "CL/cl.h"

namespace NEO {

struct ClBlitProperties {
    static BlitProperties constructProperties(BlitterConstants::BlitDirection blitDirection,
                                              CommandStreamReceiver &commandStreamReceiver,
                                              const BuiltinOpParams &builtinOpParams) {

        if (BlitterConstants::BlitDirection::BufferToBuffer == blitDirection) {
            auto dstOffset = builtinOpParams.dstOffset.x + builtinOpParams.dstMemObj->getOffset();
            auto srcOffset = builtinOpParams.srcOffset.x + builtinOpParams.srcMemObj->getOffset();

            return BlitProperties::constructPropertiesForCopyBuffer(builtinOpParams.dstMemObj->getGraphicsAllocation(),
                                                                    builtinOpParams.srcMemObj->getGraphicsAllocation(),
                                                                    dstOffset, srcOffset, builtinOpParams.size.x);
        }

        GraphicsAllocation *gpuAllocation = nullptr;
        Vec3<size_t> copyOffset = 0;

        void *hostPtr = nullptr;
        Vec3<size_t> hostPtrOffset = 0;

        uint64_t memObjGpuVa = 0;
        uint64_t hostAllocGpuVa = 0;

        GraphicsAllocation *hostAllocation = builtinOpParams.transferAllocation;

        Vec3<size_t> copySize = 0;
        size_t hostRowPitch = 0;
        size_t hostSlicePitch = 0;
        size_t gpuRowPitch = 0;
        size_t gpuSlicePitch = 0;

        if (BlitterConstants::BlitDirection::HostPtrToBuffer == blitDirection) {
            // write buffer
            hostPtr = builtinOpParams.srcPtr;
            hostPtrOffset = builtinOpParams.srcOffset;
            copyOffset = builtinOpParams.dstOffset;

            memObjGpuVa = castToUint64(builtinOpParams.dstPtr);
            hostAllocGpuVa = castToUint64(builtinOpParams.srcPtr);

            if (builtinOpParams.dstSvmAlloc) {
                gpuAllocation = builtinOpParams.dstSvmAlloc;
                hostAllocation = builtinOpParams.srcSvmAlloc;
            } else {
                gpuAllocation = builtinOpParams.dstMemObj->getGraphicsAllocation();
                memObjGpuVa = (gpuAllocation->getGpuAddress() + builtinOpParams.dstMemObj->getOffset());
            }
            copySize.x = builtinOpParams.size.x;
        }

        if (BlitterConstants::BlitDirection::BufferToHostPtr == blitDirection) {
            // read buffer
            hostPtr = builtinOpParams.dstPtr;

            hostPtrOffset = builtinOpParams.dstOffset;
            copyOffset = builtinOpParams.srcOffset;

            memObjGpuVa = castToUint64(builtinOpParams.srcPtr);
            hostAllocGpuVa = castToUint64(builtinOpParams.dstPtr);

            if (builtinOpParams.srcSvmAlloc) {
                gpuAllocation = builtinOpParams.srcSvmAlloc;
                hostAllocation = builtinOpParams.dstSvmAlloc;
            } else {
                gpuAllocation = builtinOpParams.srcMemObj->getGraphicsAllocation();
                memObjGpuVa = (gpuAllocation->getGpuAddress() + builtinOpParams.srcMemObj->getOffset());
            }

            hostRowPitch = builtinOpParams.dstRowPitch;
            hostSlicePitch = builtinOpParams.dstSlicePitch;
            gpuRowPitch = builtinOpParams.srcRowPitch;
            gpuSlicePitch = builtinOpParams.srcSlicePitch;
            copySize = builtinOpParams.size;
        }

        UNRECOVERABLE_IF(BlitterConstants::BlitDirection::HostPtrToBuffer != blitDirection &&
                         BlitterConstants::BlitDirection::BufferToHostPtr != blitDirection);

        return BlitProperties::constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, gpuAllocation,
                                                                     hostAllocation, hostPtr, memObjGpuVa, hostAllocGpuVa,
                                                                     hostPtrOffset, copyOffset, copySize,
                                                                     hostRowPitch, hostSlicePitch,
                                                                     gpuRowPitch, gpuSlicePitch);
    }

    static BlitterConstants::BlitDirection obtainBlitDirection(uint32_t commandType) {
        if (CL_COMMAND_WRITE_BUFFER == commandType) {
            return BlitterConstants::BlitDirection::HostPtrToBuffer;
        } else if (CL_COMMAND_READ_BUFFER == commandType || CL_COMMAND_READ_BUFFER_RECT == commandType) {
            return BlitterConstants::BlitDirection::BufferToHostPtr;
        } else {
            UNRECOVERABLE_IF(CL_COMMAND_COPY_BUFFER != commandType);
            return BlitterConstants::BlitDirection::BufferToBuffer;
        }
    }
};

} // namespace NEO
