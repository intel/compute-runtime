/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/image.h"

#include "common/compiler_support.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include "igfxfmid.h"

#include <map>

namespace NEO {

ImageFuncs imageFactory[IGFX_MAX_CORE] = {};

Image::Image(Context *context,
             cl_mem_flags flags,
             size_t size,
             void *hostPtr,
             cl_image_format imageFormat,
             const cl_image_desc &imageDesc,
             bool zeroCopy,
             GraphicsAllocation *graphicsAllocation,
             bool isObjectRedescribed,
             bool createTiledImage,
             uint32_t baseMipLevel,
             uint32_t mipCount,
             const SurfaceFormatInfo &surfaceFormatInfo,
             const SurfaceOffsets *surfaceOffsets)
    : MemObj(context,
             imageDesc.image_type,
             flags,
             size,
             graphicsAllocation->getUnderlyingBuffer(),
             hostPtr,
             graphicsAllocation,
             zeroCopy,
             false,
             isObjectRedescribed),
      createFunction(nullptr),
      isTiledImage(createTiledImage),
      imageFormat(std::move(imageFormat)),
      imageDesc(imageDesc),
      surfaceFormatInfo(surfaceFormatInfo),
      cubeFaceIndex(__GMM_NO_CUBE_MAP),
      mediaPlaneType(0),
      baseMipLevel(baseMipLevel),
      mipCount(mipCount) {
    magic = objectMagic;
    if (surfaceOffsets)
        setSurfaceOffsets(surfaceOffsets->offset, surfaceOffsets->xOffset, surfaceOffsets->yOffset, surfaceOffsets->yOffsetForUVplane);
    else
        setSurfaceOffsets(0, 0, 0, 0);
}

void Image::transferData(void *dest, size_t destRowPitch, size_t destSlicePitch,
                         void *src, size_t srcRowPitch, size_t srcSlicePitch,
                         std::array<size_t, 3> copyRegion, std::array<size_t, 3> copyOrigin) {

    size_t pixelSize = surfaceFormatInfo.ImageElementSizeInBytes;
    size_t lineWidth = copyRegion[0] * pixelSize;

    DBG_LOG(LogMemoryObject, __FUNCTION__, "memcpy dest:", dest, "sizeRowToCopy:", lineWidth, "src:", src);

    if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        // For 1DArray type, array region and origin are stored on 2nd position. For 2Darray its on 3rd position.
        std::swap(copyOrigin[1], copyOrigin[2]);
        std::swap(copyRegion[1], copyRegion[2]);
    }

    for (size_t slice = copyOrigin[2]; slice < (copyOrigin[2] + copyRegion[2]); slice++) {
        auto srcSliceOffset = ptrOffset(src, srcSlicePitch * slice);
        auto dstSliceOffset = ptrOffset(dest, destSlicePitch * slice);

        for (size_t height = copyOrigin[1]; height < (copyOrigin[1] + copyRegion[1]); height++) {
            auto srcRowOffset = ptrOffset(srcSliceOffset, srcRowPitch * height);
            auto dstRowOffset = ptrOffset(dstSliceOffset, destRowPitch * height);

            memcpy_s(ptrOffset(dstRowOffset, copyOrigin[0] * pixelSize), lineWidth,
                     ptrOffset(srcRowOffset, copyOrigin[0] * pixelSize), lineWidth);
        }
    }
}

Image::~Image() = default;

