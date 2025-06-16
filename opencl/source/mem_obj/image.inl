/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/image_helper.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/release_helper/release_helper.h"

#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"

#include "image_ext.inl"

namespace NEO {

template <typename GfxFamily>
void ImageHw<GfxFamily>::setImageArg(void *memory, bool setAsMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex) {
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(memory);

    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    auto gmm = graphicsAllocation->getDefaultGmm();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();

    auto imageDescriptor = Image::convertDescriptor(getImageDesc());
    ImageInfo imgInfo;
    imgInfo.imgDesc = imageDescriptor;
    imgInfo.qPitch = qPitch;
    imgInfo.surfaceFormat = &getSurfaceFormatInfo().surfaceFormat;

    uint32_t renderTargetViewExtent = 0;
    uint32_t minArrayElement = 0;

    ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceState(surfaceState, imgInfo, graphicsAllocation->getDefaultGmm(), *gmmHelper, cubeFaceIndex, graphicsAllocation->getGpuAddress(), surfaceOffsets, isNV12Image(&this->getImageFormat()), minArrayElement, renderTargetViewExtent);

    uint32_t depth = 0;

    if (getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        // image1d_buffer is image1d created from buffer. The length of buffer could be larger
        // than the maximal image width. Mock image1d_buffer with SURFACE_TYPE_SURFTYPE_BUFFER.
        SurfaceStateBufferLength length = {0};
        length.length = static_cast<uint32_t>(getImageDesc().image_width - 1);

        depth = static_cast<uint32_t>(length.surfaceState.depth + 1);
        surfaceState->setWidth(static_cast<uint32_t>(length.surfaceState.width + 1));
        surfaceState->setHeight(static_cast<uint32_t>(length.surfaceState.height + 1));
        surfaceState->setDepth(depth);
        surfaceState->setSurfacePitch(static_cast<uint32_t>(getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes));
        surfaceState->setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER);
    } else {
        ImageSurfaceStateHelper<GfxFamily>::setImageSurfaceStateDimensions(surfaceState, imgInfo, cubeFaceIndex, surfaceType, depth);
        if (setAsMediaBlockImage) {
            ImageSurfaceStateHelper<GfxFamily>::setWidthForMediaBlockSurfaceState(surfaceState, imgInfo);
        }
    }

    uint32_t mipCount = this->mipCount > 0 ? this->mipCount - 1 : 0;
    surfaceState->setSurfaceMinLOD(this->baseMipLevel + mipLevel);
    surfaceState->setMIPCountLOD(mipCount);
    ImageSurfaceStateHelper<GfxFamily>::setMipTailStartLOD(surfaceState, gmm);

    cl_channel_order imgChannelOrder = getSurfaceFormatInfo().oclImageFormat.image_channel_order;
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

