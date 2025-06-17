/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/image/image_surface_state_fixture.h"

using namespace NEO;

HWTEST2_F(ImageSurfaceStateTests, givenImageInfoWhenSetImageSurfaceStateThenProperFieldsAreSet, MatchAny) {
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
    imageInfo.imgDesc.imageType = ImageType::image2DArray;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = SurfaceFormat::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;
    SurfaceOffsets surfaceOffsets;
    surfaceOffsets.offset = 0u;
    surfaceOffsets.xOffset = 11u;
    surfaceOffsets.yOffset = 12u;
    surfaceOffsets.yOffsetForUVplane = 13u;

    const uint64_t gpuAddress = 0x000001a78a8a8000;

    uint32_t minArrayElement, renderTargetViewExtent;
    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true, minArrayElement, renderTargetViewExtent);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    EXPECT_EQ(castSurfaceState->getRenderTargetViewExtent(), 1u);
    EXPECT_EQ(castSurfaceState->getMinimumArrayElement(), cubeFaceIndex);
    EXPECT_EQ(castSurfaceState->getSurfaceQPitch(), imageInfo.qPitch >> RENDER_SURFACE_STATE::tagSURFACEQPITCH::SURFACEQPITCH_BIT_SHIFT);
    EXPECT_EQ(castSurfaceState->getSurfaceArray(), true);
    EXPECT_EQ(castSurfaceState->getSurfaceHorizontalAlignment(), static_cast<typename RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT>(mockGmm->gmmResourceInfo->getHAlignSurfaceState()));
    EXPECT_EQ(castSurfaceState->getSurfaceVerticalAlignment(), static_cast<typename RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT>(mockGmm->gmmResourceInfo->getVAlignSurfaceState()));
    EXPECT_EQ(castSurfaceState->getTileMode(), mockGmm->gmmResourceInfo->getTileModeSurfaceState());
    EXPECT_EQ(castSurfaceState->getMemoryObjectControlState(), gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    if constexpr (IsAtMostXeCore::isMatched<productFamily>()) {
        EXPECT_EQ(castSurfaceState->getCoherencyType(), RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
    }
    EXPECT_EQ(castSurfaceState->getMultisampledSurfaceStorageFormat(), RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);

    EXPECT_EQ(castSurfaceState->getSurfaceBaseAddress(), gpuAddress + surfaceOffsets.offset);

    uint32_t xOffset = surfaceOffsets.xOffset >> RENDER_SURFACE_STATE::tagXOFFSET::XOFFSET_BIT_SHIFT;
    uint32_t yOffset = surfaceOffsets.yOffset >> RENDER_SURFACE_STATE::tagYOFFSET::YOFFSET_BIT_SHIFT;

    EXPECT_EQ(castSurfaceState->getXOffset(), xOffset << RENDER_SURFACE_STATE::tagXOFFSET::XOFFSET_BIT_SHIFT);
    EXPECT_EQ(castSurfaceState->getYOffset(), yOffset << RENDER_SURFACE_STATE::tagYOFFSET::YOFFSET_BIT_SHIFT);

    EXPECT_EQ(castSurfaceState->getShaderChannelSelectAlpha(), RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ONE);
    EXPECT_EQ(castSurfaceState->getYOffsetForUOrUvPlane(), surfaceOffsets.yOffsetForUVplane);
    EXPECT_EQ(castSurfaceState->getXOffsetForUOrUvPlane(), surfaceOffsets.xOffset);

    EXPECT_EQ(castSurfaceState->getSurfaceFormat(), static_cast<SURFACE_FORMAT>(imageInfo.surfaceFormat->genxSurfaceFormat));

    surfaceState = std::make_unique<char[]>(size);
    castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, nullptr, *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, false, minArrayElement, renderTargetViewExtent);

    EXPECT_EQ(castSurfaceState->getSurfaceHorizontalAlignment(), RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT);
    EXPECT_EQ(castSurfaceState->getSurfaceVerticalAlignment(), RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);

    EXPECT_EQ(castSurfaceState->getShaderChannelSelectAlpha(), RENDER_SURFACE_STATE::SHADER_CHANNEL_SELECT_ALPHA);
    EXPECT_EQ(castSurfaceState->getYOffsetForUOrUvPlane(), 0u);
    EXPECT_EQ(castSurfaceState->getXOffsetForUOrUvPlane(), 0u);

    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfacePitch(), 1u);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceQPitch(), 0u);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceBaseAddress(), 0u);

    surfaceState = std::make_unique<char[]>(size);
    castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    typename FamilyType::RENDER_SURFACE_STATE::SURFACE_TYPE surfaceType = RENDER_SURFACE_STATE::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D;

    uint32_t depth;
    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceStateDimensions(castSurfaceState, imageInfo, cubeFaceIndex, surfaceType, depth);

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
    EncodeSurfaceState<FamilyType>::setImageAuxParamsForCCS(castSurfaceState, mockGmm.get());

    mockGmm->setCompressionEnabled(true);
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(castSurfaceState, mockGmm.get()));
}