Image *Image::create(Context *context,
                     cl_mem_flags flags,
                     const SurfaceFormatInfo *surfaceFormat,
                     const cl_image_desc *imageDesc,
                     const void *hostPtr,
                     cl_int &errcodeRet) {
    UNRECOVERABLE_IF(surfaceFormat == nullptr);
    Image *image = nullptr;
    GraphicsAllocation *memory = nullptr;
    MemoryManager *memoryManager = context->getMemoryManager();
    Buffer *parentBuffer = castToObject<Buffer>(imageDesc->mem_object);
    Image *parentImage = castToObject<Image>(imageDesc->mem_object);

    do {
        size_t imageWidth = imageDesc->image_width;
        size_t imageHeight = 1;
        size_t imageDepth = 1;
        size_t imageCount = 1;
        size_t hostPtrMinSize = 0;

        cl_image_desc imageDescriptor = *imageDesc;
        ImageInfo imgInfo = {0};
        void *hostPtrToSet = nullptr;

        if (flags & CL_MEM_USE_HOST_PTR) {
            hostPtrToSet = const_cast<void *>(hostPtr);
        }

        imgInfo.imgDesc = &imageDescriptor;
        imgInfo.surfaceFormat = surfaceFormat;
        imgInfo.mipCount = imageDesc->num_mip_levels;
        Gmm *gmm = nullptr;

        if (imageDesc->image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
            imageCount = imageDesc->image_array_size;
        }

        switch (imageDesc->image_type) {
        case CL_MEM_OBJECT_IMAGE3D:
            imageDepth = imageDesc->image_depth;
            CPP_ATTRIBUTE_FALLTHROUGH;
        case CL_MEM_OBJECT_IMAGE2D:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            imageHeight = imageDesc->image_height;
        case CL_MEM_OBJECT_IMAGE1D:
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
            break;
        default:
            DEBUG_BREAK_IF("Unsupported cl_image_type");
            break;
        }

        if (parentImage) {
            imageWidth = parentImage->getImageDesc().image_width;
            imageHeight = parentImage->getImageDesc().image_height;
            imageDepth = 1;
            if (IsNV12Image(&parentImage->getImageFormat())) {
                if (imageDesc->image_depth == 1) { // UV Plane
                    imageWidth /= 2;
                    imageHeight /= 2;
                    imgInfo.plane = GMM_PLANE_U;
                } else {
                    imgInfo.plane = GMM_PLANE_Y;
                }
            }

            imgInfo.surfaceFormat = &parentImage->surfaceFormatInfo;
            imageDescriptor = parentImage->getImageDesc();
        }

        auto hostPtrRowPitch = imageDesc->image_row_pitch ? imageDesc->image_row_pitch : imageWidth * surfaceFormat->ImageElementSizeInBytes;
        auto hostPtrSlicePitch = imageDesc->image_slice_pitch ? imageDesc->image_slice_pitch : hostPtrRowPitch * imageHeight;
        auto isTilingAllowed = context->isSharedContext ? false : GmmHelper::allowTiling(*imageDesc) && !MemObjHelper::isLinearStorageForced(flags);
        imgInfo.preferRenderCompression = MemObjHelper::isSuitableForRenderCompression(isTilingAllowed, flags,
                                                                                       context->peekContextType(), true);

        bool zeroCopy = false;
        bool transferNeeded = false;
        if (((imageDesc->image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) || (imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D)) && (parentBuffer != nullptr)) {

            HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily).checkResourceCompatibility(parentBuffer, errcodeRet);

            if (errcodeRet != CL_SUCCESS) {
                return nullptr;
            }

            memory = parentBuffer->getGraphicsAllocation();
            // Image from buffer - we never allocate memory, we use what buffer provides
            zeroCopy = true;
            hostPtr = parentBuffer->getHostPtr();
            hostPtrToSet = const_cast<void *>(hostPtr);
            parentBuffer->incRefInternal();
            GmmHelper::queryImgFromBufferParams(imgInfo, memory);

            UNRECOVERABLE_IF(imgInfo.offset != 0);
            imgInfo.offset = parentBuffer->getOffset();

            if (memoryManager->peekVirtualPaddingSupport() && (imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D)) {
                // Retrieve sizes from GMM and apply virtual padding if buffer storage is not big enough
                auto queryGmmImgInfo(imgInfo);
                std::unique_ptr<Gmm> gmm(new Gmm(queryGmmImgInfo));
                auto gmmAllocationSize = gmm->gmmResourceInfo->getSizeAllocation();
                if (gmmAllocationSize > memory->getUnderlyingBufferSize()) {
                    memory = memoryManager->createGraphicsAllocationWithPadding(memory, gmmAllocationSize);
                }
            }
        } else if (parentImage != nullptr) {
            memory = parentImage->getGraphicsAllocation();
            memory->getDefaultGmm()->queryImageParams(imgInfo);
            isTilingAllowed = parentImage->allowTiling();
        } else {
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            if (flags & CL_MEM_USE_HOST_PTR) {

                if (!context->isSharedContext) {
                    AllocationProperties allocProperties = MemObjHelper::getAllocationProperties(imgInfo, false, flags);

                    memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties, hostPtr);

                    if (memory) {
                        if (memory->getUnderlyingBuffer() != hostPtr) {
                            zeroCopy = false;
                            transferNeeded = true;
                        } else {
                            zeroCopy = true;
                        }
                    }
                } else {
                    gmm = new Gmm(imgInfo);
                    memory = memoryManager->allocateGraphicsMemoryWithProperties({false, imgInfo.size, GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE}, hostPtr);
                    memory->setDefaultGmm(gmm);
                    zeroCopy = true;
                }

            } else {
                AllocationProperties allocProperties = MemObjHelper::getAllocationProperties(imgInfo, true, flags);
                memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);

                if (memory && MemoryPool::isSystemMemoryPool(memory->getMemoryPool())) {
                    zeroCopy = true;
                }
            }
        }
        transferNeeded |= !!(flags & CL_MEM_COPY_HOST_PTR);

        switch (imageDesc->image_type) {
        case CL_MEM_OBJECT_IMAGE3D:
            hostPtrMinSize = hostPtrSlicePitch * imageDepth;
            break;
        case CL_MEM_OBJECT_IMAGE2D:
            if (IsNV12Image(&surfaceFormat->OCLImageFormat)) {
                hostPtrMinSize = hostPtrRowPitch * imageHeight + hostPtrRowPitch * imageHeight / 2;
            } else {
                hostPtrMinSize = hostPtrRowPitch * imageHeight;
            }
            hostPtrSlicePitch = 0;
            break;
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            hostPtrMinSize = hostPtrSlicePitch * imageCount;
            break;
        case CL_MEM_OBJECT_IMAGE1D:
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
            hostPtrMinSize = hostPtrRowPitch;
            hostPtrSlicePitch = 0;
            break;
        default:
            DEBUG_BREAK_IF("Unsupported cl_image_type");
            break;
        }

        if (!memory) {
            break;
        }

        if (parentBuffer == nullptr) {
            memory->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
        }
        memory->setMemObjectsAllocationWithWritableFlags(!(flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)));

        DBG_LOG(LogMemoryObject, __FUNCTION__, "hostPtr:", hostPtr, "size:", memory->getUnderlyingBufferSize(), "memoryStorage:", memory->getUnderlyingBuffer(), "GPU address:", std::hex, memory->getGpuAddress());

        if (parentImage) {
            imageDescriptor.image_height = imageHeight;
            imageDescriptor.image_width = imageWidth;
            imageDescriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
            imageDescriptor.image_depth = 1;
            imageDescriptor.image_array_size = 0;
            imageDescriptor.image_row_pitch = 0;
            imageDescriptor.image_slice_pitch = 0;
            imageDescriptor.mem_object = imageDesc->mem_object;
            parentImage->incRefInternal();
        }

        image = createImageHw(context, flags, imgInfo.size, hostPtrToSet, surfaceFormat->OCLImageFormat,
                              imageDescriptor, zeroCopy, memory, false, isTilingAllowed, 0, 0, surfaceFormat);

        if (imageDesc->image_type != CL_MEM_OBJECT_IMAGE1D_ARRAY && imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D_ARRAY) {
            image->imageDesc.image_array_size = 0;
        }
        if ((imageDesc->image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) || ((imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D) && (imageDesc->mem_object != nullptr))) {
            image->associatedMemObject = castToObject<MemObj>(imageDesc->mem_object);
        }

        // Driver needs to store rowPitch passed by the app in order to synchronize the host_ptr later on map call
        image->setHostPtrRowPitch(imageDesc->image_row_pitch ? imageDesc->image_row_pitch : hostPtrRowPitch);
        image->setHostPtrSlicePitch(hostPtrSlicePitch);
        image->setImageCount(imageCount);
        image->setHostPtrMinSize(hostPtrMinSize);
        image->setImageRowPitch(imgInfo.rowPitch);
        image->setImageSlicePitch(imgInfo.slicePitch);
        image->setQPitch(imgInfo.qPitch);
        image->setSurfaceOffsets(imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane);
        image->setMipCount(imgInfo.mipCount);
        if (parentImage) {
            image->setMediaPlaneType(static_cast<cl_uint>(imageDesc->image_depth));
            image->setParentSharingHandler(parentImage->getSharingHandler());
        }
        if (parentBuffer) {
            image->setParentSharingHandler(parentBuffer->getSharingHandler());
        }
        errcodeRet = CL_SUCCESS;
        if (context->isProvidingPerformanceHints() && image->isMemObjZeroCopy()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS, static_cast<cl_mem>(image));
        }
        if (transferNeeded) {
            std::array<size_t, 3> copyOrigin = {{0, 0, 0}};
            std::array<size_t, 3> copyRegion = {{imageWidth, imageHeight, std::max(imageDepth, imageCount)}};
            if (imageDesc->image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
                copyRegion = {{imageWidth, imageCount, 1}};
            } else {
                copyRegion = {{imageWidth, imageHeight, std::max(imageDepth, imageCount)}};
            }

            if (isTilingAllowed || !MemoryPool::isSystemMemoryPool(memory->getMemoryPool())) {
                auto cmdQ = context->getSpecialQueue();

                if (IsNV12Image(&image->getImageFormat())) {
                    errcodeRet = image->writeNV12Planes(hostPtr, hostPtrRowPitch);
                } else {
                    errcodeRet = cmdQ->enqueueWriteImage(image, CL_TRUE, &copyOrigin[0], &copyRegion[0],
                                                         hostPtrRowPitch, hostPtrSlicePitch,
                                                         hostPtr, nullptr, 0, nullptr, nullptr);
                }
            } else {
                image->transferData(memory->getUnderlyingBuffer(), imgInfo.rowPitch, imgInfo.slicePitch,
                                    const_cast<void *>(hostPtr), hostPtrRowPitch, hostPtrSlicePitch,
                                    copyRegion, copyOrigin);
            }
        }

        if (errcodeRet != CL_SUCCESS) {
            image->release();
            image = nullptr;
            memory = nullptr;
            break;
        }
    } while (false);

    return image;
}

