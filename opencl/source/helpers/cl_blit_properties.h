/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/blit_properties.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/mem_obj/image.h"

#include "CL/cl.h"

namespace NEO {

struct ClBlitProperties {
    static BlitProperties constructProperties(BlitterConstants::BlitDirection blitDirection,
                                              CommandStreamReceiver &commandStreamReceiver,
                                              const BuiltinOpParams &builtinOpParams) {

        auto rootDeviceIndex = commandStreamReceiver.getRootDeviceIndex();
        auto clearColorAllocation = commandStreamReceiver.getClearColorAllocation();
        BlitProperties blitProperties{};
        if (BlitterConstants::BlitDirection::bufferToBuffer == blitDirection ||
            BlitterConstants::BlitDirection::imageToImage == blitDirection) {
            auto dstOffset = builtinOpParams.dstOffset.x;
            auto srcOffset = builtinOpParams.srcOffset.x;
            GraphicsAllocation *dstAllocation = nullptr;
            GraphicsAllocation *srcAllocation = nullptr;

            if (!builtinOpParams.dstSvmAlloc) {
                dstOffset += builtinOpParams.dstMemObj->getOffset(); // NOLINT(clang-analyzer-core.CallAndMessage)
                srcOffset += builtinOpParams.srcMemObj->getOffset();
                dstAllocation = builtinOpParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex);
                srcAllocation = builtinOpParams.srcMemObj->getGraphicsAllocation(rootDeviceIndex);
            } else {
                dstAllocation = builtinOpParams.dstSvmAlloc;
                srcAllocation = builtinOpParams.srcSvmAlloc;
                dstOffset += ptrDiff(builtinOpParams.dstPtr, dstAllocation->getGpuAddress());
                srcOffset += ptrDiff(builtinOpParams.srcPtr, srcAllocation->getGpuAddress());
            }

            blitProperties = BlitProperties::constructPropertiesForCopy(
                dstAllocation, 0,
                srcAllocation, 0,
                {dstOffset, builtinOpParams.dstOffset.y, builtinOpParams.dstOffset.z},
                {srcOffset, builtinOpParams.srcOffset.y, builtinOpParams.srcOffset.z},
                builtinOpParams.size,
                builtinOpParams.srcRowPitch, builtinOpParams.srcSlicePitch,
                builtinOpParams.dstRowPitch, builtinOpParams.dstSlicePitch, clearColorAllocation);
            if (BlitterConstants::BlitDirection::imageToImage == blitDirection) {
                blitProperties.blitDirection = blitDirection;
                setBlitPropertiesForImage(blitProperties, builtinOpParams);
            }
            blitProperties.transform1DArrayTo2DArrayIfNeeded();
            return blitProperties;
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

        if (BlitterConstants::BlitDirection::hostPtrToBuffer == blitDirection ||
            BlitterConstants::BlitDirection::hostPtrToImage == blitDirection) {
            // write buffer/image
            hostPtr = builtinOpParams.srcPtr;
            hostPtrOffset = builtinOpParams.srcOffset;
            copyOffset = builtinOpParams.dstOffset;

            memObjGpuVa = castToUint64(builtinOpParams.dstPtr);
            hostAllocGpuVa = castToUint64(builtinOpParams.srcPtr);

            if (builtinOpParams.dstSvmAlloc) {
                gpuAllocation = builtinOpParams.dstSvmAlloc;
                hostAllocation = builtinOpParams.srcSvmAlloc;
            } else {
                gpuAllocation = builtinOpParams.dstMemObj->getGraphicsAllocation(rootDeviceIndex);
                memObjGpuVa = (gpuAllocation->getGpuAddress() + builtinOpParams.dstMemObj->getOffset());
            }

            hostRowPitch = builtinOpParams.srcRowPitch;
            hostSlicePitch = builtinOpParams.srcSlicePitch;
            gpuRowPitch = builtinOpParams.dstRowPitch;
            gpuSlicePitch = builtinOpParams.dstSlicePitch;
            copySize = builtinOpParams.size;
        }

        if (BlitterConstants::BlitDirection::bufferToHostPtr == blitDirection ||
            BlitterConstants::BlitDirection::imageToHostPtr == blitDirection) {
            // read buffer/image
            hostPtr = builtinOpParams.dstPtr;
            hostPtrOffset = builtinOpParams.dstOffset;
            copyOffset = builtinOpParams.srcOffset;

            memObjGpuVa = castToUint64(builtinOpParams.srcPtr);
            hostAllocGpuVa = castToUint64(builtinOpParams.dstPtr);

            if (builtinOpParams.srcSvmAlloc) {
                gpuAllocation = builtinOpParams.srcSvmAlloc;
                hostAllocation = builtinOpParams.dstSvmAlloc;
            } else {
                gpuAllocation = builtinOpParams.srcMemObj->getGraphicsAllocation(rootDeviceIndex);
                memObjGpuVa = (gpuAllocation->getGpuAddress() + builtinOpParams.srcMemObj->getOffset());
            }

            hostRowPitch = builtinOpParams.dstRowPitch;
            hostSlicePitch = builtinOpParams.dstSlicePitch;
            gpuRowPitch = builtinOpParams.srcRowPitch;
            gpuSlicePitch = builtinOpParams.srcSlicePitch;
            copySize = builtinOpParams.size;
        }

        UNRECOVERABLE_IF(BlitterConstants::BlitDirection::hostPtrToBuffer != blitDirection &&
                         BlitterConstants::BlitDirection::bufferToHostPtr != blitDirection &&
                         BlitterConstants::BlitDirection::hostPtrToImage != blitDirection &&
                         BlitterConstants::BlitDirection::imageToHostPtr != blitDirection);

        blitProperties = BlitProperties::constructPropertiesForReadWrite(blitDirection, commandStreamReceiver, gpuAllocation,
                                                                         hostAllocation, hostPtr, memObjGpuVa, hostAllocGpuVa,
                                                                         hostPtrOffset, copyOffset, copySize,
                                                                         hostRowPitch, hostSlicePitch,
                                                                         gpuRowPitch, gpuSlicePitch);

        if (BlitterConstants::BlitDirection::hostPtrToImage == blitDirection ||
            BlitterConstants::BlitDirection::imageToHostPtr == blitDirection) {
            setBlitPropertiesForImage(blitProperties, builtinOpParams);
        }

        blitProperties.transform1DArrayTo2DArrayIfNeeded();
        return blitProperties;
    }

