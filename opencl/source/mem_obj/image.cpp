/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/image.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/compiler_support.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/device/cl_device.h"
#include "opencl/source/device/cl_device_get_cap.inl"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/helpers/memory_properties_flags_helpers.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"

#include "igfxfmid.h"

#include <map>

namespace NEO {

ImageFuncs imageFactory[IGFX_MAX_CORE] = {};

Image::Image(Context *context,
             const MemoryPropertiesFlags &memoryProperties,
             cl_mem_flags flags,
             cl_mem_flags_intel flagsIntel,
             size_t size,
             void *hostPtr,
             cl_image_format imageFormat,
             const cl_image_desc &imageDesc,
             bool zeroCopy,
             GraphicsAllocation *graphicsAllocation,
             bool isObjectRedescribed,
             uint32_t baseMipLevel,
             uint32_t mipCount,
             const ClSurfaceFormatInfo &surfaceFormatInfo,
             const SurfaceOffsets *surfaceOffsets)
    : MemObj(context,
             imageDesc.image_type,
             memoryProperties,
             flags,
             flagsIntel,
             size,
             graphicsAllocation->getUnderlyingBuffer(),
             hostPtr,
             graphicsAllocation,
             zeroCopy,
             false,
             isObjectRedescribed),
      createFunction(nullptr),
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

