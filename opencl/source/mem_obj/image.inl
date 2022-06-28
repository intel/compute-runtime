/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"

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
void ImageHw<GfxFamily>::setImageArg(void *memory, bool setAsMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex, bool useGlobalAtomics) {
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(memory);

    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    auto gmm = graphicsAllocation->getDefaultGmm();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();

    auto imageDescriptor = Image::convertDescriptor(getImageDesc());
    ImageInfo imgInfo;
    imgInfo.imgDesc = imageDescriptor;
    imgInfo.qPitch = qPitch;
    imgInfo.surfaceFormat = &getSurfaceFormatInfo().surfaceFormat;

    setImageSurfaceState<GfxFamily>(surfaceState, imgInfo, graphicsAllocation->getDefaultGmm(), *gmmHelper, cubeFaceIndex, graphicsAllocation->getGpuAddress(), surfaceOffsets, isNV12Image(&this->getImageFormat()));

    if (getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        // image1d_buffer is image1d created from buffer. The length of buffer could be larger
        // than the maximal image width. Mock image1d_buffer with SURFACE_TYPE_SURFTYPE_BUFFER.
        SURFACE_STATE_BUFFER_LENGTH length = {0};
        length.Length = static_cast<uint32_t>(getImageDesc().image_width - 1);

        surfaceState->setWidth(static_cast<uint32_t>(length.SurfaceState.Width + 1));
        surfaceState->setHeight(static_cast<uint32_t>(length.SurfaceState.Height + 1));
        surfaceState->setDepth(static_cast<uint32_t>(length.SurfaceState.Depth + 1));
        surfaceState->setSurfacePitch(static_cast<uint32_t>(getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes));
        surfaceState->setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER);
    } else {
        setImageSurfaceStateDimensions<GfxFamily>(surfaceState, imgInfo, cubeFaceIndex, surfaceType);
        if (setAsMediaBlockImage) {
            setWidthForMediaBlockSurfaceState<GfxFamily>(surfaceState, imgInfo);
        }
    }

    surfaceState->setSurfaceMinLod(this->baseMipLevel + mipLevel);
    surfaceState->setMipCountLod((this->mipCount > 0) ? (this->mipCount - 1) : 0);
    setMipTailStartLod<GfxFamily>(surfaceState, gmm);

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

    surfaceState->setNumberOfMultisamples((typename RENDER_SURFACE_STATE::NUMBER_OF_MULTISAMPLES)mcsSurfaceInfo.multisampleCount);

    if (imageDesc.num_samples > 1) {
        setAuxParamsForMultisamples(surfaceState);
    } else if (graphicsAllocation->isCompressionEnabled()) {
        EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(surfaceState, gmm);
    } else {
        EncodeSurfaceState<GfxFamily>::disableCompressionFlags(surfaceState);
    }
    appendSurfaceStateDepthParams(surfaceState, gmm);
    EncodeSurfaceState<GfxFamily>::appendImageCompressionParams(surfaceState, graphicsAllocation, gmmHelper, isImageFromBuffer(),
                                                                this->plane);
    appendSurfaceStateParams(surfaceState, rootDeviceIndex, useGlobalAtomics);
    appendSurfaceStateExt(surfaceState);
}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setAuxParamsForMultisamples(RENDER_SURFACE_STATE *surfaceState) {
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;

    if (getMcsAllocation()) {
        auto mcsGmm = getMcsAllocation()->getDefaultGmm();

        if (mcsGmm->unifiedAuxTranslationCapable() && mcsGmm->hasMultisampleControlSurface()) {
            EncodeSurfaceState<GfxFamily>::setAuxParamsForMCSCCS(surfaceState);
            surfaceState->setAuxiliarySurfacePitch(mcsGmm->getUnifiedAuxPitchTiles());
            surfaceState->setAuxiliarySurfaceQpitch(mcsGmm->getAuxQPitch());
            EncodeSurfaceState<GfxFamily>::setClearColorParams(surfaceState, mcsGmm);
            setUnifiedAuxBaseAddress<GfxFamily>(surfaceState, mcsGmm);
        } else if (mcsGmm->unifiedAuxTranslationCapable()) {
            EncodeSurfaceState<GfxFamily>::setImageAuxParamsForCCS(surfaceState, mcsGmm);
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
void ImageHw<GfxFamily>::appendSurfaceStateParams(RENDER_SURFACE_STATE *surfaceState, uint32_t rootDeviceIndex, bool useGlobalAtomics) {
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
inline void ImageHw<GfxFamily>::setMediaSurfaceRotation(void *memory) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setRotation(MEDIA_SURFACE_STATE::ROTATION_NO_ROTATION_OR_0_DEGREE);
    surfaceState->setXOffset(0);
    surfaceState->setYOffset(0);
}

template <typename GfxFamily>
inline void ImageHw<GfxFamily>::setSurfaceMemoryObjectControlState(void *memory, uint32_t value) {
    using MEDIA_SURFACE_STATE = typename GfxFamily::MEDIA_SURFACE_STATE;
    using SURFACE_FORMAT = typename MEDIA_SURFACE_STATE::SURFACE_FORMAT;

    auto surfaceState = reinterpret_cast<MEDIA_SURFACE_STATE *>(memory);

    surfaceState->setSurfaceMemoryObjectControlState(value);
}

} // namespace NEO
