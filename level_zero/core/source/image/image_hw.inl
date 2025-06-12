/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/image_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/release_helper/release_helper.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/source/sampler/sampler_imp.h"

#include "encode_surface_state_args.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t ImageCoreFamily<gfxCoreFamily>::initialize(Device *device, const ze_image_desc_t *desc) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    const auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    StructuresLookupTable lookupTable = {};

    lookupTable.areImageProperties = true;
    lookupTable.imageProperties.imageDescriptor = convertDescriptor(*desc);

    auto parseResult = prepareL0StructuresLookupTable(lookupTable, desc->pNext);

    if (parseResult != ZE_RESULT_SUCCESS) {
        return parseResult;
    }

    bool isMediaFormatLayout = isMediaFormat(desc->format.layout);

    imgInfo.imgDesc = lookupTable.imageProperties.imageDescriptor;

    imgInfo.surfaceFormat = &ImageFormats::formats[desc->format.layout][desc->format.type];
    imageFormatDesc = *const_cast<ze_image_desc_t *>(desc);

    UNRECOVERABLE_IF(device == nullptr);
    this->device = device;

    if (imgInfo.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_INVALID) {
        return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
    }

    typename RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType;
    switch (desc->type) {
    case ZE_IMAGE_TYPE_1D:
    case ZE_IMAGE_TYPE_1DARRAY:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
        break;
    case ZE_IMAGE_TYPE_2D:
    case ZE_IMAGE_TYPE_2DARRAY:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D;
        break;
    case ZE_IMAGE_TYPE_3D:
        surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D;
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    imgInfo.linearStorage = surfaceType == RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
    imgInfo.plane = lookupTable.imageProperties.isPlanarExtension ? static_cast<GMM_YUV_PLANE>(lookupTable.imageProperties.planeIndex + 1u) : GMM_NO_PLANE;
    imgInfo.useLocalMemory = false;

    if (lookupTable.bindlessImage && this->device->getNEODevice()->getBindlessHeapsHelper() == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    this->bindlessImage = lookupTable.bindlessImage;

    if (lookupTable.imageProperties.pitchedPtr) {
        if (!this->bindlessImage || isImageView()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        this->imageFromBuffer = true;
    }

    if (lookupTable.sampledImage) {
        if (!this->bindlessImage || lookupTable.imageProperties.samplerDesc == nullptr) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        this->sampledImage = true;
        this->samplerDesc = *lookupTable.imageProperties.samplerDesc;
        this->samplerDesc.pNext = nullptr;
    }

    NEO::UsmMemAllocPool *usmPool = nullptr;

    if (!isImageView()) {
        if (lookupTable.isSharedHandle) {
            if (!lookupTable.sharedHandleType.isSupportedHandle) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            if (lookupTable.sharedHandleType.isOpaqueFDHandle || lookupTable.sharedHandleType.isDMABUFHandle) {
                NEO::MemoryManager::OsHandleData osHandleData{static_cast<NEO::osHandle>(lookupTable.sharedHandleType.fd)};
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, &imgInfo, NEO::AllocationType::sharedImage, device->getNEODevice()->getDeviceBitfield());
                allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
                device->getNEODevice()->getMemoryManager()->closeSharedHandle(allocation);
            } else if (lookupTable.sharedHandleType.isNTHandle) {
                auto verifyResult = device->getNEODevice()->getMemoryManager()->verifyHandle(NEO::toOsHandle(lookupTable.sharedHandleType.ntHandle), device->getNEODevice()->getRootDeviceIndex(), true);
                if (!verifyResult) {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }
                NEO::MemoryManager::OsHandleData osHandleData{lookupTable.sharedHandleType.ntHandle};
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, &imgInfo, NEO::AllocationType::sharedImage, device->getNEODevice()->getDeviceBitfield());
                allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
                allocation->getDefaultGmm()->queryImageParams(imgInfo);
            }
        } else {

            if (!this->imageFromBuffer) {
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, &imgInfo, NEO::AllocationType::image, device->getNEODevice()->getDeviceBitfield());

                properties.flags.preferCompressed = isSuitableForCompression(lookupTable, imgInfo);

                allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
            } else {

                auto usmAllocation = this->device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(lookupTable.imageProperties.pitchedPtr);
                if (usmAllocation == nullptr) {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }

                if (this->device->getNEODevice()->getUsmMemAllocPool() &&
                    this->device->getNEODevice()->getUsmMemAllocPool()->isInPool(lookupTable.imageProperties.pitchedPtr)) {
                    usmPool = this->device->getNEODevice()->getUsmMemAllocPool();
                    if (nullptr == usmPool->getPooledAllocationBasePtr(lookupTable.imageProperties.pitchedPtr)) {
                        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                    }
                }

                allocation = usmAllocation->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
            }
        }
        if (allocation == nullptr) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
    }

    if (this->imageFromBuffer) {
        imgInfo.linearStorage = true;
        imgInfo.imgDesc.imageRowPitch = getRowPitchFor2dImage(device, imgInfo);

        if (imgInfo.imgDesc.imageRowPitch > 0) {
            imgInfo.rowPitch = imgInfo.imgDesc.imageRowPitch;
        } else {
            imgInfo.rowPitch = imgInfo.imgDesc.imageWidth * imgInfo.surfaceFormat->imageElementSizeInBytes;
        }
        imgInfo.slicePitch = imgInfo.rowPitch * imgInfo.imgDesc.imageHeight;
        imgInfo.qPitch = 0;
        if (!isImageView()) {
            imgInfo.size = allocation->getUnderlyingBufferSize();
            if (usmPool) {
                imgInfo.size = usmPool->getPooledAllocationSize(lookupTable.imageProperties.pitchedPtr);
                imgInfo.offset = usmPool->getOffsetInPool(lookupTable.imageProperties.pitchedPtr);
            }
        }
    }

    if (this->bindlessImage) {
        auto result = allocateBindlessSlot();
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        NEO::AllocationProperties imgImplicitArgsAllocProperties(device->getRootDeviceIndex(), NEO::ImageImplicitArgs::getSize(), NEO::AllocationType::buffer, device->getNEODevice()->getDeviceBitfield());
        implicitArgsAllocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(imgImplicitArgsAllocProperties);
    }

    auto gmm = this->allocation->getDefaultGmm();
    auto gmmHelper = static_cast<const NEO::RootDeviceEnvironment &>(rootDeviceEnvironment).getGmmHelper();

    if (!this->imageFromBuffer && gmm != nullptr) {
        NEO::ImagePlane yuvPlaneType = NEO::ImagePlane::noPlane;
        if (isImageView() && (sourceImageFormatDesc->format.layout == ZE_IMAGE_FORMAT_LAYOUT_NV12)) {
            yuvPlaneType = NEO::ImagePlane::planeY;
            if (imgInfo.plane == GMM_PLANE_U) {
                yuvPlaneType = NEO::ImagePlane::planeUV;
            }
        }
        gmm->updateImgInfoAndDesc(imgInfo, 0u, yuvPlaneType);
    }
    NEO::SurfaceOffsets surfaceOffsets = {imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane};

    {
        surfaceState = GfxFamily::cmdInitRenderSurfaceState;
        uint32_t minArrayElement, renderTargetViewExtent, depth;
        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceState(&surfaceState, imgInfo, gmm, *gmmHelper, __GMM_NO_CUBE_MAP,
                                                                      this->allocation->getGpuAddress(), surfaceOffsets,
                                                                      isMediaFormatLayout, minArrayElement, renderTargetViewExtent);

        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceStateDimensions(&surfaceState, imgInfo, __GMM_NO_CUBE_MAP, surfaceType, depth);
        surfaceState.setSurfaceMinLOD(0u);
        surfaceState.setMIPCountLOD(0u);
        NEO::ImageSurfaceStateHelper<GfxFamily>::setMipTailStartLOD(&surfaceState, gmm);

        if (!isMediaFormatLayout) {
            surfaceState.setShaderChannelSelectRed(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.x]));
            surfaceState.setShaderChannelSelectGreen(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.y]));
            surfaceState.setShaderChannelSelectBlue(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.z]));
            surfaceState.setShaderChannelSelectAlpha(
                static_cast<const typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(
                    shaderChannelSelect[desc->format.w]));
        } else {
            surfaceState.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            surfaceState.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
            surfaceState.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
            surfaceState.setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        }

        surfaceState.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);

        if (allocation->isCompressionEnabled()) {
            NEO::EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(&surfaceState, gmm);
        }
    }

    if (this->bindlessImage) {
        auto ssInHeap = getBindlessSlot();
        copySurfaceStateToSSH(ssInHeap->ssPtr, 0u, NEO::BindlessImageSlot::image, false);

        if (this->sampledImage) {
            auto productFamily = this->device->getNEODevice()->getHardwareInfo().platform.eProductFamily;
            auto sampler = Sampler::create(productFamily, device, &this->samplerDesc);
            if (!sampler) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
            auto &gfxCoreHelper = this->device->getGfxCoreHelper();
            auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
            auto samplerStateOffset = static_cast<uint32_t>(NEO::BindlessImageSlot::sampler * surfaceStateSize);

            sampler->copySamplerStateToDSH(ssInHeap->ssPtr, static_cast<uint32_t>(ssInHeap->ssSize), samplerStateOffset);
            sampler->destroy();
        }
    }

    const auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();
    if (this->bindlessImage && implicitArgsAllocation) {
        implicitArgsSurfaceState = GfxFamily::cmdInitRenderSurfaceState;

        auto clChannelType = getClChannelDataType(imageFormatDesc.format);
        auto clChannelOrder = getClChannelOrder(imageFormatDesc.format);

        NEO::ImageImplicitArgs imageImplicitArgs{};
        imageImplicitArgs.structVersion = 0;

        imageImplicitArgs.imageWidth = imgInfo.imgDesc.imageWidth;
        imageImplicitArgs.imageHeight = imgInfo.imgDesc.imageHeight;
        imageImplicitArgs.imageDepth = imgInfo.imgDesc.imageDepth;
        imageImplicitArgs.imageArraySize = imgInfo.imgDesc.imageArraySize;
        imageImplicitArgs.numSamples = imgInfo.imgDesc.numSamples;
        imageImplicitArgs.channelType = clChannelType;
        imageImplicitArgs.channelOrder = clChannelOrder;
        imageImplicitArgs.numMipLevels = imgInfo.imgDesc.numMipLevels;

        auto pixelSize = imgInfo.surfaceFormat->imageElementSizeInBytes;
        imageImplicitArgs.flatBaseOffset = implicitArgsAllocation->getGpuAddress();
        imageImplicitArgs.flatWidth = (imgInfo.imgDesc.imageWidth * pixelSize) - 1u;
        imageImplicitArgs.flagHeight = (imgInfo.imgDesc.imageHeight * pixelSize) - 1u;
        imageImplicitArgs.flatPitch = imgInfo.imgDesc.imageRowPitch - 1u;

        NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *implicitArgsAllocation), *this->device->getNEODevice(), implicitArgsAllocation, 0u, &imageImplicitArgs, NEO::ImageImplicitArgs::getSize());

        {
            auto &gfxCoreHelper = this->device->getGfxCoreHelper();

            NEO::EncodeSurfaceStateArgs args;
            args.outMemory = &implicitArgsSurfaceState;
            args.size = NEO::ImageImplicitArgs::getSize();
            args.graphicsAddress = implicitArgsAllocation->getGpuAddress();
            args.gmmHelper = gmmHelper;
            args.allocation = implicitArgsAllocation;
            args.numAvailableDevices = this->device->getNEODevice()->getNumGenericSubDevices();
            args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
            args.mocs = gfxCoreHelper.getMocsIndex(*args.gmmHelper, true, false) << 1;
            args.implicitScaling = this->device->isImplicitScalingCapable();
            args.isDebuggerActive = this->device->getNEODevice()->getDebugger() != nullptr;

            gfxCoreHelper.encodeBufferSurfaceState(args);
        }
    }

    {

        NEO::ImageInfo imgInfoRedescirebed;
        [[maybe_unused]] uint32_t exponent;
        switch (imgInfo.surfaceFormat->imageElementSizeInBytes) {
        default:
            exponent = Math::log2(imgInfo.surfaceFormat->imageElementSizeInBytes);
            DEBUG_BREAK_IF(exponent >= 5u);
            imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[exponent % 5];
            break;
        case 3:
            imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[5];
            break;
        case 6:
            imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[6];
            break;
        }

        imgInfoRedescirebed.imgDesc = imgInfo.imgDesc;
        imgInfoRedescirebed.qPitch = imgInfo.qPitch;
        redescribedSurfaceState = GfxFamily::cmdInitRenderSurfaceState;

        uint32_t minArrayElement, renderTargetViewExtent, depth;
        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceState(&redescribedSurfaceState, imgInfoRedescirebed, gmm, *gmmHelper,
                                                                      __GMM_NO_CUBE_MAP, this->allocation->getGpuAddress(), surfaceOffsets,
                                                                      desc->format.layout == ZE_IMAGE_FORMAT_LAYOUT_NV12, minArrayElement, renderTargetViewExtent);

        NEO::ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceStateDimensions(&redescribedSurfaceState, imgInfoRedescirebed, __GMM_NO_CUBE_MAP, surfaceType, depth);
        redescribedSurfaceState.setSurfaceMinLOD(0u);
        redescribedSurfaceState.setMIPCountLOD(0u);
        NEO::ImageSurfaceStateHelper<GfxFamily>::setMipTailStartLOD(&redescribedSurfaceState, gmm);

        if (imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R8_UINT_TYPE ||
            imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R16_UINT_TYPE ||
            imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R32_UINT_TYPE) {
            redescribedSurfaceState.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            redescribedSurfaceState.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
            redescribedSurfaceState.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        } else if (imgInfoRedescirebed.surfaceFormat->gmmSurfaceFormat == GMM_FORMAT_R32G32_UINT_TYPE) {
            redescribedSurfaceState.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            redescribedSurfaceState.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
            redescribedSurfaceState.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        } else {
            redescribedSurfaceState.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            redescribedSurfaceState.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN);
            redescribedSurfaceState.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE);
        }

        redescribedSurfaceState.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);

        if (allocation->isCompressionEnabled()) {
            NEO::EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(&redescribedSurfaceState, gmm);
        }
    }

    if (productHelper.isPackedCopyFormatSupported()) {
        packedSurfaceState = redescribedSurfaceState;

        NEO::EncodeSurfaceState<GfxFamily>::convertSurfaceStateToPacked(&packedSurfaceState, imgInfo);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::copySurfaceStateToSSH(void *surfaceStateHeap,
                                                           uint32_t surfaceStateOffset,
                                                           uint32_t bindlessSlot,
                                                           bool isMediaBlockArg) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    const RENDER_SURFACE_STATE *src = nullptr;

    switch (bindlessSlot) {
    case NEO::BindlessImageSlot::image:
        src = &surfaceState;
        break;
    case NEO::BindlessImageSlot::redescribedImage:
        src = &redescribedSurfaceState;
        break;
    case NEO::BindlessImageSlot::implicitArgs:
        src = &implicitArgsSurfaceState;
        break;
    case NEO::BindlessImageSlot::packedImage:
        src = &packedSurfaceState;
        break;
    default:
        UNRECOVERABLE_IF(true);
    }

    auto dst = ptrOffset(surfaceStateHeap, surfaceStateOffset);
    memcpy_s(dst, sizeof(RENDER_SURFACE_STATE), src, sizeof(RENDER_SURFACE_STATE));

    if (isMediaBlockArg) {
        RENDER_SURFACE_STATE *dstRss = static_cast<RENDER_SURFACE_STATE *>(dst);
        NEO::ImageSurfaceStateHelper<GfxFamily>::setWidthForMediaBlockSurfaceState(dstRss, imgInfo);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool ImageCoreFamily<gfxCoreFamily>::isSuitableForCompression(const StructuresLookupTable &structuresLookupTable, const NEO::ImageInfo &imgInfo) {
    auto &hwInfo = device->getHwInfo();
    auto &loGfxCoreHelper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    if (structuresLookupTable.uncompressedHint) {
        return false;
    }

    return (loGfxCoreHelper.imageCompressionSupported(hwInfo) && !imgInfo.linearStorage);
}

} // namespace L0