    size_t pixelSize = surfaceFormatInfo.surfaceFormat.ImageElementSizeInBytes;
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
                     const MemoryPropertiesFlags &memoryProperties,
                     cl_mem_flags flags,
                     cl_mem_flags_intel flagsIntel,
                     const ClSurfaceFormatInfo *surfaceFormat,
                     const cl_image_desc *imageDesc,
                     const void *hostPtr,
                     cl_int &errcodeRet) {
    UNRECOVERABLE_IF(surfaceFormat == nullptr);
    Image *image = nullptr;
    GraphicsAllocation *memory = nullptr;
    GraphicsAllocation *mapAllocation = nullptr;
    MemoryManager *memoryManager = context->getMemoryManager();
    Buffer *parentBuffer = castToObject<Buffer>(imageDesc->mem_object);
    Image *parentImage = castToObject<Image>(imageDesc->mem_object);
    auto &hwHelper = HwHelper::get(context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
    auto clientContext = context->getDevice(0)->getRootDeviceEnvironment().getGmmClientContext();

    do {
        size_t imageWidth = imageDesc->image_width;
        size_t imageHeight = 1;
        size_t imageDepth = 1;
        size_t imageCount = 1;
        size_t hostPtrMinSize = 0;

        cl_image_desc imageDescriptor = *imageDesc;
        ImageInfo imgInfo = {};
        void *hostPtrToSet = nullptr;

        if (memoryProperties.flags.useHostPtr) {
            hostPtrToSet = const_cast<void *>(hostPtr);
        }

        imgInfo.imgDesc = Image::convertDescriptor(imageDescriptor);
        imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
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

            imgInfo.surfaceFormat = &parentImage->surfaceFormatInfo.surfaceFormat;
            imageDescriptor = parentImage->getImageDesc();
        }

        auto hostPtrRowPitch = imageDesc->image_row_pitch ? imageDesc->image_row_pitch : imageWidth * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
        auto hostPtrSlicePitch = imageDesc->image_slice_pitch ? imageDesc->image_slice_pitch : hostPtrRowPitch * imageHeight;
        imgInfo.linearStorage = !hwHelper.tilingAllowed(context->isSharedContext, Image::isImage1d(*imageDesc),
                                                        memoryProperties.flags.forceLinearStorage);
        imgInfo.preferRenderCompression = MemObjHelper::isSuitableForRenderCompression(!imgInfo.linearStorage, memoryProperties,
                                                                                       *context, true);
        imgInfo.preferRenderCompression &= !Image::isFormatRedescribable(surfaceFormat->OCLImageFormat);

        if (!context->getDevice(0)->getSharedDeviceInfo().imageSupport && !imgInfo.linearStorage) {
            errcodeRet = CL_INVALID_OPERATION;
            return nullptr;
        }

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

        bool zeroCopy = false;
        bool transferNeeded = false;
        if (((imageDesc->image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) || (imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D)) && (parentBuffer != nullptr)) {

            if (!hwHelper.checkResourceCompatibility(*parentBuffer->getGraphicsAllocation())) {
                errcodeRet = CL_INVALID_MEM_OBJECT;
                return nullptr;
            }

            memory = parentBuffer->getGraphicsAllocation();

            // Image from buffer - we never allocate memory, we use what buffer provides
            zeroCopy = true;
            hostPtr = parentBuffer->getHostPtr();
            hostPtrToSet = const_cast<void *>(hostPtr);
            parentBuffer->incRefInternal();
            GmmTypesConverter::queryImgFromBufferParams(imgInfo, memory);

            UNRECOVERABLE_IF(imgInfo.offset != 0);
            imgInfo.offset = parentBuffer->getOffset();

            if (memoryManager->peekVirtualPaddingSupport() && (imageDesc->image_type == CL_MEM_OBJECT_IMAGE2D)) {
                // Retrieve sizes from GMM and apply virtual padding if buffer storage is not big enough
                auto queryGmmImgInfo(imgInfo);
                auto gmm = std::make_unique<Gmm>(clientContext, queryGmmImgInfo, StorageInfo{});
                auto gmmAllocationSize = gmm->gmmResourceInfo->getSizeAllocation();
                if (gmmAllocationSize > memory->getUnderlyingBufferSize()) {
                    memory = memoryManager->createGraphicsAllocationWithPadding(memory, gmmAllocationSize);
                }
            }
        } else if (parentImage != nullptr) {
            memory = parentImage->getGraphicsAllocation();
            memory->getDefaultGmm()->queryImageParams(imgInfo);
        } else {
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            if (memoryProperties.flags.useHostPtr) {

                if (!context->isSharedContext) {
                    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(rootDeviceIndex, imgInfo, false, memoryProperties, context->getDevice(0)->getHardwareInfo());

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
                    gmm = new Gmm(clientContext, imgInfo, StorageInfo{});
                    memory = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, false, imgInfo.size, GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE, false}, hostPtr);
                    memory->setDefaultGmm(gmm);
                    zeroCopy = true;
                }
                if (memory) {
                    AllocationProperties properties{rootDeviceIndex, false, hostPtrMinSize, GraphicsAllocation::AllocationType::MAP_ALLOCATION, false};
                    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = true;
                    mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
                }
            } else {
                AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(rootDeviceIndex, imgInfo, true, memoryProperties, context->getDevice(0)->getHardwareInfo());
                memory = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);

                if (memory && MemoryPool::isSystemMemoryPool(memory->getMemoryPool())) {
                    zeroCopy = true;
                }
            }
        }
        transferNeeded |= memoryProperties.flags.copyHostPtr;

        if (!memory) {
            break;
        }

        if (parentBuffer == nullptr) {
            memory->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
        }

        memory->setMemObjectsAllocationWithWritableFlags(!memoryProperties.flags.readOnly &&
                                                         !memoryProperties.flags.hostReadOnly &&
                                                         !memoryProperties.flags.hostNoAccess);

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
            imgInfo.imgDesc = Image::convertDescriptor(imageDescriptor);
        }

        image = createImageHw(context, memoryProperties, flags, flagsIntel, imgInfo.size, hostPtrToSet, surfaceFormat->OCLImageFormat,
                              imageDescriptor, zeroCopy, memory, false, 0, 0, surfaceFormat);

        if (context->isProvidingPerformanceHints() && HwHelper::renderCompressedImagesSupported(context->getDevice(0)->getHardwareInfo())) {
            if (memory->getDefaultGmm()) {
                if (memory->getDefaultGmm()->isRenderCompressed) {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, IMAGE_IS_COMPRESSED, image);
                } else {
                    context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, IMAGE_IS_NOT_COMPRESSED, image);
                }
            }
        }

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

            if (!imgInfo.linearStorage || !MemoryPool::isSystemMemoryPool(memory->getMemoryPool())) {
                auto cmdQ = context->getSpecialQueue();

                if (IsNV12Image(&image->getImageFormat())) {
                    errcodeRet = image->writeNV12Planes(hostPtr, hostPtrRowPitch);
                } else {
                    errcodeRet = cmdQ->enqueueWriteImage(image, CL_TRUE, &copyOrigin[0], &copyRegion[0],
                                                         hostPtrRowPitch, hostPtrSlicePitch,
                                                         hostPtr, mapAllocation, 0, nullptr, nullptr);
                }
            } else {
                image->transferData(memory->getUnderlyingBuffer(), imgInfo.rowPitch, imgInfo.slicePitch,
                                    const_cast<void *>(hostPtr), hostPtrRowPitch, hostPtrSlicePitch,
                                    copyRegion, copyOrigin);
            }
        }

        image->mapAllocation = mapAllocation;

        if (errcodeRet != CL_SUCCESS) {
            image->release();
            image = nullptr;
            memory = nullptr;
            break;
        }
    } while (false);

    return image;
}

