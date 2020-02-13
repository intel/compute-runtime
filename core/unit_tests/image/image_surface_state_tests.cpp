/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/image/image_surface_state_fixture.h"

HWTEST_F(ImageSurfaceStateTests, givenImageInfoWhenSetImageSurfaceStateThenProperFieldsAreSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    const uint32_t cubeFaceIndex = 2u;

    imageInfo.qPitch = 1u;
    imageInfo.imgDesc.imageArraySize = 2u;
    imageInfo.imgDesc.imageDepth = 3u;
    imageInfo.imgDesc.imageHeight = 4u;
    imageInfo.imgDesc.imageRowPitch = 5u;
    imageInfo.imgDesc.imageSlicePitch = 6u;
    imageInfo.imgDesc.imageWidth = 7u;
    imageInfo.imgDesc.numMipLevels = 8u;
    imageInfo.imgDesc.numSamples = 9u;
    imageInfo.imgDesc.imageType = ImageType::Image2DArray;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.GenxSurfaceFormat = GFX3DSTATE_SURFACEFORMAT::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;
    SurfaceOffsets surfaceOffsets;
    surfaceOffsets.offset = 0u;
    surfaceOffsets.xOffset = 11u;
    surfaceOffsets.yOffset = 12u;
    surfaceOffsets.yOffsetForUVplane = 13u;

    const uint64_t gpuAddress = 0x000001a78a8a8000;

    setImageSurfaceState<FamilyType>(castSurfaceState, imageInfo, &mockGmm, *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    EXPECT_EQ(castSurfaceState->getRenderTargetViewExtent(), 1u);
    EXPECT_EQ(castSurfaceState->getMinimumArrayElement(), cubeFaceIndex);
    EXPECT_EQ(castSurfaceState->getSurfaceQpitch(), imageInfo.qPitch >> RENDER_SURFACE_STATE::tagSURFACEQPITCH::SURFACEQPITCH_BIT_SHIFT);
    EXPECT_EQ(castSurfaceState->getSurfaceArray(), true);
    EXPECT_EQ(castSurfaceState->getSurfaceHorizontalAlignment(), static_cast<typename RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT>(mockGmm.gmmResourceInfo->getHAlignSurfaceState()));
    EXPECT_EQ(castSurfaceState->getSurfaceVerticalAlignment(), static_cast<typename RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT>(mockGmm.gmmResourceInfo->getVAlignSurfaceState()));
    EXPECT_EQ(castSurfaceState->getTileMode(), mockGmm.gmmResourceInfo->getTileModeSurfaceState());
    EXPECT_EQ(castSurfaceState->getMemoryObjectControlState(), gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(castSurfaceState->getCoherencyType(), RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
    EXPECT_EQ(castSurfaceState->getMultisampledSurfaceStorageFormat(), RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);

    EXPECT_EQ(castSurfaceState->getSurfaceBaseAddress(), gpuAddress + surfaceOffsets.offset);

    uint32_t xOffset = surfaceOffsets.xOffset >> RENDER_SURFACE_STATE::tagXOFFSET::XOFFSET_BIT_SHIFT;
    uint32_t yOffset = surfaceOffsets.yOffset >> RENDER_SURFACE_STATE::tagYOFFSET::YOFFSET_BIT_SHIFT;

    EXPECT_EQ(castSurfaceState->getXOffset(), xOffset << RENDER_SURFACE_STATE::tagXOFFSET::XOFFSET_BIT_SHIFT);
    EXPECT_EQ(castSurfaceState->getYOffset(), yOffset << RENDER_SURFACE_STATE::tagYOFFSET::YOFFSET_BIT_SHIFT);

    EXPECT_EQ(castSurfaceState->getShaderChannelSelectAlpha(), RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    EXPECT_EQ(castSurfaceState->getYOffsetForUOrUvPlane(), surfaceOffsets.yOffsetForUVplane);
    EXPECT_EQ(castSurfaceState->getXOffsetForUOrUvPlane(), surfaceOffsets.xOffset);

    EXPECT_EQ(castSurfaceState->getSurfaceFormat(), static_cast<SURFACE_FORMAT>(imageInfo.surfaceFormat->GenxSurfaceFormat));

    surfaceState = std::make_unique<char[]>(size);
    castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    setImageSurfaceState<FamilyType>(castSurfaceState, imageInfo, nullptr, *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, false);

    EXPECT_EQ(castSurfaceState->getSurfaceHorizontalAlignment(), RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4);
    EXPECT_EQ(castSurfaceState->getSurfaceVerticalAlignment(), RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);

    EXPECT_EQ(castSurfaceState->getShaderChannelSelectAlpha(), RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    EXPECT_EQ(castSurfaceState->getYOffsetForUOrUvPlane(), 0u);
    EXPECT_EQ(castSurfaceState->getXOffsetForUOrUvPlane(), 0u);

    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfacePitch(), 1u);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceQpitch(), 0u);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceBaseAddress(), 0u);

    surfaceState = std::make_unique<char[]>(size);
    castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    typename FamilyType::RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D;

    setImageSurfaceStateDimensions<FamilyType>(castSurfaceState, imageInfo, cubeFaceIndex, surfaceType);

    EXPECT_EQ(castSurfaceState->getWidth(), static_cast<uint32_t>(imageInfo.imgDesc.imageWidth));
    EXPECT_EQ(castSurfaceState->getHeight(), static_cast<uint32_t>(imageInfo.imgDesc.imageHeight));
    EXPECT_EQ(castSurfaceState->getDepth(), __GMM_MAX_CUBE_FACE - cubeFaceIndex);
    EXPECT_EQ(castSurfaceState->getSurfacePitch(), static_cast<uint32_t>(imageInfo.imgDesc.imageRowPitch));
    EXPECT_EQ(castSurfaceState->getSurfaceType(), surfaceType);
}

HWTEST_F(ImageSurfaceStateTests, givenGmmWhenSetAuxParamsForCCSThenAuxiliarySurfaceModeIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    setAuxParamsForCCS<FamilyType>(castSurfaceState, &mockGmm);

    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}
