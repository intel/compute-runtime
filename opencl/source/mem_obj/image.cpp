/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/image.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/device/device_info.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/migration_sync_data.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/cl_device/cl_device_get_cap.inl"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/sharings/unified/unified_image.h"

#include "neo_igfxfmid.h"

namespace NEO {

ImageFactoryFuncs imageFactory[IGFX_MAX_CORE] = {};

namespace ImageFunctions {
ValidateAndCreateImageFunc validateAndCreateImage = Image::validateAndCreateImage;
} // namespace ImageFunctions

Image::Image(Context *context,
             const MemoryProperties &memoryProperties,
             cl_mem_flags flags,
             cl_mem_flags_intel flagsIntel,
             size_t size,
             void *memoryStorage,
             void *hostPtr,
             cl_image_format imageFormat,
             const cl_image_desc &imageDesc,
             bool zeroCopy,
             MultiGraphicsAllocation multiGraphicsAllocation,
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
             memoryStorage,
             hostPtr,
             std::move(multiGraphicsAllocation),
             zeroCopy,
             false,
             isObjectRedescribed),

      imageFormat(std::move(imageFormat)),
      imageDesc(imageDesc),
      surfaceFormatInfo(surfaceFormatInfo),

      baseMipLevel(baseMipLevel),
      mipCount(mipCount) {
    magic = objectMagic;
    if (surfaceOffsets)
        setSurfaceOffsets(surfaceOffsets->offset, surfaceOffsets->xOffset, surfaceOffsets->yOffset, surfaceOffsets->yOffsetForUVplane);
    else
        setSurfaceOffsets(0, 0, 0, 0);
}

Image::~Image() {

    auto &multiGa = getMultiGraphicsAllocation();

    for (const auto &ga : multiGa.getGraphicsAllocations()) {
        if (ga == nullptr || getMemoryManager() == nullptr || isObjectRedescribed == true) {
            continue;
        }

        auto deviceIndex = ga->getRootDeviceIndex();

        for (auto &engine : getMemoryManager()->getRegisteredEngines(deviceIndex)) {
            if (NEO::EngineHelpers::isComputeEngine(engine.getEngineType()) && engine.commandStreamReceiver->isDirectSubmissionEnabled()) {
                auto contextId = engine.osContext->getContextId();
                if (ga->isUsedByOsContext(contextId)) {

                    auto lock = engine.commandStreamReceiver->obtainUniqueOwnership();
                    engine.commandStreamReceiver->sendRenderStateCacheFlush();
                }
            }
        }
    }
}

void Image::transferData(void *dest, size_t destRowPitch, size_t destSlicePitch,
                         void *src, size_t srcRowPitch, size_t srcSlicePitch,
                         std::array<size_t, 3> copyRegion, std::array<size_t, 3> copyOrigin) {

    size_t pixelSize = surfaceFormatInfo.surfaceFormat.imageElementSizeInBytes;
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

Image *Image::create(Context *context,
                     const MemoryProperties &memoryProperties,
                     cl_mem_flags flags,
                     cl_mem_flags_intel flagsIntel,
                     const ClSurfaceFormatInfo *surfaceFormat,
                     const cl_image_desc *imageDesc,
                     const void *hostPtr,
                     cl_int &errcodeRet) {
    UNRECOVERABLE_IF(surfaceFormat == nullptr);

    RootDeviceIndicesContainer rootDeviceIndices;
    const RootDeviceIndicesContainer *pRootDeviceIndices;
    uint32_t defaultRootDeviceIndex;
    Device *defaultDevice;

    if (memoryProperties.associatedDevices.empty()) {
        defaultDevice = &context->getDevice(0)->getDevice();
        defaultRootDeviceIndex = defaultDevice->getRootDeviceIndex();
        pRootDeviceIndices = &context->getRootDeviceIndices();
    } else {
        for (const auto &device : memoryProperties.associatedDevices) {
            rootDeviceIndices.pushUnique(device->getRootDeviceIndex());
        }
        defaultDevice = memoryProperties.associatedDevices[0];
        defaultRootDeviceIndex = rootDeviceIndices[0];
        pRootDeviceIndices = &rootDeviceIndices;
    }

    size_t imageWidth = imageDesc->image_width;
    size_t imageHeight = getImageHeight(*imageDesc);
    size_t imageDepth = getImageDepth(*imageDesc);
    size_t imageCount = isImageArray(imageDesc->image_type) ? imageDesc->image_array_size : 1u;
    cl_image_desc imageDescriptor = *imageDesc;
    ImageInfo imgInfo = {};
    imgInfo.imgDesc = Image::convertDescriptor(imageDescriptor);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.mipCount = imageDesc->num_mip_levels;

    Buffer *parentBuffer = castToObject<Buffer>(imageDesc->mem_object);
    Image *parentImage = castToObject<Image>(imageDesc->mem_object);

    if (parentImage) {
        adjustImagePropertiesFromParentImage(imageWidth, imageHeight, imageDepth, imgInfo, imageDescriptor, parentImage);
    }

    // Driver needs to store rowPitch passed by the app in order to synchronize the host_ptr later on map call
    const auto hostPtrRowPitch = imageDesc->image_row_pitch ? imageDesc->image_row_pitch
                                                            : imageWidth * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    const auto hostPtrSlicePitch = getHostPtrSlicePitch(*imageDesc, hostPtrRowPitch, imageHeight);

    auto &defaultProductHelper = defaultDevice->getProductHelper();
    imgInfo.linearStorage = defaultProductHelper.isLinearStoragePreferred(Image::isImage1d(*imageDesc),
                                                                          memoryProperties.flags.forceLinearStorage);

    // if device doesn't support images, it can create only linear images
    if (!defaultDevice->getDeviceInfo().imageSupport && !imgInfo.linearStorage) {
        errcodeRet = CL_INVALID_OPERATION;
        return nullptr;
    }

    auto &clGfxCoreHelper = defaultDevice->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
    auto hwInfo = defaultDevice->getRootDeviceEnvironment().getHardwareInfo();
    bool compressionSupported = !imgInfo.linearStorage && !defaultProductHelper.isCompressionForbidden(*hwInfo);

    bool preferCompression = MemObjHelper::isSuitableForCompression(compressionSupported, memoryProperties,
                                                                    *context, true);
    preferCompression &= clGfxCoreHelper.allowImageCompression(surfaceFormat->oclImageFormat);
    preferCompression &= !clGfxCoreHelper.isFormatRedescribable(surfaceFormat->oclImageFormat);

    MemoryManager *memoryManager = context->getMemoryManager();
    size_t hostPtrMinSize = getHostPtrMinSize(imageDesc->image_type, surfaceFormat->oclImageFormat,
                                              hostPtrRowPitch, hostPtrSlicePitch, imageHeight, imageDepth, imageCount);
    void *hostPtrToSet = memoryProperties.flags.useHostPtr ? const_cast<void *>(hostPtr) : nullptr;

    auto maxRootDeviceIndex = context->getMaxRootDeviceIndex();
    auto multiGraphicsAllocation = MultiGraphicsAllocation(maxRootDeviceIndex);
    AllocationInfoType allocationInfos;
    allocationInfos.resize(maxRootDeviceIndex + 1u);

    bool isParentObject = parentBuffer || parentImage;
    auto imageFromBuffer = isImageFromBuffer(*imageDesc, parentBuffer);

    // get allocation for image
    for (auto &rootDeviceIndex : *pRootDeviceIndices) {
        allocationInfos[rootDeviceIndex] = {};
        auto &allocationInfo = allocationInfos[rootDeviceIndex];
        allocationInfo.zeroCopyAllowed = false;

        auto &rootDeviceEnvironment = *memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex];
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

        if (imageFromBuffer) {
            // Image from buffer - we never allocate memory, we use what buffer provides
            setAllocationInfoFromParentBuffer(allocationInfo, hostPtr, hostPtrToSet, parentBuffer, imgInfo, rootDeviceIndex);
            if (!gfxCoreHelper.checkResourceCompatibility(*allocationInfo.memory)) {
                cleanAllGraphicsAllocations(*context, *memoryManager, allocationInfos, isParentObject);
                errcodeRet = CL_INVALID_MEM_OBJECT;
                return nullptr;
            }
        } else if (parentImage != nullptr) {
            // Image from parent image - reuse allocation from parent image
            allocationInfo.memory = parentImage->getGraphicsAllocation(rootDeviceIndex);
            allocationInfo.memory->getDefaultGmm()->queryImageParams(imgInfo);
        } else if (memoryProperties.flags.useHostPtr) {
            // create graphics allocation from shared context
            setAllocationInfoFromHostPtr(allocationInfo, rootDeviceIndex, hwInfo, memoryProperties, imgInfo, context,
                                         preferCompression, memoryManager, hostPtr, hostPtrMinSize);
        } else {
            // create graphics allocation from image info
            setAllocationInfoFromImageInfo(allocationInfo, rootDeviceIndex, hwInfo, memoryProperties, imgInfo, context,
                                           preferCompression, memoryManager);
        }

        // if we couldn't get allocation for image, return nullptr
        if (!allocationInfo.memory) {
            errcodeRet = CL_OUT_OF_HOST_MEMORY;
            cleanAllGraphicsAllocations(*context, *memoryManager, allocationInfos, isParentObject);
            return nullptr;
        }

        if (parentBuffer == nullptr) {
            allocationInfo.memory->setAllocationType(AllocationType::image);
        }

        if (parentImage) {
            setImageDesriptorIfParentImage(imageDescriptor, imageWidth, imageHeight, imageDesc->mem_object);
            parentImage->incRefInternal();
            imgInfo.imgDesc = Image::convertDescriptor(imageDescriptor);
        }

        auto isWritable = !memoryProperties.flags.readOnly &&
                          !memoryProperties.flags.hostReadOnly &&
                          !memoryProperties.flags.hostNoAccess;

        allocationInfo.memory->setMemObjectsAllocationWithWritableFlags(isWritable);
        allocationInfo.transferNeeded |= memoryProperties.flags.copyHostPtr;

        DBG_LOG(LogMemoryObject, __FUNCTION__, "hostPtr:", hostPtr, "size:", allocationInfo.memory->getUnderlyingBufferSize(),
                "memoryStorage:", allocationInfo.memory->getUnderlyingBuffer(), "GPU address:", std::hex, allocationInfo.memory->getGpuAddress());

        multiGraphicsAllocation.addAllocation(allocationInfo.memory);
    }

    if (pRootDeviceIndices->size() > 1) {
        multiGraphicsAllocation.setMultiStorage(!MemoryPoolHelper::isSystemMemoryPool(allocationInfos[defaultRootDeviceIndex].memory->getMemoryPool()));
    }

    Image *image = createImageHw(context, memoryProperties, flags, flagsIntel, imgInfo.size, hostPtrToSet, surfaceFormat->oclImageFormat,
                                 imageDescriptor, allocationInfos[defaultRootDeviceIndex].zeroCopyAllowed, std::move(multiGraphicsAllocation), false, 0, 0, surfaceFormat);

    setImageProperties(image, *imageDesc, imgInfo, parentImage, parentBuffer, hostPtrRowPitch, hostPtrSlicePitch, imageCount, hostPtrMinSize);

    errcodeRet = CL_SUCCESS;
    auto &defaultHwInfo = defaultDevice->getHardwareInfo();
    if (context->isProvidingPerformanceHints()) {

        auto &allocationInfo = allocationInfos[defaultRootDeviceIndex];

        providePerformanceHintForCreateImage(image, defaultHwInfo, allocationInfo, context);
    }

    for (auto &allocationInfo : allocationInfos) {
        if (allocationInfo.mapAllocation) {
            image->mapAllocations.addAllocation(allocationInfo.mapAllocation);
        }
    }
    if (allocationInfos[defaultRootDeviceIndex].transferNeeded) {
        auto memory = image->getGraphicsAllocation(defaultRootDeviceIndex);
        std::array<size_t, 3> copyOrigin = {{0, 0, 0}};
        std::array<size_t, 3> copyRegion = {{imageWidth, imageHeight, std::max(imageDepth, imageCount)}};
        if (imageDesc->image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
            copyRegion = {imageWidth, imageCount, 1};
        }

        auto &defaultGfxCoreHelper = defaultDevice->getGfxCoreHelper();
        auto allocationInSystemMemory = MemoryPoolHelper::isSystemMemoryPool(memory->getMemoryPool());
        bool isCpuTransferPreferred = imgInfo.linearStorage && defaultGfxCoreHelper.isCpuImageTransferPreferred(defaultHwInfo);
        bool isCpuTransferPreferredInSystemMemory = imgInfo.linearStorage && allocationInSystemMemory;

        if (isCpuTransferPreferredInSystemMemory) {
            void *pDestinationAddress = memory->getUnderlyingBuffer();
            image->transferData(pDestinationAddress, imgInfo.rowPitch, imgInfo.slicePitch,
                                const_cast<void *>(hostPtr), hostPtrRowPitch, hostPtrSlicePitch,
                                copyRegion, copyOrigin);

        } else if (isCpuTransferPreferred) {
            void *pDestinationAddress = context->getMemoryManager()->lockResource(memory);
            image->transferData(pDestinationAddress, imgInfo.rowPitch, imgInfo.slicePitch,
                                const_cast<void *>(hostPtr), hostPtrRowPitch, hostPtrSlicePitch,
                                copyRegion, copyOrigin);
            context->getMemoryManager()->unlockResource(memory);

        } else {
            auto cmdQ = context->getSpecialQueue(defaultRootDeviceIndex);
            defaultDevice->stopDirectSubmissionForCopyEngine();
            if (isNV12Image(&image->getImageFormat())) {
                errcodeRet = image->writeNV12Planes(hostPtr, hostPtrRowPitch, defaultRootDeviceIndex);
            } else {
                errcodeRet = cmdQ->enqueueWriteImage(image, CL_TRUE, &copyOrigin[0], &copyRegion[0],
                                                     hostPtrRowPitch, hostPtrSlicePitch,
                                                     hostPtr, image->getMapAllocation(defaultRootDeviceIndex), 0, nullptr, nullptr);
            }
        }
        auto migrationSyncData = image->getMultiGraphicsAllocation().getMigrationSyncData();
        if (migrationSyncData) {
            migrationSyncData->setCurrentLocation(defaultRootDeviceIndex);
        }
    }

    if (imageFromBuffer) {
        parentBuffer->incRefInternal();
    }

    if (errcodeRet != CL_SUCCESS) {
        image->release();
        image = nullptr;
        cleanAllGraphicsAllocations(*context, *memoryManager, allocationInfos, isParentObject);
    }

    return image;
}

Image *Image::createImageHw(Context *context, const MemoryProperties &memoryProperties, cl_mem_flags flags, cl_mem_flags_intel flagsIntel, size_t size, void *hostPtr,
                            const cl_image_format &imageFormat, const cl_image_desc &imageDesc,
                            bool zeroCopy, MultiGraphicsAllocation multiGraphicsAllocation,
                            bool isObjectRedescribed, uint32_t baseMipLevel, uint32_t mipCount,
                            const ClSurfaceFormatInfo *surfaceFormatInfo) {
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = imageFactory[hwInfo.platform.eRenderCoreFamily].createImageFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);
    auto image = funcCreate(context, memoryProperties, flags, flagsIntel, size, hostPtr, imageFormat, imageDesc,
                            zeroCopy, std::move(multiGraphicsAllocation), isObjectRedescribed, baseMipLevel, mipCount, surfaceFormatInfo, nullptr);
    DEBUG_BREAK_IF(nullptr == image);
    image->createFunction = funcCreate;
    return image;
}

