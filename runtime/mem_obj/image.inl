/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/gmm_helper/resource_info.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/hw_cmds.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/mem_obj/image.h"

#include "image_ext.inl"

namespace NEO {

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : BITFIELD_RANGE(0, 6);
        uint32_t Height : BITFIELD_RANGE(7, 20);
        uint32_t Depth : BITFIELD_RANGE(21, 31);
    } SurfaceState;
};

template <typename GfxFamily>
void ImageHw<GfxFamily>::setImageArg(void *memory, bool setAsMediaBlockImage, uint32_t mipLevel) {
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(memory);
    auto gmm = getGraphicsAllocation()->getDefaultGmm();
    auto gmmHelper = executionEnvironment->getGmmHelper();

    auto imageCount = std::max(getImageDesc().image_depth, getImageDesc().image_array_size);
    if (imageCount == 0) {
        imageCount = 1;
    }

    bool isImageArray = getImageDesc().image_array_size > 1 &&
                        (getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
                         getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY);

    uint32_t renderTargetViewExtent = static_cast<uint32_t>(imageCount);
    uint32_t minimumArrayElement = 0;
    auto hAlign = RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4;
    auto vAlign = RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4;

    if (gmm) {
        hAlign = static_cast<typename RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT>(gmm->gmmResourceInfo->getHAlignSurfaceState());
        vAlign = static_cast<typename RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT>(gmm->gmmResourceInfo->getVAlignSurfaceState());
    }

    if (cubeFaceIndex != __GMM_NO_CUBE_MAP) {
        isImageArray = true;
        imageCount = __GMM_MAX_CUBE_FACE - cubeFaceIndex;
        renderTargetViewExtent = 1;
        minimumArrayElement = cubeFaceIndex;
    }

    auto imageHeight = getImageDesc().image_height;
    if (imageHeight == 0) {
        imageHeight = 1;
    }

    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    surfaceState->setAuxiliarySurfacePitch(1u);
    surfaceState->setAuxiliarySurfaceQpitch(0u);
    surfaceState->setAuxiliarySurfaceBaseAddress(0u);

    if (getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        // image1d_buffer is image1d created from buffer. The length of buffer could be larger
        // than the maximal image width. Mock image1d_buffer with SURFACE_TYPE_SURFTYPE_BUFFER.
        SURFACE_STATE_BUFFER_LENGTH Length = {0};
        Length.Length = static_cast<uint32_t>(getImageDesc().image_width - 1);

        surfaceState->setWidth(static_cast<uint32_t>(Length.SurfaceState.Width + 1));
        surfaceState->setHeight(static_cast<uint32_t>(Length.SurfaceState.Height + 1));
        surfaceState->setDepth(static_cast<uint32_t>(Length.SurfaceState.Depth + 1));
        surfaceState->setSurfacePitch(static_cast<uint32_t>(getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes));
        surfaceState->setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER);
    } else {
        if (setAsMediaBlockImage) {
            uint32_t elSize = static_cast<uint32_t>(getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes);
            surfaceState->setWidth(static_cast<uint32_t>((getImageDesc().image_width * elSize) / sizeof(uint32_t)));
        } else {
            surfaceState->setWidth(static_cast<uint32_t>(getImageDesc().image_width));
        }
        surfaceState->setHeight(static_cast<uint32_t>(imageHeight));
        surfaceState->setDepth(static_cast<uint32_t>(imageCount));
        surfaceState->setSurfacePitch(static_cast<uint32_t>(getImageDesc().image_row_pitch));
        surfaceState->setSurfaceType(surfaceType);
    }

    surfaceState->setSurfaceBaseAddress(getGraphicsAllocation()->getGpuAddress() + this->surfaceOffsets.offset);
    surfaceState->setRenderTargetViewExtent(renderTargetViewExtent);
    surfaceState->setMinimumArrayElement(minimumArrayElement);
    surfaceState->setSurfaceMinLod(this->baseMipLevel + mipLevel);
    surfaceState->setMipCountLod((this->mipCount > 0) ? (this->mipCount - 1) : 0);
    setMipTailStartLod(surfaceState);

    // SurfaceQpitch is in rows but must be a multiple of VALIGN
    surfaceState->setSurfaceQpitch(qPitch);

    surfaceState->setSurfaceFormat(static_cast<SURFACE_FORMAT>(getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat));
    surfaceState->setSurfaceArray(isImageArray);

    cl_channel_order imgChannelOrder = getSurfaceFormatInfo().OCLImageFormat.image_channel_order;
    int shaderChannelValue = ImageHw<GfxFamily>::getShaderChannelValue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED, imgChannelOrder);
    surfaceState->setShaderChannelSelectRed(static_cast<typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(shaderChannelValue));

    if (imgChannelOrder == CL_LUMINANCE) {
        surfaceState->setShaderChannelSelectGreen(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
        surfaceState->setShaderChannelSelectBlue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_RED);
    } else {
        shaderChannelValue = ImageHw<GfxFamily>::getShaderChannelValue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_GREEN, imgChannelOrder);
        surfaceState->setShaderChannelSelectGreen(static_cast<typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(shaderChannelValue));
        shaderChannelValue = ImageHw<GfxFamily>::getShaderChannelValue(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_BLUE, imgChannelOrder);
        surfaceState->setShaderChannelSelectBlue(static_cast<typename RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT>(shaderChannelValue));
    }

    if (IsNV12Image(&this->getImageFormat())) {
        surfaceState->setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    } else {
        surfaceState->setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    }

    surfaceState->setSurfaceHorizontalAlignment(hAlign);
    surfaceState->setSurfaceVerticalAlignment(vAlign);

    uint32_t tileMode = gmm ? gmm->gmmResourceInfo->getTileModeSurfaceState()
                            : static_cast<uint32_t>(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);

    surfaceState->setTileMode(static_cast<typename RENDER_SURFACE_STATE::TILE_MODE>(tileMode));

    surfaceState->setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));

    surfaceState->setXOffset(this->surfaceOffsets.xOffset);
    surfaceState->setYOffset(this->surfaceOffsets.yOffset);

    surfaceState->setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);

    if (IsNV12Image(&this->getImageFormat())) {
        surfaceState->setYOffsetForUOrUvPlane(this->surfaceOffsets.yOffsetForUVplane);
        surfaceState->setXOffsetForUOrUvPlane(this->surfaceOffsets.xOffset);
    } else {
        surfaceState->setYOffsetForUOrUvPlane(0);
        surfaceState->setXOffsetForUOrUvPlane(0);
    }

    surfaceState->setNumberOfMultisamples((typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES)mcsSurfaceInfo.multisampleCount);
    surfaceState->setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);

    if (imageDesc.num_samples > 1) {
        setAuxParamsForMultisamples(surfaceState);
    } else if (gmm && gmm->isRenderCompressed) {
        setAuxParamsForCCS(surfaceState, gmm);
    }
    appendSurfaceStateParams(surfaceState);
    appendSurfaceStateExt(surfaceState);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setAuxParamsForMultisamples(RENDER_SURFACE_STATE *surfaceState) {
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    if (getMcsAllocation()) {
        auto mcsGmm = getMcsAllocation()->getDefaultGmm();

        if (mcsGmm->unifiedAuxTranslationCapable() && mcsGmm->hasMultisampleControlSurface()) {
            setAuxParamsForMCSCCS(surfaceState, mcsGmm);
            surfaceState->setAuxiliarySurfacePitch(mcsGmm->getUnifiedAuxPitchTiles());
            surfaceState->setAuxiliarySurfaceQpitch(mcsGmm->getAuxQPitch());
            setClearColorParams(surfaceState, mcsGmm);
            setUnifiedAuxBaseAddress(surfaceState, mcsGmm);
        } else if (mcsGmm->unifiedAuxTranslationCapable()) {
            setAuxParamsForCCS(surfaceState, mcsGmm);
        } else {
            surfaceState->setAuxiliarySurfaceMode((typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE)1);
            surfaceState->setAuxiliarySurfacePitch(mcsSurfaceInfo.pitch);
            surfaceState->setAuxiliarySurfaceQpitch(mcsSurfaceInfo.qPitch);
            surfaceState->setAuxiliarySurfaceBaseAddress(mcsAllocation->getGpuAddress());
        }
    } else if (isDepthFormat(imageFormat) && surfaceState->getSurfaceFormat() != SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS) {
        surfaceState->setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    }
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setAuxParamsForCCS(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
    surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    setFlagsForMediaCompression(surfaceState, gmm);

    setClearColorParams(surfaceState, gmm);
    setUnifiedAuxBaseAddress(surfaceState, gmm);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setUnifiedAuxBaseAddress(RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
    uint64_t baseAddress = surfaceState->getSurfaceBaseAddress() +
                           gmm->gmmResourceInfo->getUnifiedAuxSurfaceOffset(GMM_UNIFIED_AUX_TYPE::GMM_AUX_SURF);
    surfaceState->setAuxiliarySurfaceBaseAddress(baseAddress);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::appendSurfaceStateParams(RENDER_SURFACE_STATE *surfaceState) {
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setFlagsForMediaCompression(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setMediaImageArg(void *memory) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;
    SURFACE_FORMAT surfaceFormat = MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM_VA;

    auto gmmHelper = executionEnvironment->getGmmHelper();
    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);
    *surfaceState = GfxFamily::cmdInitMediaSurfaceState;

    setMediaSurfaceRotation(reinterpret_cast<void *>(surfaceState));

    DEBUG_BREAK_IF(surfaceFormat == MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y1_UNORM);
    surfaceState->setWidth(static_cast<uint32_t>(getImageDesc().image_width));

    surfaceState->setHeight(static_cast<uint32_t>(getImageDesc().image_height));
    surfaceState->setPictureStructure(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE);

    auto gmm = getGraphicsAllocation()->getDefaultGmm();
    auto tileMode = static_cast<typename MEDIA_SURFACE_STATE::TILE_MODE>(gmm->gmmResourceInfo->getTileModeSurfaceState());

    surfaceState->setTileMode(tileMode);
    surfaceState->setSurfacePitch(static_cast<uint32_t>(getImageDesc().image_row_pitch));

    surfaceState->setSurfaceFormat(surfaceFormat);

    surfaceState->setHalfPitchForChroma(false);
    surfaceState->setInterleaveChroma(false);
    surfaceState->setXOffsetForUCb(0);
    surfaceState->setYOffsetForUCb(0);
    surfaceState->setXOffsetForVCr(0);
    surfaceState->setYOffsetForVCr(0);

    setSurfaceMemoryObjectControlStateIndexToMocsTable(
        reinterpret_cast<void *>(surfaceState),
        gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));

    if (IsNV12Image(&this->getImageFormat())) {
        surfaceState->setInterleaveChroma(true);
        surfaceState->setYOffsetForUCb(this->surfaceOffsets.yOffsetForUVplane);
    }

    surfaceState->setVerticalLineStride(0);
    surfaceState->setVerticalLineStrideOffset(0);

    surfaceState->setSurfaceBaseAddress(getGraphicsAllocation()->getGpuAddress() + this->surfaceOffsets.offset);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::transformImage2dArrayTo3d(void *memory) {
    DEBUG_BREAK_IF(imageDesc.image_type != CL_MEM_OBJECT_IMAGE3D);
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(memory);
    surfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
    surfaceState->setSurfaceArray(false);
}
template <typename GfxFamily>
void ImageHw<GfxFamily>::transformImage3dTo2dArray(void *memory) {
    DEBUG_BREAK_IF(imageDesc.image_type != CL_MEM_OBJECT_IMAGE3D);
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(memory);
    surfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    surfaceState->setSurfaceArray(true);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setClearColorParams(RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setAuxParamsForMCSCCS(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setMipTailStartLod(RENDER_SURFACE_STATE *surfaceState) {
    surfaceState->setMipTailStartLod(0);

    if (auto gmm = getGraphicsAllocation()->getDefaultGmm()) {
        surfaceState->setMipTailStartLod(gmm->gmmResourceInfo->getMipTailStartLodSurfaceState());
    }
}
} // namespace NEO
