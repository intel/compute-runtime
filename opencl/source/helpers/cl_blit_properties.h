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

        return BlitProperties::constructPropertiesForReadWriteBuffer(blitDirection, commandStreamReceiver, gpuAllocation,
                                                                     hostAllocation, hostPtr, memObjGpuVa, hostAllocGpuVa,
                                                                     hostPtrOffset, copyOffset, builtinOpParams.size.x);
    }

    static BlitterConstants::BlitDirection obtainBlitDirection(uint32_t commandType) {
        if (CL_COMMAND_WRITE_BUFFER == commandType) {
            return BlitterConstants::BlitDirection::HostPtrToBuffer;
        } else if (CL_COMMAND_READ_BUFFER == commandType) {
            return BlitterConstants::BlitDirection::BufferToHostPtr;
        } else {
            UNRECOVERABLE_IF(CL_COMMAND_COPY_BUFFER != commandType);
            return BlitterConstants::BlitDirection::BufferToBuffer;
        }
    }
};

} // namespace NEO