Image *Image::createSharedImage(Context *context, SharingHandler *sharingHandler, const McsSurfaceInfo &mcsSurfaceInfo,
                                MultiGraphicsAllocation multiGraphicsAllocation, GraphicsAllocation *mcsAllocation,
                                cl_mem_flags flags, cl_mem_flags_intel flagsIntel, const ClSurfaceFormatInfo *surfaceFormat,
                                ImageInfo &imgInfo, uint32_t cubeFaceIndex, uint32_t baseMipLevel, uint32_t mipCount, bool hasUnifiedMcsSurface) {
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
    auto size = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)->getUnderlyingBufferSize();
    auto sharedImage = createImageHw(
        context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags, flagsIntel, size, nullptr,
        surfaceFormat->oclImageFormat, Image::convertDescriptor(imgInfo.imgDesc), false,
        std::move(multiGraphicsAllocation), false, baseMipLevel, mipCount, surfaceFormat);
    sharedImage->setSharingHandler(sharingHandler);
    sharedImage->setMcsAllocation(mcsAllocation);
    sharedImage->setQPitch(imgInfo.qPitch);
    sharedImage->setHostPtrRowPitch(imgInfo.imgDesc.imageRowPitch);
    sharedImage->setHostPtrSlicePitch(imgInfo.imgDesc.imageSlicePitch);
    sharedImage->setCubeFaceIndex(cubeFaceIndex);
    sharedImage->setSurfaceOffsets(imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane);
    sharedImage->setMcsSurfaceInfo(mcsSurfaceInfo);
    sharedImage->setPlane(imgInfo.plane);
    sharedImage->setIsDisplayable(imgInfo.isDisplayable);
    sharedImage->setIsUnifiedMcsSurface(hasUnifiedMcsSurface);
    return sharedImage;
}