Image *Image::createImageHw(Context *context, cl_mem_flags flags, size_t size, void *hostPtr,
                            const cl_image_format &imageFormat, const cl_image_desc &imageDesc,
                            bool zeroCopy, GraphicsAllocation *graphicsAllocation,
                            bool isObjectRedescribed, bool createTiledImage, uint32_t baseMipLevel, uint32_t mipCount,
                            const SurfaceFormatInfo *surfaceFormatInfo) {
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = imageFactory[hwInfo.platform.eRenderCoreFamily].createImageFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);
    auto image = funcCreate(context, flags, size, hostPtr, imageFormat, imageDesc,
                            zeroCopy, graphicsAllocation, isObjectRedescribed, createTiledImage, baseMipLevel, mipCount, surfaceFormatInfo, nullptr);
    DEBUG_BREAK_IF(nullptr == image);
    image->createFunction = funcCreate;
    return image;
}

Image *Image::createSharedImage(Context *context, SharingHandler *sharingHandler, McsSurfaceInfo &mcsSurfaceInfo,
                                GraphicsAllocation *graphicsAllocation, GraphicsAllocation *mcsAllocation,
                                cl_mem_flags flags, ImageInfo &imgInfo, uint32_t cubeFaceIndex, uint32_t baseMipLevel, uint32_t mipCount) {
    bool isTiledImage = graphicsAllocation->getDefaultGmm()->gmmResourceInfo->getTileModeSurfaceState() != 0;

    auto sharedImage = createImageHw(context, flags, graphicsAllocation->getUnderlyingBufferSize(),
                                     nullptr, imgInfo.surfaceFormat->OCLImageFormat, *imgInfo.imgDesc, false, graphicsAllocation, false, isTiledImage, baseMipLevel, mipCount, imgInfo.surfaceFormat);
    sharedImage->setSharingHandler(sharingHandler);
    sharedImage->setMcsAllocation(mcsAllocation);
    sharedImage->setQPitch(imgInfo.qPitch);
    sharedImage->setHostPtrRowPitch(imgInfo.imgDesc->image_row_pitch);
    sharedImage->setHostPtrSlicePitch(imgInfo.imgDesc->image_slice_pitch);
    sharedImage->setCubeFaceIndex(cubeFaceIndex);
    sharedImage->setSurfaceOffsets(imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane);
    sharedImage->setMcsSurfaceInfo(mcsSurfaceInfo);
    return sharedImage;
}

cl_int Image::validate(Context *context,
                       cl_mem_flags flags,
                       const SurfaceFormatInfo *surfaceFormat,
                       const cl_image_desc *imageDesc,
                       const void *hostPtr) {
    auto pDevice = context->getDevice(0);
    size_t srcSize = 0;
    size_t retSize = 0;
    const size_t *maxWidth = nullptr;
    const size_t *maxHeight = nullptr;
    const uint32_t *pitchAlignment = nullptr;
    const uint32_t *baseAddressAlignment = nullptr;
    if (!surfaceFormat) {
        return CL_IMAGE_FORMAT_NOT_SUPPORTED;
    }

    Image *parentImage = castToObject<Image>(imageDesc->mem_object);
    Buffer *parentBuffer = castToObject<Buffer>(imageDesc->mem_object);
    if (imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D) {
        pDevice->getCap<CL_DEVICE_IMAGE2D_MAX_WIDTH>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);
        pDevice->getCap<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);
        if (imageDesc->image_width > *maxWidth ||
            imageDesc->image_height > *maxHeight) {
            return CL_INVALID_IMAGE_SIZE;
        }
        if (parentBuffer) { // Image 2d from buffer
            pDevice->getCap<CL_DEVICE_IMAGE_PITCH_ALIGNMENT>(reinterpret_cast<const void *&>(pitchAlignment), srcSize, retSize);
            pDevice->getCap<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT>(reinterpret_cast<const void *&>(baseAddressAlignment), srcSize, retSize);

            const auto rowSize = imageDesc->image_row_pitch != 0 ? imageDesc->image_row_pitch : alignUp(imageDesc->image_width * surfaceFormat->NumChannels * surfaceFormat->PerChannelSizeInBytes, *pitchAlignment);
            const auto minimumBufferSize = imageDesc->image_height * rowSize;

            if ((imageDesc->image_row_pitch % (*pitchAlignment)) ||
                ((parentBuffer->getFlags() & CL_MEM_USE_HOST_PTR) && (reinterpret_cast<uint64_t>(parentBuffer->getHostPtr()) % (*baseAddressAlignment))) ||
                (minimumBufferSize > parentBuffer->getSize())) {
                return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
            } else if (flags & (CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR)) {
                return CL_INVALID_VALUE;
            }
        }
        if (parentImage && !IsNV12Image(&parentImage->getImageFormat())) { // Image 2d from image 2d
            if (!parentImage->hasSameDescriptor(*imageDesc) || !parentImage->hasValidParentImageFormat(surfaceFormat->OCLImageFormat)) {
                return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
            }
        }
        if (!(parentImage && IsNV12Image(&parentImage->getImageFormat())) &&
            (imageDesc->image_width == 0 || imageDesc->image_height == 0)) {
            return CL_INVALID_IMAGE_DESCRIPTOR;
        }
    }
    if (hostPtr == nullptr) {
        if (imageDesc->image_row_pitch != 0 && imageDesc->mem_object == nullptr) {
            return CL_INVALID_IMAGE_DESCRIPTOR;
        }
    } else {
        if (imageDesc->image_row_pitch != 0) {
            if (imageDesc->image_row_pitch % surfaceFormat->ImageElementSizeInBytes != 0 ||
                imageDesc->image_row_pitch < imageDesc->image_width * surfaceFormat->ImageElementSizeInBytes) {
                return CL_INVALID_IMAGE_DESCRIPTOR;
            }
        }
    }

    if (parentBuffer && imageDesc->image_type != CL_MEM_OBJECT_IMAGE1D_BUFFER && imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D) {
        return CL_INVALID_IMAGE_DESCRIPTOR;
    }

    if (parentImage && imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D) {
        return CL_INVALID_IMAGE_DESCRIPTOR;
    }

    return validateImageTraits(context, flags, &surfaceFormat->OCLImageFormat, imageDesc, hostPtr);
}