    static BlitterConstants::BlitDirection obtainBlitDirection(uint32_t commandType) {

        switch (commandType) {
        case CL_COMMAND_WRITE_BUFFER:
        case CL_COMMAND_WRITE_BUFFER_RECT:
            return BlitterConstants::BlitDirection::hostPtrToBuffer;
        case CL_COMMAND_READ_BUFFER:
        case CL_COMMAND_READ_BUFFER_RECT:
            return BlitterConstants::BlitDirection::bufferToHostPtr;
        case CL_COMMAND_COPY_BUFFER:
        case CL_COMMAND_COPY_BUFFER_RECT:
        case CL_COMMAND_SVM_MEMCPY:
            return BlitterConstants::BlitDirection::bufferToBuffer;
        case CL_COMMAND_WRITE_IMAGE:
            return BlitterConstants::BlitDirection::hostPtrToImage;
        case CL_COMMAND_READ_IMAGE:
            return BlitterConstants::BlitDirection::imageToHostPtr;
        case CL_COMMAND_COPY_IMAGE:
            return BlitterConstants::BlitDirection::imageToImage;
        default:
            UNRECOVERABLE_IF(true);
        }
    }

    static void setBlitPropertiesForImage(BlitProperties &blitProperties, const BuiltinOpParams &builtinOpParams) {
        DEBUG_BREAK_IF(!blitProperties.isImageOperation());

        auto srcImage = castToObject<Image>(builtinOpParams.srcMemObj);
        auto dstImage = castToObject<Image>(builtinOpParams.dstMemObj);

        bool src1DBuffer = false, dst1DBuffer = false;

        if (srcImage) {
            const auto &imageDesc = srcImage->getImageDesc();
            blitProperties.srcSize.x = imageDesc.image_width;
            blitProperties.srcSize.y = std::max(imageDesc.image_height, size_t(1));
            blitProperties.srcSize.z = std::max(imageDesc.image_depth, size_t(1));
            if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
                blitProperties.srcSize.z = std::max(blitProperties.srcSize.z, imageDesc.image_array_size);
            }
            blitProperties.srcRowPitch = imageDesc.image_row_pitch;
            blitProperties.srcSlicePitch = imageDesc.image_slice_pitch;
            blitProperties.srcPlane = srcImage->getPlane();
            SurfaceOffsets surfaceOffsets;
            srcImage->getSurfaceOffsets(surfaceOffsets);
            blitProperties.srcGpuAddress += surfaceOffsets.offset;
            blitProperties.bytesPerPixel = srcImage->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
            if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
                src1DBuffer = true;
            }
        }

