/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/source/image/image_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t ImageCoreFamily<gfxCoreFamily>::initialize(Device *device, const ze_image_desc_t *desc) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

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

    if (imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_INVALID) {
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

    if (!isImageView) {
        if (lookupTable.isSharedHandle) {
            if (!lookupTable.sharedHandleType.isSupportedHandle) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            if (lookupTable.sharedHandleType.isDMABUFHandle) {
                NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, imgInfo, NEO::AllocationType::SHARED_IMAGE, device->getNEODevice()->getDeviceBitfield());
                allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromSharedHandle(lookupTable.sharedHandleType.fd, properties, false, false);
                device->getNEODevice()->getMemoryManager()->closeSharedHandle(allocation);
            } else if (lookupTable.sharedHandleType.isNTHandle) {
                auto verifyResult = device->getNEODevice()->getMemoryManager()->verifyHandle(NEO::toOsHandle(lookupTable.sharedHandleType.ntHnadle), device->getNEODevice()->getRootDeviceIndex(), true);
                if (!verifyResult) {
                    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
                }
                allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromNTHandle(lookupTable.sharedHandleType.ntHnadle, device->getNEODevice()->getRootDeviceIndex(), NEO::AllocationType::SHARED_IMAGE);
            }
        } else {
            NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, imgInfo, NEO::AllocationType::IMAGE, device->getNEODevice()->getDeviceBitfield());

            properties.flags.preferCompressed = isSuitableForCompression(lookupTable, imgInfo);

            allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        }
        if (allocation == nullptr) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
    }

    auto gmm = this->allocation->getDefaultGmm();
    auto gmmHelper = static_cast<const NEO::RootDeviceEnvironment &>(device->getNEODevice()->getRootDeviceEnvironment()).getGmmHelper();

    if (gmm != nullptr) {
        gmm->updateImgInfoAndDesc(imgInfo, 0u);
    }
    NEO::SurfaceOffsets surfaceOffsets = {imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane};

    {
        surfaceState = GfxFamily::cmdInitRenderSurfaceState;

        NEO::setImageSurfaceState<GfxFamily>(&surfaceState, imgInfo, gmm, *gmmHelper, __GMM_NO_CUBE_MAP,
                                             this->allocation->getGpuAddress(), surfaceOffsets,
                                             isMediaFormatLayout);

        NEO::setImageSurfaceStateDimensions<GfxFamily>(&surfaceState, imgInfo, __GMM_NO_CUBE_MAP, surfaceType);
        surfaceState.setSurfaceMinLod(0u);
        surfaceState.setMipCountLod(0u);
        NEO::setMipTailStartLod<GfxFamily>(&surfaceState, gmm);

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
    {
        const uint32_t exponent = Math::log2(imgInfo.surfaceFormat->ImageElementSizeInBytes);
        DEBUG_BREAK_IF(exponent >= 5u);

        NEO::ImageInfo imgInfoRedescirebed;
        imgInfoRedescirebed.surfaceFormat = &ImageFormats::surfaceFormatsForRedescribe[exponent % 5];
        imgInfoRedescirebed.imgDesc = imgInfo.imgDesc;
        imgInfoRedescirebed.qPitch = imgInfo.qPitch;
        redescribedSurfaceState = GfxFamily::cmdInitRenderSurfaceState;

        NEO::setImageSurfaceState<GfxFamily>(&redescribedSurfaceState, imgInfoRedescirebed, gmm, *gmmHelper,
                                             __GMM_NO_CUBE_MAP, this->allocation->getGpuAddress(), surfaceOffsets,
                                             desc->format.layout == ZE_IMAGE_FORMAT_LAYOUT_NV12);

        NEO::setImageSurfaceStateDimensions<GfxFamily>(&redescribedSurfaceState, imgInfoRedescirebed, __GMM_NO_CUBE_MAP, surfaceType);
        redescribedSurfaceState.setSurfaceMinLod(0u);
        redescribedSurfaceState.setMipCountLod(0u);
        NEO::setMipTailStartLod<GfxFamily>(&redescribedSurfaceState, gmm);

        if (imgInfoRedescirebed.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_R8_UINT_TYPE ||
            imgInfoRedescirebed.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_R16_UINT_TYPE ||
            imgInfoRedescirebed.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_R32_UINT_TYPE) {
            redescribedSurfaceState.setShaderChannelSelectRed(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
            redescribedSurfaceState.setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
            redescribedSurfaceState.setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ZERO);
        } else if (imgInfoRedescirebed.surfaceFormat->GMMSurfaceFormat == GMM_FORMAT_R32G32_UINT_TYPE) {
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

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::copySurfaceStateToSSH(void *surfaceStateHeap,
                                                           const uint32_t surfaceStateOffset,
                                                           bool isMediaBlockArg) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    // Copy the image's surface state into position in the provided surface state heap
    auto destSurfaceState = ptrOffset(surfaceStateHeap, surfaceStateOffset);
    memcpy_s(destSurfaceState, sizeof(RENDER_SURFACE_STATE),
             &surfaceState, sizeof(RENDER_SURFACE_STATE));
    if (isMediaBlockArg) {
        RENDER_SURFACE_STATE *dstRss = static_cast<RENDER_SURFACE_STATE *>(destSurfaceState);
        NEO::setWidthForMediaBlockSurfaceState<GfxFamily>(dstRss, imgInfo);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::copyRedescribedSurfaceStateToSSH(void *surfaceStateHeap,
                                                                      const uint32_t surfaceStateOffset) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    // Copy the image's surface state into position in the provided surface state heap
    auto destSurfaceState = ptrOffset(surfaceStateHeap, surfaceStateOffset);
    memcpy_s(destSurfaceState, sizeof(RENDER_SURFACE_STATE),
             &redescribedSurfaceState, sizeof(RENDER_SURFACE_STATE));
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool ImageCoreFamily<gfxCoreFamily>::isSuitableForCompression(const StructuresLookupTable &structuresLookupTable, const NEO::ImageInfo &imgInfo) {
    auto &hwInfo = device->getHwInfo();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (structuresLookupTable.uncompressedHint) {
        return false;
    }

    return (l0HwHelper.imageCompressionSupported(hwInfo) && !imgInfo.linearStorage);
}

} // namespace L0