    if (imgChannelOrder == CL_DEPTH) {
        surfaceState->setShaderChannelSelectAlpha(RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    }

    surfaceState->setNumberOfMultisamples(static_cast<typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES>(mcsSurfaceInfo.multisampleCount));

    if (imageDesc.num_samples > 1) {
        setAuxParamsForMultisamples(surfaceState, rootDeviceIndex);
    } else if (graphicsAllocation->isCompressionEnabled()) {
        EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(surfaceState, gmm);
    } else {
        EncodeSurfaceState<GfxFamily>::disableCompressionFlags(surfaceState);
    }
    appendSurfaceStateDepthParams(surfaceState, gmm);
    EncodeSurfaceState<GfxFamily>::appendImageCompressionParams(surfaceState, graphicsAllocation, gmmHelper, isImageFromBuffer(),
                                                                this->plane);

    adjustDepthLimitations(surfaceState, minArrayElement, renderTargetViewExtent, depth, mipCount, is3DUAVOrRTV);
    appendSurfaceStateParams(surfaceState, rootDeviceIndex);
    appendSurfaceStateExt(surfaceState);

    if (isPackedFormat) {
        NEO::EncodeSurfaceState<GfxFamily>::convertSurfaceStateToPacked(surfaceState, imgInfo);
    }
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setAuxParamsForMultisamples(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex) {
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    if (getMcsAllocation()) {
        auto mcsGmm = getMcsAllocation()->getDefaultGmm();

        if (EncodeSurfaceState<GfxFamily>::shouldProgramAuxForMcs(mcsGmm->unifiedAuxTranslationCapable(), mcsGmm->hasMultisampleControlSurface())) {
            auto *releaseHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
            DEBUG_BREAK_IF(releaseHelper == nullptr);
            EncodeSurfaceState<GfxFamily>::setAuxParamsForMCSCCS(surfaceState, releaseHelper);
            surfaceState->setAuxiliarySurfacePitch(mcsGmm->getUnifiedAuxPitchTiles());
            surfaceState->setAuxiliarySurfaceQPitch(mcsGmm->getAuxQPitch());
            EncodeSurfaceState<GfxFamily>::setClearColorParams(surfaceState, mcsGmm);
            ImageSurfaceStateHelper<GfxFamily>::setUnifiedAuxBaseAddress(surfaceState, mcsGmm);
        } else if (mcsGmm->unifiedAuxTranslationCapable()) {
            EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(surfaceState, mcsGmm);
        } else {
            surfaceState->setAuxiliarySurfaceMode(static_cast<typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE>(1));
            surfaceState->setAuxiliarySurfacePitch(mcsSurfaceInfo.pitch);
            surfaceState->setAuxiliarySurfaceQPitch(mcsSurfaceInfo.qPitch);
            surfaceState->setAuxiliarySurfaceBaseAddress(mcsAllocation->getGpuAddress());
        }
    } else if (isDepthFormat(imageFormat) && surfaceState->getSurfaceFormat() != SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS) {
        surfaceState->setMultisampledSurfaceStorageFormat(RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
    }
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::appendSurfaceStateParams(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex) {
}

template <typename GfxFamily>
inline void ImageHw<GfxFamily>::appendSurfaceStateDepthParams(RENDER_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setMediaImageArg(void *memory, uint32_t rootDeviceIndex) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;
    SURFACE_FORMAT surfaceFormat = MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y8_UNORM_VA;

    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);
    MEDIA_SURFACE_STATE state = GfxFamily::cmdInitMediaSurfaceState;

    setMediaSurfaceRotation(reinterpret_cast<void *>(&state));

    DEBUG_BREAK_IF(surfaceFormat == MEDIA_SURFACE_STATE::SURFACE_FORMAT_Y1_UNORM);
    state.setWidth(static_cast<uint32_t>(getImageDesc().image_width));

    state.setHeight(static_cast<uint32_t>(getImageDesc().image_height));
    state.setPictureStructure(MEDIA_SURFACE_STATE::PICTURE_STRUCTURE_FRAME_PICTURE);

    auto gmm = graphicsAllocation->getDefaultGmm();
    auto tileMode = static_cast<typename MEDIA_SURFACE_STATE::TILE_MODE>(gmm->gmmResourceInfo->getTileModeSurfaceState());

    state.setTileMode(tileMode);
    state.setSurfacePitch(static_cast<uint32_t>(getImageDesc().image_row_pitch));

    state.setSurfaceFormat(surfaceFormat);

    state.setHalfPitchForChroma(false);
    state.setInterleaveChroma(false);
    state.setXOffsetForUCb(0);
    state.setYOffsetForUCb(0);
    state.setXOffsetForVCr(0);
    state.setYOffsetForVCr(0);

    setSurfaceMemoryObjectControlState(
        reinterpret_cast<void *>(&state),
        gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));

    if (isNV12Image(&this->getImageFormat())) {
        state.setInterleaveChroma(true);
        state.setYOffsetForUCb(this->surfaceOffsets.yOffsetForUVplane);
    }

    state.setVerticalLineStride(0);
    state.setVerticalLineStrideOffset(0);

    state.setSurfaceBaseAddress(graphicsAllocation->getGpuAddress() + this->surfaceOffsets.offset);

    *surfaceState = state;
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
void ImageHw<GfxFamily>::adjustDepthLimitations(RENDER_SURFACE_STATE *surfaceState, uint32_t minArrayElement, uint32_t renderTargetViewExtent, uint32_t depth, uint32_t mipCount, bool is3DUavOrRtv) {
}

template <typename GfxFamily>
inline void ImageHw<GfxFamily>::setMediaSurfaceRotation(void *memory) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setRotation(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE);
    surfaceState->setXOffset(0);
    surfaceState->setYOffset(0);
}

template <typename GfxFamily>
inline void ImageHw<GfxFamily>::setSurfaceMemoryObjectControlState(void *memory, uint32_t value) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setSurfaceMemoryObjectControlState(value);
}

} // namespace NEO
