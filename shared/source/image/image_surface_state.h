/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
template <typename GfxFamily>
void setFilterMode(typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo);

template <typename GfxFamily>
inline void setImageSurfaceState(typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const ImageInfo &imageInfo, Gmm *gmm, GmmHelper &gmmHelper, uint32_t cubeFaceIndex, uint64_t gpuAddress, const SurfaceOffsets &surfaceOffsets, bool isNV12Format) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto imageCount = std::max(imageInfo.imgDesc.imageDepth, imageInfo.imgDesc.imageArraySize);
    if (imageCount == 0) {
        imageCount = 1;
    }

    bool isImageArray = imageInfo.imgDesc.imageArraySize > 1 &&
                        (imageInfo.imgDesc.imageType == ImageType::Image2DArray ||
                         imageInfo.imgDesc.imageType == ImageType::Image1DArray);

    isImageArray |= (imageInfo.imgDesc.imageType == ImageType::Image2D || imageInfo.imgDesc.imageType == ImageType::Image2DArray) && DebugManager.flags.Force2dImageAsArray.get() == 1;

    uint32_t renderTargetViewExtent = static_cast<uint32_t>(imageCount);
    uint32_t minimumArrayElement = 0;
    auto hAlign = RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT;
    auto vAlign = RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4;

    if (gmm) {
        hAlign = static_cast<typename RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT>(gmm->gmmResourceInfo->getHAlignSurfaceState());
        vAlign = static_cast<typename RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT>(gmm->gmmResourceInfo->getVAlignSurfaceState());
    }

    if (cubeFaceIndex != __GMM_NO_CUBE_MAP) {
        isImageArray = true;
        renderTargetViewExtent = 1;
        minimumArrayElement = cubeFaceIndex;
    }

    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    surfaceState->setAuxiliarySurfacePitch(1u);
    surfaceState->setAuxiliarySurfaceQpitch(0u);
    surfaceState->setAuxiliarySurfaceBaseAddress(0u);
    surfaceState->setRenderTargetViewExtent(renderTargetViewExtent);
    surfaceState->setMinimumArrayElement(minimumArrayElement);

    // SurfaceQpitch is in rows but must be a multiple of VALIGN
    surfaceState->setSurfaceQpitch(imageInfo.qPitch);

    surfaceState->setSurfaceArray(isImageArray);

    surfaceState->setSurfaceHorizontalAlignment(hAlign);
    surfaceState->setSurfaceVerticalAlignment(vAlign);

    uint32_t tileMode = gmm ? gmm->gmmResourceInfo->getTileModeSurfaceState()
                            : static_cast<uint32_t>(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);

    surfaceState->setTileMode(static_cast<typename RENDER_SURFACE_STATE::TILE_MODE>(tileMode));

    surfaceState->setMemoryObjectControlState(gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));

    surfaceState->setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);

    surfaceState->setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);

    surfaceState->setSurfaceBaseAddress(gpuAddress + surfaceOffsets.offset);
    surfaceState->setXOffset(surfaceOffsets.xOffset);
    surfaceState->setYOffset(surfaceOffsets.yOffset);

    if (isNV12Format) {
        surfaceState->setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
        surfaceState->setYOffsetForUOrUvPlane(surfaceOffsets.yOffsetForUVplane);
        surfaceState->setXOffsetForUOrUvPlane(surfaceOffsets.xOffset);
    } else {
        surfaceState->setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
        surfaceState->setYOffsetForUOrUvPlane(0);
        surfaceState->setXOffsetForUOrUvPlane(0);
    }

    surfaceState->setSurfaceFormat(static_cast<SURFACE_FORMAT>(imageInfo.surfaceFormat->GenxSurfaceFormat));
    setFilterMode<GfxFamily>(surfaceState, gmmHelper.getHardwareInfo());
}

template <typename GfxFamily>
inline void setImageSurfaceStateDimensions(typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const ImageInfo &imageInfo, uint32_t cubeFaceIndex, typename GfxFamily::RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType) {
    auto imageCount = std::max(imageInfo.imgDesc.imageDepth, imageInfo.imgDesc.imageArraySize);
    if (imageCount == 0) {
        imageCount = 1;
    }

    auto imageHeight = imageInfo.imgDesc.imageHeight;
    if (imageHeight == 0) {
        imageHeight = 1;
    }

    auto imageWidth = imageInfo.imgDesc.imageWidth;
    if (imageWidth == 0) {
        imageWidth = 1;
    }

    if (cubeFaceIndex != __GMM_NO_CUBE_MAP) {
        imageCount = __GMM_MAX_CUBE_FACE - cubeFaceIndex;
    }

    surfaceState->setWidth(static_cast<uint32_t>(imageWidth));
    surfaceState->setHeight(static_cast<uint32_t>(imageHeight));
    surfaceState->setDepth(static_cast<uint32_t>(imageCount));
    surfaceState->setSurfacePitch(static_cast<uint32_t>(imageInfo.imgDesc.imageRowPitch));
    surfaceState->setSurfaceType(surfaceType);
}

template <typename GfxFamily>
inline void setWidthForMediaBlockSurfaceState(typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const ImageInfo &imageInfo) {
    auto elSize = imageInfo.surfaceFormat->ImageElementSizeInBytes;
    auto numDwords = static_cast<uint32_t>(Math::divideAndRoundUp(imageInfo.imgDesc.imageWidth * elSize, sizeof(uint32_t)));
    surfaceState->setWidth(numDwords);
}

template <typename GfxFamily>
inline void setUnifiedAuxBaseAddress(typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
    uint64_t baseAddress = surfaceState->getSurfaceBaseAddress() +
                           gmm->gmmResourceInfo->getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_SURF);
    surfaceState->setAuxiliarySurfaceBaseAddress(baseAddress);
}

template <typename GfxFamily>
void setMipTailStartLod(typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, Gmm *gmm);

} // namespace NEO