HWTEST_F(ImageSurfaceStateTests, givenImage2DWhen2dImageWAIsEnabledThenArrayFlagIsSet) {
    DebugManagerStateRestore debugSettingsRestore;
    debugManager.flags.Force2dImageAsArray.set(1);
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    imageInfo.imgDesc.imageType = ImageType::image2D;
    imageInfo.imgDesc.imageDepth = 1;
    imageInfo.imgDesc.imageArraySize = 1;
    imageInfo.qPitch = 0;
    SurfaceOffsets surfaceOffsets = {0, 0, 0, 0};
    const uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = SurfaceFormat::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;

    const uint64_t gpuAddress = 0x000001a78a8a8000;

    uint32_t minArrayElement, renderTargetViewExtent;
    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true, minArrayElement, renderTargetViewExtent);
    EXPECT_TRUE(castSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageSurfaceStateTests, givenImage2DWhen2dImageWAIsDisabledThenArrayFlagIsNotSet) {
    DebugManagerStateRestore debugSettingsRestore;
    debugManager.flags.Force2dImageAsArray.set(0);
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    imageInfo.imgDesc.imageType = ImageType::image2D;
    imageInfo.imgDesc.imageDepth = 1;
    imageInfo.imgDesc.imageArraySize = 1;
    imageInfo.qPitch = 0;
    SurfaceOffsets surfaceOffsets = {0, 0, 0, 0};
    const uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = SurfaceFormat::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;

    const uint64_t gpuAddress = 0x000001a78a8a8000;
    uint32_t minArrayElement, renderTargetViewExtent;

    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true, minArrayElement, renderTargetViewExtent);
    EXPECT_FALSE(castSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageSurfaceStateTests, givenImage2DArrayOfSize1When2dImageWAIsEnabledThenArrayFlagIsSet) {
    DebugManagerStateRestore debugSettingsRestore;
    debugManager.flags.Force2dImageAsArray.set(1);
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    imageInfo.imgDesc.imageType = ImageType::image2DArray;
    imageInfo.imgDesc.imageDepth = 1;
    imageInfo.imgDesc.imageArraySize = 1;
    imageInfo.qPitch = 0;
    SurfaceOffsets surfaceOffsets = {0, 0, 0, 0};
    const uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = SurfaceFormat::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;

    const uint64_t gpuAddress = 0x000001a78a8a8000;
    uint32_t minArrayElement, renderTargetViewExtent;

    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true, minArrayElement, renderTargetViewExtent);
    EXPECT_TRUE(castSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageSurfaceStateTests, givenImage2DArrayOfSize1When2dImageWAIsDisabledThenArrayFlagIsNotSet) {
    DebugManagerStateRestore debugSettingsRestore;
    debugManager.flags.Force2dImageAsArray.set(0);
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    imageInfo.imgDesc.imageType = ImageType::image2DArray;
    imageInfo.imgDesc.imageDepth = 1;
    imageInfo.imgDesc.imageArraySize = 1;
    imageInfo.qPitch = 0;
    SurfaceOffsets surfaceOffsets = {0, 0, 0, 0};
    const uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = SurfaceFormat::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;

    const uint64_t gpuAddress = 0x000001a78a8a8000;
    uint32_t minArrayElement, renderTargetViewExtent;

    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true, minArrayElement, renderTargetViewExtent);

    if (ImageSurfaceStateHelper<FamilyType>::imageAsArrayWithArraySizeOf1NotPreferred()) {
        EXPECT_FALSE(castSurfaceState->getSurfaceArray());
    } else {
        EXPECT_TRUE(castSurfaceState->getSurfaceArray());
    }
}