        if (dstImage) {
            const auto &imageDesc = dstImage->getImageDesc();
            blitProperties.dstSize.x = imageDesc.image_width;
            blitProperties.dstSize.y = std::max(imageDesc.image_height, size_t(1));
            blitProperties.dstSize.z = std::max(imageDesc.image_depth, size_t(1));
            if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
                blitProperties.dstSize.z = std::max(blitProperties.dstSize.z, imageDesc.image_array_size);
            }
            blitProperties.dstRowPitch = imageDesc.image_row_pitch;
            blitProperties.dstSlicePitch = imageDesc.image_slice_pitch;
            blitProperties.dstPlane = dstImage->getPlane();
            SurfaceOffsets surfaceOffsets;
            dstImage->getSurfaceOffsets(surfaceOffsets);
            blitProperties.dstGpuAddress += surfaceOffsets.offset;
            blitProperties.bytesPerPixel = dstImage->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
            if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
                dst1DBuffer = true;
            }
        }

        if (src1DBuffer && dst1DBuffer) {
            blitProperties.srcSize.x *= blitProperties.bytesPerPixel;
            blitProperties.dstSize.x *= blitProperties.bytesPerPixel;
            blitProperties.srcOffset.x *= blitProperties.bytesPerPixel;
            blitProperties.dstOffset.x *= blitProperties.bytesPerPixel;
            blitProperties.copySize.x *= blitProperties.bytesPerPixel;
            blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToBuffer;
            blitProperties.bytesPerPixel = 1;
        } else if (src1DBuffer) {
            blitProperties.srcSize.x *= blitProperties.bytesPerPixel;
            blitProperties.srcOffset.x *= blitProperties.bytesPerPixel;
            blitProperties.copySize.x *= blitProperties.bytesPerPixel;
            blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
            blitProperties.bytesPerPixel = 1;
        } else if (dst1DBuffer) {
            blitProperties.dstSize.x *= blitProperties.bytesPerPixel;
            blitProperties.dstOffset.x *= blitProperties.bytesPerPixel;
            blitProperties.copySize.x *= blitProperties.bytesPerPixel;
            blitProperties.blitDirection = BlitterConstants::BlitDirection::hostPtrToBuffer;
            blitProperties.bytesPerPixel = 1;
        }

        blitProperties.srcRowPitch = blitProperties.srcRowPitch ? blitProperties.srcRowPitch : blitProperties.srcSize.x * blitProperties.bytesPerPixel;
        blitProperties.dstRowPitch = blitProperties.dstRowPitch ? blitProperties.dstRowPitch : blitProperties.dstSize.x * blitProperties.bytesPerPixel;
        blitProperties.srcSlicePitch = blitProperties.srcSlicePitch ? blitProperties.srcSlicePitch : blitProperties.srcRowPitch * blitProperties.srcSize.y;
        blitProperties.dstSlicePitch = blitProperties.dstSlicePitch ? blitProperties.dstSlicePitch : blitProperties.dstRowPitch * blitProperties.dstSize.y;
    }
};

} // namespace NEO