cl_int Image::validateImageFormat(const cl_image_format *imageFormat) {
    if (!imageFormat) {
        return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
    }
    bool isValidFormat = isValidSingleChannelFormat(imageFormat) ||
                         isValidIntensityFormat(imageFormat) ||
                         isValidLuminanceFormat(imageFormat) ||
                         isValidDepthFormat(imageFormat) ||
                         isValidDoubleChannelFormat(imageFormat) ||
                         isValidTripleChannelFormat(imageFormat) ||
                         isValidRGBAFormat(imageFormat) ||
                         isValidSRGBFormat(imageFormat) ||
                         isValidARGBFormat(imageFormat) ||
                         isValidDepthStencilFormat(imageFormat);
#if SUPPORT_YUV
    isValidFormat = isValidFormat || isValidYUVFormat(imageFormat);
#endif
    if (isValidFormat) {
        return CL_SUCCESS;
    }
    return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
}

cl_int Image::validatePlanarYUV(Context *context,
                                cl_mem_flags flags,
                                const cl_image_desc *imageDesc,
                                const void *hostPtr) {
    cl_int errorCode = CL_SUCCESS;
    auto pDevice = context->getDevice(0);
    const size_t *maxWidth = nullptr;
    const size_t *maxHeight = nullptr;
    size_t srcSize = 0;
    size_t retSize = 0;

    while (true) {

        Image *memObject = castToObject<Image>(imageDesc->mem_object);
        if (memObject != nullptr) {
            if (memObject->memObjectType == CL_MEM_OBJECT_IMAGE2D) {
                if (imageDesc->image_depth != 1 && imageDesc->image_depth != 0) {
                    errorCode = CL_INVALID_IMAGE_DESCRIPTOR;
                }
            }
            break;
        }

        if (imageDesc->mem_object != nullptr) {
            errorCode = CL_INVALID_IMAGE_DESCRIPTOR;
            break;
        }
        if (!(flags & CL_MEM_HOST_NO_ACCESS)) {
            errorCode = CL_INVALID_VALUE;
            break;
        } else {
            if (imageDesc->image_height % 4 ||
                imageDesc->image_width % 4 ||
                imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D) {
                errorCode = CL_INVALID_IMAGE_DESCRIPTOR;
                break;
            }
        }

        pDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);
        pDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);
        if (imageDesc->image_width > *maxWidth || imageDesc->image_height > *maxHeight) {
            errorCode = CL_INVALID_IMAGE_SIZE;
            break;
        }
        break;
    }
    return errorCode;
}

cl_int Image::validatePackedYUV(cl_mem_flags flags, const cl_image_desc *imageDesc) {
    cl_int errorCode = CL_SUCCESS;
    while (true) {
        if (!(flags & CL_MEM_READ_ONLY)) {
            errorCode = CL_INVALID_VALUE;
            break;
        } else {
            if (imageDesc->image_width % 2 != 0 ||
                imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D) {
                errorCode = CL_INVALID_IMAGE_DESCRIPTOR;
                break;
            }
        }
        break;
    }
    return errorCode;
}

cl_int Image::validateImageTraits(Context *context, cl_mem_flags flags, const cl_image_format *imageFormat, const cl_image_desc *imageDesc, const void *hostPtr) {
    if (IsNV12Image(imageFormat))
        return validatePlanarYUV(context, flags, imageDesc, hostPtr);
    else if (IsPackedYuvImage(imageFormat))
        return validatePackedYUV(flags, imageDesc);

    return CL_SUCCESS;
}

size_t Image::calculateHostPtrSize(const size_t *region, size_t rowPitch, size_t slicePitch, size_t pixelSize, uint32_t imageType) {
    DEBUG_BREAK_IF(!((rowPitch != 0) && (slicePitch != 0)));
    size_t sizeToReturn = 0u;

    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        sizeToReturn = region[0] * pixelSize;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        sizeToReturn = (region[1] - 1) * rowPitch + region[0] * pixelSize;
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        sizeToReturn = (region[1] - 1) * slicePitch + region[0] * pixelSize;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        sizeToReturn = (region[2] - 1) * slicePitch + (region[1] - 1) * rowPitch + region[0] * pixelSize;
        break;
    default:
        DEBUG_BREAK_IF("Unsupported cl_image_type");
        break;
    }

    DEBUG_BREAK_IF(sizeToReturn == 0);
    return sizeToReturn;
}

void Image::calculateHostPtrOffset(size_t *imageOffset, const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch, uint32_t imageType, size_t bytesPerPixel) {

    size_t computedImageRowPitch = rowPitch ? rowPitch : region[0] * bytesPerPixel;
    size_t computedImageSlicePitch = slicePitch ? slicePitch : region[1] * computedImageRowPitch * bytesPerPixel;
    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
    case CL_MEM_OBJECT_IMAGE2D:
        DEBUG_BREAK_IF(slicePitch != 0 && slicePitch < computedImageRowPitch * region[1]);
        CPP_ATTRIBUTE_FALLTHROUGH;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
    case CL_MEM_OBJECT_IMAGE3D:
        *imageOffset = origin[2] * computedImageSlicePitch + origin[1] * computedImageRowPitch + origin[0] * bytesPerPixel;
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        *imageOffset = origin[1] * computedImageSlicePitch + origin[0] * bytesPerPixel;
        break;
    default:
        DEBUG_BREAK_IF("Unsupported cl_image_type");
        *imageOffset = 0;
        break;
    }
}