Image *Image::createImageHw(Context *context, const MemoryPropertiesFlags &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, size_t size, void *hostPtr,
                            const cl_image_format &imageFormat, const cl_image_desc &imageDesc,
                            bool zeroCopy, GraphicsAllocation *graphicsAllocation,
                            bool isObjectRedescribed, uint32_t baseMipLevel, uint32_t mipCount,
                            const ClSurfaceFormatInfo *surfaceFormatInfo) {
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = imageFactory[hwInfo.platform.eRenderCoreFamily].createImageFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);
    auto image = funcCreate(context, memoryProperties, flags, flagsIntel, size, hostPtr, imageFormat, imageDesc,
                            zeroCopy, graphicsAllocation, isObjectRedescribed, baseMipLevel, mipCount, surfaceFormatInfo, nullptr);
    DEBUG_BREAK_IF(nullptr == image);
    image->createFunction = funcCreate;
    return image;
}

Image *Image::createSharedImage(Context *context, SharingHandler *sharingHandler, const McsSurfaceInfo &mcsSurfaceInfo,
                                GraphicsAllocation *graphicsAllocation, GraphicsAllocation *mcsAllocation,
                                cl_mem_flags flags, const ClSurfaceFormatInfo *surfaceFormat, ImageInfo &imgInfo, uint32_t cubeFaceIndex, uint32_t baseMipLevel, uint32_t mipCount) {
    auto sharedImage = createImageHw(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, graphicsAllocation->getUnderlyingBufferSize(),
                                     nullptr, surfaceFormat->OCLImageFormat, Image::convertDescriptor(imgInfo.imgDesc), false, graphicsAllocation, false, baseMipLevel, mipCount, surfaceFormat);
    sharedImage->setSharingHandler(sharingHandler);
    sharedImage->setMcsAllocation(mcsAllocation);
    sharedImage->setQPitch(imgInfo.qPitch);
    sharedImage->setHostPtrRowPitch(imgInfo.imgDesc.imageRowPitch);
    sharedImage->setHostPtrSlicePitch(imgInfo.imgDesc.imageSlicePitch);
    sharedImage->setCubeFaceIndex(cubeFaceIndex);
    sharedImage->setSurfaceOffsets(imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane);
    sharedImage->setMcsSurfaceInfo(mcsSurfaceInfo);
    return sharedImage;
}

cl_int Image::validate(Context *context,
                       const MemoryPropertiesFlags &memoryProperties,
                       const ClSurfaceFormatInfo *surfaceFormat,
                       const cl_image_desc *imageDesc,
                       const void *hostPtr) {
    auto pClDevice = context->getDevice(0);
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
        pClDevice->getCap<CL_DEVICE_IMAGE2D_MAX_WIDTH>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);
        pClDevice->getCap<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);
        if (imageDesc->image_width > *maxWidth ||
            imageDesc->image_height > *maxHeight) {
            return CL_INVALID_IMAGE_SIZE;
        }
        if (parentBuffer) { // Image 2d from buffer
            pClDevice->getCap<CL_DEVICE_IMAGE_PITCH_ALIGNMENT>(reinterpret_cast<const void *&>(pitchAlignment), srcSize, retSize);
            pClDevice->getCap<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT>(reinterpret_cast<const void *&>(baseAddressAlignment), srcSize, retSize);

            const auto rowSize = imageDesc->image_row_pitch != 0 ? imageDesc->image_row_pitch : alignUp(imageDesc->image_width * surfaceFormat->surfaceFormat.NumChannels * surfaceFormat->surfaceFormat.PerChannelSizeInBytes, *pitchAlignment);
            const auto minimumBufferSize = imageDesc->image_height * rowSize;

            if ((imageDesc->image_row_pitch % (*pitchAlignment)) ||
                ((parentBuffer->getMemoryPropertiesFlags() & CL_MEM_USE_HOST_PTR) && (reinterpret_cast<uint64_t>(parentBuffer->getHostPtr()) % (*baseAddressAlignment))) ||
                (minimumBufferSize > parentBuffer->getSize())) {
                return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
            } else if (memoryProperties.flags.useHostPtr || memoryProperties.flags.copyHostPtr) {
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
            if (imageDesc->image_row_pitch % surfaceFormat->surfaceFormat.ImageElementSizeInBytes != 0 ||
                imageDesc->image_row_pitch < imageDesc->image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes) {
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

    return validateImageTraits(context, memoryProperties, &surfaceFormat->OCLImageFormat, imageDesc, hostPtr);
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
                         isValidDepthStencilFormat(imageFormat) ||
                         isValidYUVFormat(imageFormat);

    if (isValidFormat) {
        return CL_SUCCESS;
    }
    return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
}

cl_int Image::validatePlanarYUV(Context *context,
                                const MemoryPropertiesFlags &memoryProperties,
                                const cl_image_desc *imageDesc,
                                const void *hostPtr) {
    cl_int errorCode = CL_SUCCESS;
    auto pClDevice = context->getDevice(0);
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
        if (!memoryProperties.flags.hostNoAccess) {
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

        pClDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_WIDTH_INTEL>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);
        pClDevice->getCap<CL_DEVICE_PLANAR_YUV_MAX_HEIGHT_INTEL>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);
        if (imageDesc->image_width > *maxWidth || imageDesc->image_height > *maxHeight) {
            errorCode = CL_INVALID_IMAGE_SIZE;
            break;
        }
        break;
    }
    return errorCode;
}