HWTEST_F(ImageSurfaceStateTests, givenImage1DWhen2dImageWAIsEnabledThenArrayFlagIsNotSet) {
    DebugManagerStateRestore debugSettingsRestore;
    debugManager.flags.Force2dImageAsArray.set(1);
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    imageInfo.imgDesc.imageType = ImageType::image1D;
    imageInfo.imgDesc.imageDepth = 1;
    imageInfo.imgDesc.imageArraySize = 1;
    imageInfo.qPitch = 0;
    SurfaceOffsets surfaceOffsets = {0, 0, 0, 0};
    const uint32_t cubeFaceIndex = __GMM_NO_CUBE_MAP;
    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = SurfaceFormat::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;

    const uint64_t gpuAddress = 0x000001a78a8a8000;
    uint32_t minArrayElement, renderTargetViewExtent;

    ImageSurfaceStateHelper<FamilyType>::setImageSurfaceState(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true, minArrayElement, renderTargetViewExtent);
    EXPECT_FALSE(castSurfaceState->getSurfaceArray());
}

struct ImageWidthTest : ImageSurfaceStateTests {

    struct ImageWidthParams {
        uint32_t imageWidth;
        uint32_t expectedWidthInDwords;
    };

    template <typename FamilyType>
    void verifyProgramming(typename FamilyType::RENDER_SURFACE_STATE &renderSurfaceState, const std::array<ImageWidthParams, 6> &params) {
        for (auto &param : params) {
            imageInfo.imgDesc.imageWidth = param.imageWidth;
            ImageSurfaceStateHelper<FamilyType>::setWidthForMediaBlockSurfaceState(&renderSurfaceState, imageInfo);
            EXPECT_EQ(param.expectedWidthInDwords, renderSurfaceState.getWidth());
        }
    }
};

HWTEST_F(ImageWidthTest, givenMediaBlockWhenProgrammingWidthInSurfaceStateThenCorrectValueIsProgrammed) {
    SurfaceFormatInfo surfaceFormatInfo{};
    imageInfo.surfaceFormat = &surfaceFormatInfo;

    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    {
        surfaceFormatInfo.imageElementSizeInBytes = 1u;
        static constexpr std::array<ImageWidthParams, 6> params = {{
            {1, 1},
            {2, 1},
            {3, 1},
            {4, 1},
            {5, 2},
            {6, 2},
        }};
        verifyProgramming<FamilyType>(renderSurfaceState, params);
    }
    {
        surfaceFormatInfo.imageElementSizeInBytes = 2u;
        static constexpr std::array<ImageWidthParams, 6> params = {{
            {1, 1},
            {2, 1},
            {3, 2},
            {4, 2},
            {5, 3},
            {6, 3},
        }};
        verifyProgramming<FamilyType>(renderSurfaceState, params);
    }
    {
        surfaceFormatInfo.imageElementSizeInBytes = 4u;
        static constexpr std::array<ImageWidthParams, 6> params = {{
            {1, 1},
            {2, 2},
            {3, 3},
            {4, 4},
            {5, 5},
            {6, 6},
        }};
        verifyProgramming<FamilyType>(renderSurfaceState, params);
    }
}