// Called by clGetImageParamsINTEL to obtain image row pitch and slice pitch
// Assumption: all parameters are already validated be calling function
cl_int Image::getImageParams(Context *context,
                             cl_mem_flags memFlags,
                             const SurfaceFormatInfo *surfaceFormat,
                             const cl_image_desc *imageDesc,
                             size_t *imageRowPitch,
                             size_t *imageSlicePitch) {
    cl_int retVal = CL_SUCCESS;

    ImageInfo imgInfo = {0};
    cl_image_desc imageDescriptor = *imageDesc;
    imgInfo.imgDesc = &imageDescriptor;
    imgInfo.surfaceFormat = surfaceFormat;

    std::unique_ptr<Gmm> gmm(new Gmm(imgInfo)); // query imgInfo

    *imageRowPitch = imgInfo.rowPitch;
    *imageSlicePitch = imgInfo.slicePitch;

    return retVal;
}

const cl_image_desc &Image::getImageDesc() const {
    return imageDesc;
}

const cl_image_format &Image::getImageFormat() const {
    return imageFormat;
}

const SurfaceFormatInfo &Image::getSurfaceFormatInfo() const {
    return surfaceFormatInfo;
}

bool Image::isCopyRequired(ImageInfo &imgInfo, const void *hostPtr) {
    if (!hostPtr) {
        return false;
    }

    size_t imageWidth = imgInfo.imgDesc->image_width;
    size_t imageHeight = 1;
    size_t imageDepth = 1;
    size_t imageCount = 1;

    switch (imgInfo.imgDesc->image_type) {
    case CL_MEM_OBJECT_IMAGE3D:
        imageDepth = imgInfo.imgDesc->image_depth;
        CPP_ATTRIBUTE_FALLTHROUGH;
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        imageHeight = imgInfo.imgDesc->image_height;
        break;
    default:
        break;
    }

    auto hostPtrRowPitch = imgInfo.imgDesc->image_row_pitch ? imgInfo.imgDesc->image_row_pitch : imageWidth * imgInfo.surfaceFormat->ImageElementSizeInBytes;
    auto hostPtrSlicePitch = imgInfo.imgDesc->image_slice_pitch ? imgInfo.imgDesc->image_slice_pitch : hostPtrRowPitch * imgInfo.imgDesc->image_height;
    auto isTilingAllowed = GmmHelper::allowTiling(*imgInfo.imgDesc);

    size_t pointerPassedSize = hostPtrRowPitch * imageHeight * imageDepth * imageCount;
    auto alignedSizePassedPointer = alignSizeWholePage(const_cast<void *>(hostPtr), pointerPassedSize);
    auto alignedSizeRequiredForAllocation = alignSizeWholePage(const_cast<void *>(hostPtr), imgInfo.size);

    // Passed pointer doesn't have enough memory, copy is needed
    bool copyRequired = (alignedSizeRequiredForAllocation > alignedSizePassedPointer) |
                        (imgInfo.rowPitch != hostPtrRowPitch) |
                        (imgInfo.slicePitch != hostPtrSlicePitch) |
                        ((reinterpret_cast<uintptr_t>(hostPtr) & (MemoryConstants::cacheLineSize - 1)) != 0) |
                        isTilingAllowed;

    return copyRequired;
}