cl_int Image::validatePackedYUV(const MemoryPropertiesFlags &memoryProperties, const cl_image_desc *imageDesc) {
    cl_int errorCode = CL_SUCCESS;
    while (true) {
        if (!memoryProperties.flags.readOnly) {
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

cl_int Image::validateImageTraits(Context *context, const MemoryPropertiesFlags &memoryProperties, const cl_image_format *imageFormat, const cl_image_desc *imageDesc, const void *hostPtr) {
    if (IsNV12Image(imageFormat))
        return validatePlanarYUV(context, memoryProperties, imageDesc, hostPtr);
    else if (IsPackedYuvImage(imageFormat))
        return validatePackedYUV(memoryProperties, imageDesc);

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
                             const ClSurfaceFormatInfo *surfaceFormat,
                             const cl_image_desc *imageDesc,
                             size_t *imageRowPitch,
                             size_t *imageSlicePitch) {
    cl_int retVal = CL_SUCCESS;
    auto clientContext = context->getDevice(0)->getRootDeviceEnvironment().getGmmClientContext();

    ImageInfo imgInfo = {};
    cl_image_desc imageDescriptor = *imageDesc;
    imgInfo.imgDesc = Image::convertDescriptor(imageDescriptor);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;

    auto gmm = std::make_unique<Gmm>(clientContext, imgInfo, StorageInfo{});

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

const ClSurfaceFormatInfo &Image::getSurfaceFormatInfo() const {
    return surfaceFormatInfo;
}

cl_mem_object_type Image::convertType(const ImageType type) {
    switch (type) {
    case ImageType::Image2D:
        return CL_MEM_OBJECT_IMAGE2D;
    case ImageType::Image3D:
        return CL_MEM_OBJECT_IMAGE3D;
    case ImageType::Image2DArray:
        return CL_MEM_OBJECT_IMAGE2D_ARRAY;
    case ImageType::Image1D:
        return CL_MEM_OBJECT_IMAGE1D;
    case ImageType::Image1DArray:
        return CL_MEM_OBJECT_IMAGE1D_ARRAY;
    case ImageType::Image1DBuffer:
        return CL_MEM_OBJECT_IMAGE1D_BUFFER;
    default:
        break;
    }
    return 0;
}

ImageType Image::convertType(const cl_mem_object_type type) {
    switch (type) {
    case CL_MEM_OBJECT_IMAGE2D:
        return ImageType::Image2D;
    case CL_MEM_OBJECT_IMAGE3D:
        return ImageType::Image3D;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return ImageType::Image2DArray;
    case CL_MEM_OBJECT_IMAGE1D:
        return ImageType::Image1D;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        return ImageType::Image1DArray;
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return ImageType::Image1DBuffer;
    default:
        break;
    }
    return ImageType::Invalid;
}

ImageDescriptor Image::convertDescriptor(const cl_image_desc &imageDesc) {
    ImageDescriptor desc = {};
    desc.fromParent = imageDesc.mem_object != nullptr;
    desc.imageArraySize = imageDesc.image_array_size;
    desc.imageDepth = imageDesc.image_depth;
    desc.imageHeight = imageDesc.image_height;
    desc.imageRowPitch = imageDesc.image_row_pitch;
    desc.imageSlicePitch = imageDesc.image_slice_pitch;
    desc.imageType = convertType(imageDesc.image_type);
    desc.imageWidth = imageDesc.image_width;
    desc.numMipLevels = imageDesc.num_mip_levels;
    desc.numSamples = imageDesc.num_samples;
    return desc;
}

cl_image_desc Image::convertDescriptor(const ImageDescriptor &imageDesc) {
    cl_image_desc desc = {};
    desc.mem_object = nullptr;
    desc.image_array_size = imageDesc.imageArraySize;
    desc.image_depth = imageDesc.imageDepth;
    desc.image_height = imageDesc.imageHeight;
    desc.image_row_pitch = imageDesc.imageRowPitch;
    desc.image_slice_pitch = imageDesc.imageSlicePitch;
    desc.image_type = convertType(imageDesc.imageType);
    desc.image_width = imageDesc.imageWidth;
    desc.num_mip_levels = imageDesc.numMipLevels;
    desc.num_samples = imageDesc.numSamples;
    return desc;
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
        srcParam = &(surfFmtInfo.surfaceFormat.ImageElementSizeInBytes);
        break;

    case CL_IMAGE_ROW_PITCH:
        srcParamSize = sizeof(size_t);
        if (mcsSurfaceInfo.multisampleCount > 1) {
            retParam = imageDesc.image_width * surfFmtInfo.surfaceFormat.ImageElementSizeInBytes * imageDesc.num_samples;
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

    retVal = changeGetInfoStatusToCLResultType(::getInfo(paramValue, paramValueSize, srcParam, srcParamSize));

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
    const ClSurfaceFormatInfo *surfaceFormat = nullptr;
    uint32_t redescribeTableCol = this->surfaceFormatInfo.surfaceFormat.NumChannels / 2;
    uint32_t redescribeTableRow = this->surfaceFormatInfo.surfaceFormat.PerChannelSizeInBytes / 2;

    ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();

    uint32_t surfaceFormatIdx = redescribeTable[redescribeTableRow][redescribeTableCol];
    surfaceFormat = &readWriteSurfaceFormats[surfaceFormatIdx];

    imageFormatNew.image_channel_order = surfaceFormat->OCLImageFormat.image_channel_order;
    imageFormatNew.image_channel_data_type = surfaceFormat->OCLImageFormat.image_channel_data_type;

    DEBUG_BREAK_IF(nullptr == createFunction);
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags | CL_MEM_USE_HOST_PTR, flagsIntel, 0);
    auto image = createFunction(context,
                                memoryProperties,
                                flags | CL_MEM_USE_HOST_PTR,
                                flagsIntel,
                                this->getSize(),
                                this->getCpuAddress(),
                                imageFormatNew,
                                imageDescNew,
                                this->isMemObjZeroCopy(),
                                this->getGraphicsAllocation(),
                                true,
                                this->baseMipLevel,
                                this->mipCount,
                                surfaceFormat,
                                &this->surfaceOffsets);
    image->setQPitch(this->getQPitch());
    image->setCubeFaceIndex(this->getCubeFaceIndex());
    image->associatedMemObject = this->associatedMemObject;
    return image;
}

static const uint32_t redescribeTableBytes[] = {
    17, // {CL_R, CL_UNSIGNED_INT8}        1 byte
    18, // {CL_R, CL_UNSIGNED_INT16}       2 byte
    19, // {CL_R, CL_UNSIGNED_INT32}       4 byte
    29, // {CL_RG, CL_UNSIGNED_INT32}      8 byte
    7   // {CL_RGBA, CL_UNSIGNED_INT32}    16 byte
};

bool Image::isFormatRedescribable(cl_image_format format) {
    const ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
    for (auto indexInRedescribeTable = 0u; indexInRedescribeTable < sizeof(redescribeTableBytes) / sizeof(uint32_t); indexInRedescribeTable++) {
        const uint32_t formatIndex = redescribeTableBytes[indexInRedescribeTable];
        const cl_image_format nonRedescribableFormat = readWriteSurfaceFormats[formatIndex].OCLImageFormat;
        if (nonRedescribableFormat.image_channel_data_type == format.image_channel_data_type &&
            nonRedescribableFormat.image_channel_order == format.image_channel_order) {
            return false;
        }
    }
    return true;
}

Image *Image::redescribe() {
    const uint32_t bytesPerPixel = this->surfaceFormatInfo.surfaceFormat.NumChannels * surfaceFormatInfo.surfaceFormat.PerChannelSizeInBytes;
    const uint32_t exponent = Math::log2(bytesPerPixel);
    DEBUG_BREAK_IF(exponent >= 5u);
    const uint32_t surfaceFormatIdx = redescribeTableBytes[exponent % 5];
    const ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
    const ClSurfaceFormatInfo *surfaceFormat = &readWriteSurfaceFormats[surfaceFormatIdx];

    auto imageFormatNew = this->imageFormat;
    imageFormatNew.image_channel_order = surfaceFormat->OCLImageFormat.image_channel_order;
    imageFormatNew.image_channel_data_type = surfaceFormat->OCLImageFormat.image_channel_data_type;

    DEBUG_BREAK_IF(nullptr == createFunction);
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags | CL_MEM_USE_HOST_PTR, flagsIntel, 0);
    auto image = createFunction(context,
                                memoryProperties,
                                flags | CL_MEM_USE_HOST_PTR,
                                flagsIntel,
                                this->getSize(),
                                this->getCpuAddress(),
                                imageFormatNew,
                                this->imageDesc,
                                this->isMemObjZeroCopy(),
                                this->getGraphicsAllocation(),
                                true,
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
    const ClSurfaceFormatInfo *surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageYPlane(Image::create(
        context,
        MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
        flags,
        0,
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
    surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);
    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane(Image::create(
        context,
        MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal));

    retVal = cmdQ->enqueueWriteImage(imageUVPlane.get(), CL_TRUE, origin, region, hostPtrRowPitch, 0, hostPtr, nullptr, 0, nullptr, nullptr);

    return retVal;
}

const ClSurfaceFormatInfo *Image::getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat, unsigned int clVersionSupport) {
    if (!imageFormat) {
        DEBUG_BREAK_IF("Invalid format");
        return nullptr;
    }

    ArrayRef<const ClSurfaceFormatInfo> formats = SurfaceFormats::surfaceFormats(flags, imageFormat, clVersionSupport);

    for (auto &format : formats) {
        if (format.OCLImageFormat.image_channel_data_type == imageFormat->image_channel_data_type &&
            format.OCLImageFormat.image_channel_order == imageFormat->image_channel_order) {
            return &format;
        }
    }
    DEBUG_BREAK_IF("Invalid format");
    return nullptr;
}

bool Image::isImage1d(const cl_image_desc &imageDesc) {
    auto imageType = imageDesc.image_type;
    auto buffer = castToObject<Buffer>(imageDesc.buffer);

    return (imageType == CL_MEM_OBJECT_IMAGE1D || imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
            imageType == CL_MEM_OBJECT_IMAGE1D_BUFFER || buffer);
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
                                     const MemoryPropertiesFlags &memoryProperties,
                                     cl_mem_flags flags,
                                     cl_mem_flags_intel flagsIntel,
                                     const cl_image_format *imageFormat,
                                     const cl_image_desc *imageDesc,
                                     const void *hostPtr,
                                     cl_int &errcodeRet) {

    if (!MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, imageDesc->mem_object)) {
        errcodeRet = CL_INVALID_VALUE;
        return nullptr;
    }

    bool isHostPtrUsed = (hostPtr != nullptr);
    bool areHostPtrFlagsUsed = memoryProperties.flags.copyHostPtr || memoryProperties.flags.useHostPtr;
    if (isHostPtrUsed != areHostPtrFlagsUsed) {
        errcodeRet = CL_INVALID_HOST_PTR;
        return nullptr;
    }

    errcodeRet = Image::validateImageFormat(imageFormat);
    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }

    const auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);

    errcodeRet = Image::validate(context, memoryProperties, surfaceFormat, imageDesc, hostPtr);
    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }

    return Image::create(context, memoryProperties, flags, flagsIntel, surfaceFormat, imageDesc, hostPtr, errcodeRet);
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

    size_t offset = getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes * origin[0];

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
