/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/compiler_support.h"

#include "level_zero/core/source/image/image_hw.h"

inline NEO::ImageType convertType(const ze_image_type_t type) {
    switch (type) {
    case ZE_IMAGE_TYPE_2D:
        return NEO::ImageType::Image2D;
    case ZE_IMAGE_TYPE_3D:
        return NEO::ImageType::Image3D;
    case ZE_IMAGE_TYPE_2DARRAY:
        return NEO::ImageType::Image2DArray;
    case ZE_IMAGE_TYPE_1D:
        return NEO::ImageType::Image1D;
    case ZE_IMAGE_TYPE_1DARRAY:
        return NEO::ImageType::Image1DArray;
    case ZE_IMAGE_TYPE_BUFFER:
        return NEO::ImageType::Image1DBuffer;
    default:
        break;
    }
    return NEO::ImageType::Invalid;
}

inline NEO::ImageDescriptor convertDescriptor(const ze_image_desc_t &imageDesc) {
    NEO::ImageDescriptor desc = {};
    desc.fromParent = false;
    desc.imageArraySize = imageDesc.arraylevels;
    desc.imageDepth = imageDesc.depth;
    desc.imageHeight = imageDesc.height;
    desc.imageRowPitch = 0u;
    desc.imageSlicePitch = 0u;
    desc.imageType = convertType(imageDesc.type);
    desc.imageWidth = imageDesc.width;
    desc.numMipLevels = imageDesc.miplevels;
    desc.numSamples = 0u;
    return desc;
}

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
bool ImageCoreFamily<gfxCoreFamily>::initialize(Device *device, const ze_image_desc_t *desc) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    if (desc == nullptr) {
        return false;
    }

    if (desc->format.layout > ZE_IMAGE_FORMAT_LAYOUT_MAX) {
        return false;
    }

    if (desc->format.type > ZE_IMAGE_FORMAT_TYPE_MAX) {
        return false;
    }

    if (desc->format.x > ZE_IMAGE_FORMAT_SWIZZLE_MAX ||
        desc->format.y > ZE_IMAGE_FORMAT_SWIZZLE_MAX ||
        desc->format.z > ZE_IMAGE_FORMAT_SWIZZLE_MAX ||
        desc->format.w > ZE_IMAGE_FORMAT_SWIZZLE_MAX) {
        return false;
    }

    if (desc->format.type > ZE_IMAGE_FORMAT_TYPE_MAX) {
        return false;
    }

    auto imageDescriptor = convertDescriptor(*desc);
    imgInfo.imgDesc = imageDescriptor;
    imgInfo.surfaceFormat = &surfaceFormatTable[desc->format.layout][desc->format.type];

    imageFormatDesc = *const_cast<ze_image_desc_t *>(desc);

    UNRECOVERABLE_IF(device == nullptr);
    this->device = device;

    if (desc == nullptr) {
        return false;
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
        return false;
    }

    imgInfo.linearStorage = surfaceType == RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D;
    imgInfo.plane = GMM_NO_PLANE;
    imgInfo.useLocalMemory = false;
    imgInfo.preferRenderCompression = false;

    NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, imgInfo, NEO::GraphicsAllocation::AllocationType::IMAGE);
    allocation = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    UNRECOVERABLE_IF(allocation == nullptr);

    auto gmm = this->allocation->getDefaultGmm();
    NEO::SurfaceOffsets surfaceOffsets = {imgInfo.offset, imgInfo.xOffset, imgInfo.yOffset, imgInfo.yOffsetForUVPlane};
    auto gmmHelper = static_cast<const NEO::RootDeviceEnvironment &>(device->getNEODevice()->getRootDeviceEnvironment()).getGmmHelper();

    if (gmm != nullptr) {
        gmm->updateImgInfoAndDesc(imgInfo, 0u);
    }

    {
        surfaceState = GfxFamily::cmdInitRenderSurfaceState;

        NEO::setImageSurfaceState<GfxFamily>(&surfaceState, imgInfo, gmm, *gmmHelper, __GMM_NO_CUBE_MAP,
                                             this->allocation->getGpuAddress(), surfaceOffsets,
                                             desc->format.layout == ZE_IMAGE_FORMAT_LAYOUT_NV12);

        NEO::setImageSurfaceStateDimensions<GfxFamily>(&surfaceState, imgInfo, __GMM_NO_CUBE_MAP, surfaceType);
        surfaceState.setSurfaceMinLod(0u);
        surfaceState.setMipCountLod(0u);
        NEO::setMipTailStartLod<GfxFamily>(&surfaceState, gmm);

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

        surfaceState.setNumberOfMultisamples(RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES::NUMBER_OF_MULTISAMPLES_MULTISAMPLECOUNT_1);

        if (gmm && gmm->isRenderCompressed) {
            NEO::setAuxParamsForCCS<GfxFamily>(&surfaceState, gmm);
        }
    }
    {
        const uint32_t exponent = Math::log2(imgInfo.surfaceFormat->NumChannels * imgInfo.surfaceFormat->PerChannelSizeInBytes);
        DEBUG_BREAK_IF(exponent >= 5u);

        NEO::ImageInfo imgInfoRedescirebed;
        imgInfoRedescirebed.surfaceFormat = &surfaceFormatsForRedescribe[exponent % 5];
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

        if (gmm && gmm->isRenderCompressed) {
            NEO::setAuxParamsForCCS<GfxFamily>(&redescribedSurfaceState, gmm);
        }
    }

    return true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void ImageCoreFamily<gfxCoreFamily>::copySurfaceStateToSSH(void *surfaceStateHeap,
                                                           const uint32_t surfaceStateOffset) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    // Copy the image's surface state into position in the provided surface state heap
    auto destSurfaceState = ptrOffset(surfaceStateHeap, surfaceStateOffset);
    memcpy_s(destSurfaceState, sizeof(RENDER_SURFACE_STATE),
             &surfaceState, sizeof(RENDER_SURFACE_STATE));
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

} // namespace L0