cl_int Image::getImageInfo(cl_image_info paramName,
                           size_t paramValueSize,
                           void *paramValue,
                           size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t srcParamSize = 0;
    void *srcParam = nullptr;
    auto imageDesc = getImageDesc();
    auto surfFmtInfo = getSurfaceFormatInfo();
    size_t retParam;
    size_t array_size = imageDesc.image_array_size * (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY);
    size_t SlicePitch = hostPtrSlicePitch * !(imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D || imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D || imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER);

    switch (paramName) {
    case CL_IMAGE_FORMAT:
        srcParamSize = sizeof(cl_image_format);
        srcParam = &(surfFmtInfo.OCLImageFormat);
        break;

    case CL_IMAGE_ELEMENT_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &(surfFmtInfo.ImageElementSizeInBytes);
        break;

    case CL_IMAGE_ROW_PITCH:
        srcParamSize = sizeof(size_t);
        if (mcsSurfaceInfo.multisampleCount > 1) {
            retParam = imageDesc.image_width * surfFmtInfo.ImageElementSizeInBytes * imageDesc.num_samples;
        } else {
            retParam = hostPtrRowPitch;
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_SLICE_PITCH:
        srcParamSize = sizeof(size_t);
        srcParam = &SlicePitch;
        break;

    case CL_IMAGE_WIDTH:
        srcParamSize = sizeof(size_t);
        retParam = imageDesc.image_width;
        if (this->baseMipLevel) {
            retParam = imageDesc.image_width >> this->baseMipLevel;
            retParam = std::max(retParam, (size_t)1);
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_HEIGHT:
        srcParamSize = sizeof(size_t);
        retParam = imageDesc.image_height * !((imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D) || (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) || (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER));
        if ((retParam != 0) && (this->baseMipLevel > 0)) {
            retParam = retParam >> this->baseMipLevel;
            retParam = std::max(retParam, (size_t)1);
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_DEPTH:
        srcParamSize = sizeof(size_t);
        retParam = imageDesc.image_depth * (imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D);
        if ((retParam != 0) && (this->baseMipLevel > 0)) {
            retParam = retParam >> this->baseMipLevel;
            retParam = std::max(retParam, (size_t)1);
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_ARRAY_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &(array_size);
        break;

    case CL_IMAGE_BUFFER:
        srcParamSize = sizeof(cl_mem);
        srcParam = &(imageDesc.buffer);
        break;

    case CL_IMAGE_NUM_MIP_LEVELS:
        srcParamSize = sizeof(cl_uint);
        srcParam = &(imageDesc.num_mip_levels);
        break;

    case CL_IMAGE_NUM_SAMPLES:
        srcParamSize = sizeof(cl_uint);
        srcParam = &(imageDesc.num_samples);
        break;

    default:
        getOsSpecificImageInfo(paramName, &srcParamSize, &srcParam);
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcParamSize;
    }

    return retVal;
}

Image *Image::redescribeFillImage() {
    const uint32_t redescribeTable[3][3] = {
        {17, 27, 5}, // {CL_R, CL_UNSIGNED_INT8},  {CL_RG, CL_UNSIGNED_INT8},  {CL_RGBA, CL_UNSIGNED_INT8}
        {18, 28, 6}, // {CL_R, CL_UNSIGNED_INT16}, {CL_RG, CL_UNSIGNED_INT16}, {CL_RGBA, CL_UNSIGNED_INT16}
        {19, 29, 7}  // {CL_R, CL_UNSIGNED_INT32}, {CL_RG, CL_UNSIGNED_INT32}, {CL_RGBA, CL_UNSIGNED_INT32}
    };

    auto imageFormatNew = this->imageFormat;
    auto imageDescNew = this->imageDesc;
    const SurfaceFormatInfo *surfaceFormat = nullptr;
    uint32_t redescribeTableCol = this->surfaceFormatInfo.NumChannels / 2;
    uint32_t redescribeTableRow = this->surfaceFormatInfo.PerChannelSizeInBytes / 2;

    ArrayRef<const SurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();

    uint32_t surfaceFormatIdx = redescribeTable[redescribeTableRow][redescribeTableCol];
    surfaceFormat = &readWriteSurfaceFormats[surfaceFormatIdx];

    imageFormatNew.image_channel_order = surfaceFormat->OCLImageFormat.image_channel_order;
    imageFormatNew.image_channel_data_type = surfaceFormat->OCLImageFormat.image_channel_data_type;

    DEBUG_BREAK_IF(nullptr == createFunction);
    auto image = createFunction(context,
                                properties.flags | CL_MEM_USE_HOST_PTR,
                                this->getSize(),
                                this->getCpuAddress(),
                                imageFormatNew,
                                imageDescNew,
                                this->isMemObjZeroCopy(),
                                this->getGraphicsAllocation(),
                                true,
                                isTiledImage,
                                this->baseMipLevel,
                                this->mipCount,
                                surfaceFormat,
                                &this->surfaceOffsets);
    image->setQPitch(this->getQPitch());
    image->setCubeFaceIndex(this->getCubeFaceIndex());
    image->associatedMemObject = this->associatedMemObject;
    return image;
}

Image *Image::redescribe() {

    const uint32_t redescribeTableBytes[] = {
        17, // {CL_R, CL_UNSIGNED_INT8}        1 byte
        18, // {CL_R, CL_UNSIGNED_INT16}       2 byte
        19, // {CL_R, CL_UNSIGNED_INT32}       4 byte
        29, // {CL_RG, CL_UNSIGNED_INT32}      8 byte
        7   // {CL_RGBA, CL_UNSIGNED_INT32}    16 byte
    };

    auto imageFormatNew = this->imageFormat;
    auto imageDescNew = this->imageDesc;
    const SurfaceFormatInfo *surfaceFormat = nullptr;
    auto bytesPerPixel = this->surfaceFormatInfo.NumChannels * surfaceFormatInfo.PerChannelSizeInBytes;
    uint32_t exponent = 0;

    exponent = Math::log2(bytesPerPixel);
    DEBUG_BREAK_IF(exponent >= 32);

    uint32_t surfaceFormatIdx = redescribeTableBytes[exponent % 5];
    ArrayRef<const SurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();

    surfaceFormat = &readWriteSurfaceFormats[surfaceFormatIdx];

    imageFormatNew.image_channel_order = surfaceFormat->OCLImageFormat.image_channel_order;
    imageFormatNew.image_channel_data_type = surfaceFormat->OCLImageFormat.image_channel_data_type;

    DEBUG_BREAK_IF(nullptr == createFunction);
    auto image = createFunction(context,
                                properties.flags | CL_MEM_USE_HOST_PTR,
                                this->getSize(),
                                this->getCpuAddress(),
                                imageFormatNew,
                                imageDescNew,
                                this->isMemObjZeroCopy(),
                                this->getGraphicsAllocation(),
                                true,
                                isTiledImage,
                                this->baseMipLevel,
                                this->mipCount,
                                surfaceFormat,
                                &this->surfaceOffsets);
    image->setQPitch(this->getQPitch());
    image->setCubeFaceIndex(this->getCubeFaceIndex());
    image->associatedMemObject = this->associatedMemObject;
    return image;
}

void Image::transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(hostPtr, hostPtrRowPitch, hostPtrSlicePitch,
                 graphicsAllocation->getUnderlyingBuffer(), imageDesc.image_row_pitch, imageDesc.image_slice_pitch,
                 copySize, copyOffset);
}

void Image::transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(memoryStorage, imageDesc.image_row_pitch, imageDesc.image_slice_pitch,
                 hostPtr, hostPtrRowPitch, hostPtrSlicePitch,
                 copySize, copyOffset);
}

cl_int Image::writeNV12Planes(const void *hostPtr, size_t hostPtrRowPitch) {
    CommandQueue *cmdQ = context->getSpecialQueue();
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {this->imageDesc.image_width, this->imageDesc.image_height, 1};

    cl_int retVal = 0;
    cl_image_desc imageDesc = {0};
    cl_image_format imageFormat = {0};
    // Make NV12 planes readable and writable both on device and host
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    // Plane Y
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    // image_width & image_height are ignored for plane extraction
    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    // set mem_object to the full NV12 image
    imageDesc.mem_object = this;
    // get access to the Y plane (CL_R)
    imageDesc.image_depth = 0;
    SurfaceFormatInfo *surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageYPlane(Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal));

    retVal = cmdQ->enqueueWriteImage(imageYPlane.get(), CL_TRUE, origin, region, hostPtrRowPitch, 0, hostPtr, nullptr, 0, nullptr, nullptr);

    // UV Plane is two times smaller than Plane Y
    region[0] = region[0] / 2;
    region[1] = region[1] / 2;

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 1; // UV plane
    imageFormat.image_channel_order = CL_RG;

    hostPtr = static_cast<const void *>(static_cast<const char *>(hostPtr) + (hostPtrRowPitch * this->imageDesc.image_height));
    surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, &imageFormat);
    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane(Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal));

    retVal = cmdQ->enqueueWriteImage(imageUVPlane.get(), CL_TRUE, origin, region, hostPtrRowPitch, 0, hostPtr, nullptr, 0, nullptr, nullptr);

    return retVal;
}