cl_int Image::validate(Context *context,
                       const MemoryProperties &memoryProperties,
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
        if ((imageDesc->mem_object != nullptr) && (pClDevice->getSharedDeviceInfo().imageSupport == false)) {
            return CL_INVALID_OPERATION;
        }

        pClDevice->getCap<CL_DEVICE_IMAGE2D_MAX_WIDTH>(reinterpret_cast<const void *&>(maxWidth), srcSize, retSize);
        pClDevice->getCap<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(reinterpret_cast<const void *&>(maxHeight), srcSize, retSize);
        if (imageDesc->image_width > *maxWidth ||
            imageDesc->image_height > *maxHeight) {
            return CL_INVALID_IMAGE_SIZE;
        }
        if (parentBuffer) { // Image 2d from buffer
            pClDevice->getCap<CL_DEVICE_IMAGE_PITCH_ALIGNMENT>(reinterpret_cast<const void *&>(pitchAlignment), srcSize, retSize);
            pClDevice->getCap<CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT>(reinterpret_cast<const void *&>(baseAddressAlignment), srcSize, retSize);

            const auto rowSize = imageDesc->image_row_pitch != 0 ? imageDesc->image_row_pitch : alignUp(imageDesc->image_width * surfaceFormat->surfaceFormat.numChannels * surfaceFormat->surfaceFormat.perChannelSizeInBytes, *pitchAlignment);
            const auto minimumBufferSize = imageDesc->image_height * rowSize;

            if ((imageDesc->image_row_pitch % (*pitchAlignment)) ||
                ((parentBuffer->getFlags() & CL_MEM_USE_HOST_PTR) && (reinterpret_cast<uint64_t>(parentBuffer->getHostPtr()) % (*baseAddressAlignment))) ||
                (minimumBufferSize > parentBuffer->getSize())) {
                return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
            } else if (memoryProperties.flags.useHostPtr || memoryProperties.flags.copyHostPtr) {
                return CL_INVALID_VALUE;
            }
        }
        if (parentImage && (!isNV12Image(&parentImage->getImageFormat()) && !isPackedYuvImage(&parentImage->getImageFormat()))) { // Image 2d from image 2d
            if (!parentImage->hasSameDescriptor(*imageDesc) || !parentImage->hasValidParentImageFormat(surfaceFormat->oclImageFormat)) {
                return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
            }
        }
        if (parentImage && isPackedYuvImage(&parentImage->getImageFormat())) {
            if (!parentImage->hasValidParentImageFormat(surfaceFormat->oclImageFormat) || imageDesc->image_width != parentImage->getImageDesc().image_width / 2) {
                return CL_INVALID_IMAGE_DESCRIPTOR;
            }
        }
        if (!((parentImage && isNV12Image(&parentImage->getImageFormat())) || (parentImage && isPackedYuvImage(&parentImage->getImageFormat()))) &&
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
            if (imageDesc->image_row_pitch % surfaceFormat->surfaceFormat.imageElementSizeInBytes != 0 ||
                imageDesc->image_row_pitch < imageDesc->image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes) {
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

    return validateImageTraits(context, memoryProperties, &surfaceFormat->oclImageFormat, imageDesc, hostPtr);
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
                                const MemoryProperties &memoryProperties,
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
        } else if (imageDesc->image_height % 4 ||
                   imageDesc->image_width % 4 ||
                   imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D) {
            errorCode = CL_INVALID_IMAGE_DESCRIPTOR;
            break;
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

cl_int Image::validatePackedYUV(const MemoryProperties &memoryProperties, const cl_image_desc *imageDesc) {
    cl_int errorCode = CL_SUCCESS;
    while (true) {
        if (!memoryProperties.flags.readOnly) {
            errorCode = CL_INVALID_VALUE;
            break;
        } else if (imageDesc->image_width % 2 != 0 ||
                   imageDesc->image_type != CL_MEM_OBJECT_IMAGE2D) {
            errorCode = CL_INVALID_IMAGE_DESCRIPTOR;
            break;
        }
        break;
    }
    return errorCode;
}

cl_int Image::validateImageTraits(Context *context, const MemoryProperties &memoryProperties, const cl_image_format *imageFormat, const cl_image_desc *imageDesc, const void *hostPtr) {
    if (isNV12Image(imageFormat))
        return validatePlanarYUV(context, memoryProperties, imageDesc, hostPtr);
    else if (isPackedYuvImage(imageFormat))
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
        [[fallthrough]];
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
    auto gmmHelper = context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper();

    ImageInfo imgInfo = {};
    cl_image_desc imageDescriptor = *imageDesc;
    imgInfo.imgDesc = Image::convertDescriptor(imageDescriptor);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;

    auto gmm = std::make_unique<Gmm>(gmmHelper, imgInfo, StorageInfo{}, false);

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
    case ImageType::image2D:
        return CL_MEM_OBJECT_IMAGE2D;
    case ImageType::image3D:
        return CL_MEM_OBJECT_IMAGE3D;
    case ImageType::image2DArray:
        return CL_MEM_OBJECT_IMAGE2D_ARRAY;
    case ImageType::image1D:
        return CL_MEM_OBJECT_IMAGE1D;
    case ImageType::image1DArray:
        return CL_MEM_OBJECT_IMAGE1D_ARRAY;
    case ImageType::image1DBuffer:
        return CL_MEM_OBJECT_IMAGE1D_BUFFER;
    default:
        break;
    }
    return 0;
}

ImageType Image::convertType(const cl_mem_object_type type) {
    switch (type) {
    case CL_MEM_OBJECT_IMAGE2D:
        return ImageType::image2D;
    case CL_MEM_OBJECT_IMAGE3D:
        return ImageType::image3D;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return ImageType::image2DArray;
    case CL_MEM_OBJECT_IMAGE1D:
        return ImageType::image1D;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        return ImageType::image1DArray;
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return ImageType::image1DBuffer;
    default:
        break;
    }
    return ImageType::invalid;
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
    size_t srcParamSize = GetInfo::invalidSourceSize;
    void *srcParam = nullptr;
    auto imageDesc = getImageDesc();
    auto surfFmtInfo = getSurfaceFormatInfo();
    size_t retParam;
    size_t arraySize = imageDesc.image_array_size * (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY);
    size_t slicePitch = hostPtrSlicePitch * !(imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D || imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D || imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER);

    switch (paramName) {
    case CL_IMAGE_FORMAT:
        srcParamSize = sizeof(cl_image_format);
        srcParam = &(surfFmtInfo.oclImageFormat);
        break;

    case CL_IMAGE_ELEMENT_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &(surfFmtInfo.surfaceFormat.imageElementSizeInBytes);
        break;

    case CL_IMAGE_ROW_PITCH:
        srcParamSize = sizeof(size_t);
        if (mcsSurfaceInfo.multisampleCount > 1) {
            retParam = imageDesc.image_width * surfFmtInfo.surfaceFormat.imageElementSizeInBytes * imageDesc.num_samples;
        } else {
            retParam = hostPtrRowPitch;
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_SLICE_PITCH:
        srcParamSize = sizeof(size_t);
        srcParam = &slicePitch;
        break;

    case CL_IMAGE_WIDTH:
        srcParamSize = sizeof(size_t);
        retParam = imageDesc.image_width;
        if (this->baseMipLevel) {
            retParam = imageDesc.image_width >> this->baseMipLevel;
            retParam = std::max(retParam, static_cast<size_t>(1));
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_HEIGHT:
        srcParamSize = sizeof(size_t);
        retParam = imageDesc.image_height * !((imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D) || (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) || (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER));
        if ((retParam != 0) && (this->baseMipLevel > 0)) {
            retParam = retParam >> this->baseMipLevel;
            retParam = std::max(retParam, static_cast<size_t>(1));
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_DEPTH:
        srcParamSize = sizeof(size_t);
        retParam = imageDesc.image_depth * (imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D);
        if ((retParam != 0) && (this->baseMipLevel > 0)) {
            retParam = retParam >> this->baseMipLevel;
            retParam = std::max(retParam, static_cast<size_t>(1));
        }
        srcParam = &retParam;
        break;

    case CL_IMAGE_ARRAY_SIZE:
        srcParamSize = sizeof(size_t);
        srcParam = &(arraySize);
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

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcParamSize, getInfoStatus);

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
    uint32_t redescribeTableCol = this->surfaceFormatInfo.surfaceFormat.numChannels / 2;
    uint32_t redescribeTableRow = this->surfaceFormatInfo.surfaceFormat.perChannelSizeInBytes / 2;

    ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();

    uint32_t surfaceFormatIdx = redescribeTable[redescribeTableRow][redescribeTableCol];
    surfaceFormat = &readWriteSurfaceFormats[surfaceFormatIdx];

    imageFormatNew.image_channel_order = surfaceFormat->oclImageFormat.image_channel_order;
    imageFormatNew.image_channel_data_type = surfaceFormat->oclImageFormat.image_channel_data_type;

    DEBUG_BREAK_IF(nullptr == createFunction);
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags | CL_MEM_USE_HOST_PTR, flagsIntel, 0,
                                                                                         &context->getDevice(0)->getDevice());
    auto image = createFunction(context,
                                memoryProperties,
                                flags | CL_MEM_USE_HOST_PTR,
                                flagsIntel,
                                this->getSize(),
                                this->getCpuAddress(),
                                imageFormatNew,
                                imageDescNew,
                                this->isMemObjZeroCopy(),
                                this->multiGraphicsAllocation,
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

Image *Image::redescribe() {
    const uint32_t bytesPerPixel = this->surfaceFormatInfo.surfaceFormat.numChannels * surfaceFormatInfo.surfaceFormat.perChannelSizeInBytes;
    const uint32_t exponent = Math::log2(bytesPerPixel);
    DEBUG_BREAK_IF(exponent >= 5u);
    const uint32_t surfaceFormatIdx = redescribeTableBytes[exponent % 5];
    const ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
    const ClSurfaceFormatInfo *surfaceFormat = &readWriteSurfaceFormats[surfaceFormatIdx];

    auto imageFormatNew = this->imageFormat;
    imageFormatNew.image_channel_order = surfaceFormat->oclImageFormat.image_channel_order;
    imageFormatNew.image_channel_data_type = surfaceFormat->oclImageFormat.image_channel_data_type;

    DEBUG_BREAK_IF(nullptr == createFunction);
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags | CL_MEM_USE_HOST_PTR, flagsIntel, 0,
                                                                                         &context->getDevice(0)->getDevice());
    auto image = createFunction(context,
                                memoryProperties,
                                flags | CL_MEM_USE_HOST_PTR,
                                flagsIntel,
                                this->getSize(),
                                this->getCpuAddress(),
                                imageFormatNew,
                                this->imageDesc,
                                this->isMemObjZeroCopy(),
                                this->multiGraphicsAllocation,
                                true,
                                this->baseMipLevel,
                                this->mipCount,
                                surfaceFormat,
                                &this->surfaceOffsets);
    image->setQPitch(this->getQPitch());
    image->setCubeFaceIndex(this->getCubeFaceIndex());
    image->associatedMemObject = this->associatedMemObject;
    image->createFunction = createFunction;
    image->plane = this->plane;
    return image;
}

void Image::transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(hostPtr, hostPtrRowPitch, hostPtrSlicePitch,
                 memoryStorage, imageDesc.image_row_pitch, imageDesc.image_slice_pitch,
                 copySize, copyOffset);
}

void Image::transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(memoryStorage, imageDesc.image_row_pitch, imageDesc.image_slice_pitch,
                 hostPtr, hostPtrRowPitch, hostPtrSlicePitch,
                 copySize, copyOffset);
}

cl_int Image::writeNV12Planes(const void *hostPtr, size_t hostPtrRowPitch, uint32_t rootDeviceIndex) {
    CommandQueue *cmdQ = context->getSpecialQueue(rootDeviceIndex);
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
    const ClSurfaceFormatInfo *surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageYPlane(Image::create(
        context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
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
    surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    // Create NV12 UV Plane image
    std::unique_ptr<Image> imageUVPlane(Image::create(
        context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal));

    retVal = cmdQ->enqueueWriteImage(imageUVPlane.get(), CL_TRUE, origin, region, hostPtrRowPitch, 0, hostPtr, nullptr, 0, nullptr, nullptr);

    return retVal;
}

const ClSurfaceFormatInfo *Image::getSurfaceFormatFromTable(cl_mem_flags flags, const cl_image_format *imageFormat, bool supportsOcl20Features) {
    if (!imageFormat) {
        DEBUG_BREAK_IF("Invalid format");
        return nullptr;
    }

    ArrayRef<const ClSurfaceFormatInfo> formats = SurfaceFormats::surfaceFormats(flags, imageFormat, supportsOcl20Features);

    for (auto &format : formats) {
        if (format.oclImageFormat.image_channel_data_type == imageFormat->image_channel_data_type &&
            format.oclImageFormat.image_channel_order == imageFormat->image_channel_order) {
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

bool Image::isImage3d(cl_mem_object_type imageType) {
    return imageType == CL_MEM_OBJECT_IMAGE3D;
}

bool Image::isImageArray(cl_mem_object_type imageType) {
    return (imageType == CL_MEM_OBJECT_IMAGE1D_ARRAY || imageType == CL_MEM_OBJECT_IMAGE2D_ARRAY);
}

bool Image::isDepthFormat(const cl_image_format &imageFormat) {
    return imageFormat.image_channel_order == CL_DEPTH || imageFormat.image_channel_order == CL_DEPTH_STENCIL;
}

size_t Image::getImageHeight(const cl_image_desc &imageDesc) {
    switch (imageDesc.image_type) {
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return imageDesc.image_height;
    default:
        return 1u;
    }
}

size_t Image::getHostPtrMinSize(cl_mem_object_type imageType, const cl_image_format &imageFormat,
                                size_t hostPtrRowPitch, size_t hostPtrSlicePitch, size_t imageHeight, size_t imageDepth, size_t imageCount) {
    size_t hostPtrMinSize = 0;

    switch (imageType) {
    case CL_MEM_OBJECT_IMAGE3D:
        hostPtrMinSize = hostPtrSlicePitch * imageDepth;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        if (isNV12Image(&imageFormat)) {
            hostPtrMinSize = hostPtrRowPitch * imageHeight + hostPtrRowPitch * imageHeight / 2;
        } else {
            hostPtrMinSize = hostPtrRowPitch * imageHeight;
        }
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        hostPtrMinSize = hostPtrSlicePitch * imageCount;
        break;
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        hostPtrMinSize = hostPtrRowPitch;
        break;
    default:
        DEBUG_BREAK_IF("Unsupported cl_image_type");
        break;
    }

    return hostPtrMinSize;
}

size_t Image::getHostPtrSlicePitch(const cl_image_desc &imageDesc, size_t hostPtrRowPitch, size_t imageHeight) {
    size_t hostPtrSlicePitch = 0;
    switch (imageDesc.image_type) {
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        hostPtrSlicePitch = 0;
        break;
    default:
        hostPtrSlicePitch = imageDesc.image_slice_pitch ? imageDesc.image_slice_pitch
                                                        : hostPtrRowPitch * imageHeight;
    }
    return hostPtrSlicePitch;
}

bool Image::isParentMemObject(const cl_image_desc &imageDesc) {
    bool parementMemObject = imageDesc.mem_object != nullptr;
    parementMemObject &= (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) || (imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D);
    return parementMemObject;
}

bool Image::isImageFromBuffer(const cl_image_desc &imageDesc, Buffer *buffer) {
    bool imageFromBuffer = buffer != nullptr;

    imageFromBuffer &= (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) || (imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D);
    return imageFromBuffer;
}

void Image::setImageProperties(Image *image, const cl_image_desc &imageDesc, const ImageInfo &imageInfo, Image *parentImage, Buffer *parentBuffer,
                               size_t hostPtrRowPitch, size_t hostPtrSlicePitch, size_t imageCount, size_t hostPtrMinSize) {
    if (!isImageArray(imageDesc.image_type)) {
        image->imageDesc.image_array_size = 0;
    }

    if (isParentMemObject(imageDesc)) {
        image->associatedMemObject = castToObject<MemObj>(imageDesc.mem_object);
    }

    image->setHostPtrRowPitch(hostPtrRowPitch);
    image->setHostPtrSlicePitch(hostPtrSlicePitch);
    image->setImageCount(imageCount);
    image->setHostPtrMinSize(hostPtrMinSize);
    image->setImageRowPitch(imageInfo.rowPitch);
    image->setImageSlicePitch(imageInfo.slicePitch);
    image->setQPitch(imageInfo.qPitch);
    image->setSurfaceOffsets(imageInfo.offset, imageInfo.xOffset, imageInfo.yOffset, imageInfo.yOffsetForUVPlane);
    image->setMipCount(imageInfo.mipCount);
    image->setPlane(imageInfo.plane);

    if (parentImage) {
        image->setMediaPlaneType(static_cast<cl_uint>(imageDesc.image_depth));
        image->setParentSharingHandler(parentImage->getSharingHandler());
    } else if (parentBuffer) {
        image->setParentSharingHandler(parentBuffer->getSharingHandler());
    }
}

void Image::adjustImagePropertiesFromParentImage(size_t &width, size_t &height, size_t &depth, ImageInfo &imageInfo, cl_image_desc &descriptor, Image *parentImage) {
    if (isPackedYuvImage(&parentImage->getImageFormat())) {
        width = parentImage->getImageDesc().image_width / 2;
        height = parentImage->getImageDesc().image_height;
    } else {
        width = parentImage->getImageDesc().image_width;
        height = parentImage->getImageDesc().image_height;
        depth = 1;
        if (isNV12Image(&parentImage->getImageFormat())) {
            if (descriptor.image_depth == 1) { // UV Plane
                width /= 2;
                height /= 2;
                imageInfo.plane = GMM_PLANE_U;
            } else {
                imageInfo.plane = GMM_PLANE_Y;
            }
        }

        imageInfo.surfaceFormat = &parentImage->surfaceFormatInfo.surfaceFormat;
        descriptor = parentImage->getImageDesc();
    }
}

void Image::setAllocationInfoFromParentBuffer(CreateMemObj::AllocationInfo &allocationInfo, const void *&hostPtr, void *&hostPtrToSet,
                                              Buffer *parentBuffer, ImageInfo &imageInfo, uint32_t rootDeviceIndex) {

    allocationInfo.zeroCopyAllowed = true;
    allocationInfo.memory = parentBuffer->getGraphicsAllocation(rootDeviceIndex);

    hostPtr = parentBuffer->getHostPtr();
    hostPtrToSet = const_cast<void *>(hostPtr);
    GmmTypesConverter::queryImgFromBufferParams(imageInfo, allocationInfo.memory);

    UNRECOVERABLE_IF(imageInfo.offset != 0);
    imageInfo.offset = parentBuffer->getOffset();
}

void Image::setAllocationInfoFromHostPtr(CreateMemObj::AllocationInfo &allocationInfo, uint32_t rootDeviceIndex, const HardwareInfo &hwInfo,
                                         const MemoryProperties &memoryProperties, ImageInfo &imageInfo, Context *context, bool preferCompression,
                                         MemoryManager *memoryManager, const void *hostPtr, size_t hostPtrMinSize) {

    AllocationProperties properties = MemObjHelper::getAllocationPropertiesWithImageInfo(rootDeviceIndex, imageInfo,
                                                                                         false, // allocateMemory
                                                                                         memoryProperties, hwInfo,
                                                                                         context->getDeviceBitfieldForAllocation(rootDeviceIndex),
                                                                                         context->isSingleDeviceContext());
    properties.flags.preferCompressed = preferCompression;
    allocationInfo.memory = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);

    if (allocationInfo.memory) {
        auto allocationCpuPtr = allocationInfo.memory->getUnderlyingBuffer();

        if (allocationCpuPtr == hostPtr) {
            allocationInfo.zeroCopyAllowed = true;
        } else {
            allocationInfo.zeroCopyAllowed = false;
            allocationInfo.transferNeeded = true;

            AllocationProperties properties{rootDeviceIndex,
                                            false, // allocateMemory
                                            hostPtrMinSize, AllocationType::mapAllocation,
                                            false, // isMultiStorageAllocation
                                            context->getDeviceBitfieldForAllocation(rootDeviceIndex)};
            properties.flags.flushL3RequiredForRead = true;
            properties.flags.flushL3RequiredForWrite = true;
            properties.flags.preferCompressed = preferCompression;
            allocationInfo.mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtr);
        }
    }
}

void Image::setAllocationInfoFromImageInfo(CreateMemObj::AllocationInfo &allocationInfo, uint32_t rootDeviceIndex, const HardwareInfo &hwInfo,
                                           const MemoryProperties &memoryProperties, ImageInfo &imageInfo, Context *context, bool preferCompression,
                                           MemoryManager *memoryManager) {
    // get allocation from image info
    AllocationProperties properties = MemObjHelper::getAllocationPropertiesWithImageInfo(rootDeviceIndex, imageInfo,
                                                                                         true, // allocateMemory
                                                                                         memoryProperties, hwInfo,
                                                                                         context->getDeviceBitfieldForAllocation(rootDeviceIndex),
                                                                                         context->isSingleDeviceContext());
    properties.flags.preferCompressed = preferCompression;
    allocationInfo.memory = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    if (allocationInfo.memory && MemoryPoolHelper::isSystemMemoryPool(allocationInfo.memory->getMemoryPool())) {
        allocationInfo.zeroCopyAllowed = true;
    }
}

void Image::providePerformanceHintForCreateImage(Image *image, const HardwareInfo &hwInfo, CreateMemObj::AllocationInfo &allocationInfo, Context *context) {
    if (GfxCoreHelper::compressedImagesSupported(hwInfo)) {
        if (allocationInfo.memory->isCompressionEnabled()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, IMAGE_IS_COMPRESSED, image);
        } else {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, IMAGE_IS_NOT_COMPRESSED, image);
        }
    }

    if (image->isMemObjZeroCopy()) {
        context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS, static_cast<cl_mem>(image));
    }
}

void Image::setImageDesriptorIfParentImage(cl_image_desc &imageDescriptor, size_t imageWidth, size_t imageHeight, cl_mem memObject) {
    imageDescriptor.image_height = imageHeight;
    imageDescriptor.image_width = imageWidth;
    imageDescriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDescriptor.image_depth = 1;
    imageDescriptor.image_array_size = 0;
    imageDescriptor.image_row_pitch = 0;
    imageDescriptor.image_slice_pitch = 0;
    imageDescriptor.mem_object = memObject;
}

size_t Image::getImageDepth(const cl_image_desc &imageDesc) {
    return Image::isImage3d(imageDesc.image_type) ? imageDesc.image_depth : 1u;
}

cl_mem Image::validateAndCreateImage(cl_context context,
                                     const cl_mem_properties *properties,
                                     cl_mem_flags flags,
                                     cl_mem_flags_intel flagsIntel,
                                     const cl_image_format *imageFormat,
                                     const cl_image_desc *imageDesc,
                                     const void *hostPtr,
                                     cl_int &errcodeRet) {

    Context *pContext = nullptr;
    errcodeRet = validateObjects(withCastToInternal(context, &pContext));
    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }

    MemoryProperties memoryProperties{};
    cl_mem_flags_intel emptyFlagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if ((false == ClMemoryPropertiesHelper::parseMemoryProperties(nullptr, memoryProperties, flags, emptyFlagsIntel, allocflags,
                                                                  ClMemoryPropertiesHelper::ObjType::image, *pContext)) ||
        (false == MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, emptyFlagsIntel, imageDesc->mem_object,
                                                                 *pContext))) {
        errcodeRet = CL_INVALID_VALUE;
        return nullptr;
    }

    if ((false == ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                  ClMemoryPropertiesHelper::ObjType::image, *pContext)) ||
        (false == MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, imageDesc->mem_object,
                                                                 *pContext))) {
        errcodeRet = CL_INVALID_PROPERTY;
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

    const auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, imageFormat, pContext->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    errcodeRet = Image::validate(pContext, memoryProperties, surfaceFormat, imageDesc, hostPtr);
    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }

    Image *image = nullptr;
    UnifiedSharingMemoryDescription extMem{};

    if (memoryProperties.handle) {
        if (validateHandleType(memoryProperties, extMem)) {
            extMem.handle = reinterpret_cast<void *>(memoryProperties.handle);
            image = UnifiedImage::createSharedUnifiedImage(pContext, flags, extMem, imageFormat, imageDesc, &errcodeRet);
        } else {
            errcodeRet = CL_INVALID_PROPERTY;
            return nullptr;
        }
    } else {
        image = Image::create(pContext, memoryProperties, flags, flagsIntel, surfaceFormat, imageDesc, hostPtr, errcodeRet);
    }

    if (errcodeRet == CL_SUCCESS) {
        image->storeProperties(properties);
    }

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

    bool isValidOrder = isNV12Image(imageFormat) || isPackedYuvImage(imageFormat);

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

    size_t offset = getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes * origin[0];

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

    if (origin[0] + region[0] > imgDesc.image_width) {
        return CL_INVALID_VALUE;
    }

    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D || imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
        imgDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
        if (origin[1] + region[1] > imgDesc.image_height) {
            return CL_INVALID_VALUE;
        }
    }

    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
        if (origin[2] + region[2] > imgDesc.image_depth) {
            return CL_INVALID_VALUE;
        }
    }

    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        if (origin[1] + region[1] > imgDesc.image_array_size) {
            return CL_INVALID_VALUE;
        }
    }

    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        if (origin[2] + region[2] > imgDesc.image_array_size) {
            return CL_INVALID_VALUE;
        }
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
    case CL_YUYV_INTEL:
        return imageFormat.image_channel_order == CL_RGBA;
    default:
        return false;
    }
}

cl_int Image::checkIfDeviceSupportsImages(cl_context context) {
    auto pContext = castToObject<Context>(context);
    if (pContext != nullptr) {
        auto capabilityTable = pContext->getDevice(0)->getHardwareInfo().capabilityTable;
        if (!capabilityTable.supportsImages) {
            return CL_INVALID_OPERATION;
        }

        return CL_SUCCESS;
    }

    return CL_INVALID_CONTEXT;
}
void Image::fillImageRegion(size_t *region) const {
    region[0] = imageDesc.image_width;
    if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        region[1] = imageDesc.image_array_size;
    } else if (Image::isImage1d(imageDesc)) {
        region[1] = 1u;
    } else {
        region[1] = imageDesc.image_height;
    }

    if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        region[2] = imageDesc.image_array_size;
    } else if (imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
        region[2] = imageDesc.image_depth;
    } else {
        region[2] = 1u;
    }
}

void Image::setAs3DUavOrRtvImage(bool isUavOrRtv) {
    is3DUAVOrRTV = isUavOrRtv;
}

} // namespace NEO
