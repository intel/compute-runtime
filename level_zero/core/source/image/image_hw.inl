/*
 * Copyright (C) 2020-2021 Intel Corporation
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

#include "level_zero/core/source/image/image_formats.h"
#include "level_zero/core/source/image/image_hw.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t ImageCoreFamily<gfxCoreFamily>::initialize(Device *device, const ze_image_desc_t *desc) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    bool isMediaFormatLayout = isMediaFormat(desc->format.layout);

    auto imageDescriptor = convertDescriptor(*desc);
    imgInfo.imgDesc = imageDescriptor;

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
    imgInfo.plane = GMM_NO_PLANE;
    imgInfo.useLocalMemory = false;
    imgInfo.preferRenderCompression = false;

    if (desc->pNext) {
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC) {
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD) {
            const ze_external_memory_import_fd_t *externalMemoryImportDesc =
                reinterpret_cast<const ze_external_memory_import_fd_t *>(extendedDesc);
            if (externalMemoryImportDesc->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }

            NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, imgInfo, NEO::GraphicsAllocation::AllocationType::SHARED_IMAGE, device->getNEODevice()->getDeviceBitfield());

            allocation = device->getNEODevice()->getMemoryManager()->createGraphicsAllocationFromSharedHandle(externalMemoryImportDesc->fd, properties, false);
            device->getNEODevice()->getMemoryManager()->closeSharedHandle(allocation);
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        }
    } else {
        NEO::AllocationProperties properties(device->getRootDeviceIndex(), true, imgInfo, NEO::GraphicsAllocation::AllocationType::IMAGE, device->getNEODevice()->getDeviceBitfield());

        allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }
    UNRECOVERABLE_IF(allocation == nullptr);
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

        if (gmm && gmm->isRenderCompressed) {
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

        if (gmm && gmm->isRenderCompressed) {
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
        uint32_t elSize = static_cast<uint32_t>(imgInfo.surfaceFormat->ImageElementSizeInBytes);
        dstRss->setWidth(static_cast<uint32_t>((imgInfo.imgDesc.imageWidth * elSize) / sizeof(uint32_t)));
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

} // namespace L0