const SurfaceFormatInfo *Image::getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat) {
    if (!imageFormat) {
        DEBUG_BREAK_IF("Invalid format");
        return nullptr;
    }

    ArrayRef<const SurfaceFormatInfo> formats = SurfaceFormats::surfaceFormats(flags, imageFormat);

    for (auto &format : formats) {
        if (format.OCLImageFormat.image_channel_data_type == imageFormat->image_channel_data_type &&
            format.OCLImageFormat.image_channel_order == imageFormat->image_channel_order) {
            return &format;
        }
    }
    DEBUG_BREAK_IF("Invalid format");
    return nullptr;
}

bool Image::isImage2d(cl_mem_object_type imageType) {
    return imageType == CL_MEM_OBJECT_IMAGE2D;
}

bool Image::isImage2dOr2dArray(cl_mem_object_type imageType) {
    return imageType == CL_MEM_OBJECT_IMAGE2D || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY;
}

bool Image::isDepthFormat(const cl_image_format &imageFormat) {
    return imageFormat.image_channel_order == CL_DEPTH || imageFormat.image_channel_order == CL_DEPTH_STENCIL;
}

Image *Image::validateAndCreateImage(Context *context,
                                     cl_mem_flags flags,
                                     const cl_image_format *imageFormat,
                                     const cl_image_desc *imageDesc,
                                     const void *hostPtr,
                                     cl_int &errcodeRet) {
    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }
    SurfaceFormatInfo *surfaceFormat = nullptr;
    Image *image = nullptr;
    do {
        errcodeRet = Image::validateImageFormat(imageFormat);
        if (CL_SUCCESS != errcodeRet) {
            break;
        }
        surfaceFormat = (SurfaceFormatInfo *)Image::getSurfaceFormatFromTable(flags, imageFormat);
        errcodeRet = Image::validate(context, flags, surfaceFormat, imageDesc, hostPtr);
        if (CL_SUCCESS != errcodeRet) {
            break;
        }
        image = Image::create(context, flags, surfaceFormat, imageDesc, hostPtr, errcodeRet);
    } while (false);
    return image;
}

bool Image::isValidSingleChannelFormat(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    auto dataType = imageFormat->image_channel_data_type;

    bool isValidOrder = (channelOrder == CL_A) ||
                        (channelOrder == CL_R) ||
                        (channelOrder == CL_Rx);

    bool isValidDataType = (dataType == CL_UNORM_INT8) ||
                           (dataType == CL_UNORM_INT16) ||
                           (dataType == CL_SNORM_INT8) ||
                           (dataType == CL_SNORM_INT16) ||
                           (dataType == CL_HALF_FLOAT) ||
                           (dataType == CL_FLOAT) ||
                           (dataType == CL_SIGNED_INT8) ||
                           (dataType == CL_SIGNED_INT16) ||
                           (dataType == CL_SIGNED_INT32) ||
                           (dataType == CL_UNSIGNED_INT8) ||
                           (dataType == CL_UNSIGNED_INT16) ||
                           (dataType == CL_UNSIGNED_INT32);

    return isValidOrder && isValidDataType;
}

bool Image::isValidIntensityFormat(const cl_image_format *imageFormat) {
    if (imageFormat->image_channel_order != CL_INTENSITY) {
        return false;
    }
    auto dataType = imageFormat->image_channel_data_type;
    return (dataType == CL_UNORM_INT8) ||
           (dataType == CL_UNORM_INT16) ||
           (dataType == CL_SNORM_INT8) ||
           (dataType == CL_SNORM_INT16) ||
           (dataType == CL_HALF_FLOAT) ||
           (dataType == CL_FLOAT);
}

bool Image::isValidLuminanceFormat(const cl_image_format *imageFormat) {
    if (imageFormat->image_channel_order != CL_LUMINANCE) {
        return false;
    }
    auto dataType = imageFormat->image_channel_data_type;
    return (dataType == CL_UNORM_INT8) ||
           (dataType == CL_UNORM_INT16) ||
           (dataType == CL_SNORM_INT8) ||
           (dataType == CL_SNORM_INT16) ||
           (dataType == CL_HALF_FLOAT) ||
           (dataType == CL_FLOAT);
}

bool Image::isValidDepthFormat(const cl_image_format *imageFormat) {
    if (imageFormat->image_channel_order != CL_DEPTH) {
        return false;
    }
    auto dataType = imageFormat->image_channel_data_type;
    return (dataType == CL_UNORM_INT16) ||
           (dataType == CL_FLOAT);
}

bool Image::isValidDoubleChannelFormat(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    auto dataType = imageFormat->image_channel_data_type;

    bool isValidOrder = (channelOrder == CL_RG) ||
                        (channelOrder == CL_RGx) ||
                        (channelOrder == CL_RA);

    bool isValidDataType = (dataType == CL_UNORM_INT8) ||
                           (dataType == CL_UNORM_INT16) ||
                           (dataType == CL_SNORM_INT8) ||
                           (dataType == CL_SNORM_INT16) ||
                           (dataType == CL_HALF_FLOAT) ||
                           (dataType == CL_FLOAT) ||
                           (dataType == CL_SIGNED_INT8) ||
                           (dataType == CL_SIGNED_INT16) ||
                           (dataType == CL_SIGNED_INT32) ||
                           (dataType == CL_UNSIGNED_INT8) ||
                           (dataType == CL_UNSIGNED_INT16) ||
                           (dataType == CL_UNSIGNED_INT32);

    return isValidOrder && isValidDataType;
}

bool Image::isValidTripleChannelFormat(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    auto dataType = imageFormat->image_channel_data_type;

    bool isValidOrder = (channelOrder == CL_RGB) ||
                        (channelOrder == CL_RGBx);

    bool isValidDataType = (dataType == CL_UNORM_SHORT_565) ||
                           (dataType == CL_UNORM_SHORT_555) ||
                           (dataType == CL_UNORM_INT_101010);

    return isValidOrder && isValidDataType;
}

bool Image::isValidRGBAFormat(const cl_image_format *imageFormat) {
    if (imageFormat->image_channel_order != CL_RGBA) {
        return false;
    }
    auto dataType = imageFormat->image_channel_data_type;
    return (dataType == CL_UNORM_INT8) ||
           (dataType == CL_UNORM_INT16) ||
           (dataType == CL_SNORM_INT8) ||
           (dataType == CL_SNORM_INT16) ||
           (dataType == CL_HALF_FLOAT) ||
           (dataType == CL_FLOAT) ||
           (dataType == CL_SIGNED_INT8) ||
           (dataType == CL_SIGNED_INT16) ||
           (dataType == CL_SIGNED_INT32) ||
           (dataType == CL_UNSIGNED_INT8) ||
           (dataType == CL_UNSIGNED_INT16) ||
           (dataType == CL_UNSIGNED_INT32);
}

bool Image::isValidSRGBFormat(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    auto dataType = imageFormat->image_channel_data_type;

    bool isValidOrder = (channelOrder == CL_sRGB) ||
                        (channelOrder == CL_sRGBx) ||
                        (channelOrder == CL_sRGBA) ||
                        (channelOrder == CL_sBGRA);

    bool isValidDataType = (dataType == CL_UNORM_INT8);

    return isValidOrder && isValidDataType;
}

bool Image::isValidARGBFormat(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    auto dataType = imageFormat->image_channel_data_type;

    bool isValidOrder = (channelOrder == CL_ARGB) ||
                        (channelOrder == CL_BGRA) ||
                        (channelOrder == CL_ABGR);

    bool isValidDataType = (dataType == CL_UNORM_INT8) ||
                           (dataType == CL_SNORM_INT8) ||
                           (dataType == CL_SIGNED_INT8) ||
                           (dataType == CL_UNSIGNED_INT8);

    return isValidOrder && isValidDataType;
}

bool Image::isValidDepthStencilFormat(const cl_image_format *imageFormat) {
    if (imageFormat->image_channel_order != CL_DEPTH_STENCIL) {
        return false;
    }
    auto dataType = imageFormat->image_channel_data_type;
    return (dataType == CL_UNORM_INT24) ||
           (dataType == CL_FLOAT);
}

bool Image::isValidYUVFormat(const cl_image_format *imageFormat) {
    auto dataType = imageFormat->image_channel_data_type;

    bool isValidOrder = IsNV12Image(imageFormat) || IsPackedYuvImage(imageFormat);

    bool isValidDataType = (dataType == CL_UNORM_INT8);

    return isValidOrder && isValidDataType;
}

bool Image::hasAlphaChannel(const cl_image_format *imageFormat) {
    auto channelOrder = imageFormat->image_channel_order;
    return (channelOrder == CL_A) ||
           (channelOrder == CL_Rx) ||
           (channelOrder == CL_RA) ||
           (channelOrder == CL_RGx) ||
           (channelOrder == CL_RGBx) ||
           (channelOrder == CL_RGBA) ||
           (channelOrder == CL_BGRA) ||
           (channelOrder == CL_ARGB) ||
           (channelOrder == CL_INTENSITY) ||
           (channelOrder == CL_sRGBA) ||
           (channelOrder == CL_sBGRA) ||
           (channelOrder == CL_sRGBx) ||
           (channelOrder == CL_ABGR);
}

size_t Image::calculateOffsetForMapping(const MemObjOffsetArray &origin) const {
    size_t rowPitch = mappingOnCpuAllowed() ? imageDesc.image_row_pitch : getHostPtrRowPitch();
    size_t slicePitch = mappingOnCpuAllowed() ? imageDesc.image_slice_pitch : getHostPtrSlicePitch();

    size_t offset = getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0];

    switch (imageDesc.image_type) {
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        offset += slicePitch * origin[1];
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        offset += rowPitch * origin[1];
        break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
    case CL_MEM_OBJECT_IMAGE3D:
        offset += rowPitch * origin[1] + slicePitch * origin[2];
        break;
    default:
        break;
    }

    return offset;
}

cl_int Image::validateRegionAndOrigin(const size_t *origin, const size_t *region, const cl_image_desc &imgDesc) {
    if (region[0] == 0 || region[1] == 0 || region[2] == 0) {
        return CL_INVALID_VALUE;
    }

    bool notMipMapped = (false == isMipMapped(imgDesc));

    if ((imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D || imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) &&
        (((origin[1] > 0) && notMipMapped) || origin[2] > 0 || region[1] > 1 || region[2] > 1)) {
        return CL_INVALID_VALUE;
    }

    if ((imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D || imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) &&
        (((origin[2] > 0) && notMipMapped) || region[2] > 1)) {
        return CL_INVALID_VALUE;
    }

    if (notMipMapped) {
        return CL_SUCCESS;
    }

    uint32_t mipLevel = findMipLevel(imgDesc.image_type, origin);
    if (mipLevel < imgDesc.num_mip_levels) {
        return CL_SUCCESS;
    } else {
        return CL_INVALID_MIP_LEVEL;
    }
}

bool Image::hasSameDescriptor(const cl_image_desc &imageDesc) const {
    return this->imageDesc.image_type == imageDesc.image_type &&
           this->imageDesc.image_width == imageDesc.image_width &&
           this->imageDesc.image_height == imageDesc.image_height &&
           this->imageDesc.image_depth == imageDesc.image_depth &&
           this->imageDesc.image_array_size == imageDesc.image_array_size &&
           this->hostPtrRowPitch == imageDesc.image_row_pitch &&
           this->hostPtrSlicePitch == imageDesc.image_slice_pitch &&
           this->imageDesc.num_mip_levels == imageDesc.num_mip_levels &&
           this->imageDesc.num_samples == imageDesc.num_samples;
}

bool Image::hasValidParentImageFormat(const cl_image_format &imageFormat) const {
    if (this->imageFormat.image_channel_data_type != imageFormat.image_channel_data_type) {
        return false;
    }
    switch (this->imageFormat.image_channel_order) {
    case CL_BGRA:
        return imageFormat.image_channel_order == CL_sBGRA;
    case CL_sBGRA:
        return imageFormat.image_channel_order == CL_BGRA;
    case CL_RGBA:
        return imageFormat.image_channel_order == CL_sRGBA;
    case CL_sRGBA:
        return imageFormat.image_channel_order == CL_RGBA;
    case CL_RGB:
        return imageFormat.image_channel_order == CL_sRGB;
    case CL_sRGB:
        return imageFormat.image_channel_order == CL_RGB;
    case CL_RGBx:
        return imageFormat.image_channel_order == CL_sRGBx;
    case CL_sRGBx:
        return imageFormat.image_channel_order == CL_RGBx;
    case CL_R:
        return imageFormat.image_channel_order == CL_DEPTH;
    default:
        return false;
    }
}
} // namespace NEO
