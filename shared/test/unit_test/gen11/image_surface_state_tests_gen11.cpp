/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/image/image_surface_state_fixture.h"

using ImageSurfaceStateTestsGen11 = ImageSurfaceStateTests;

GEN11TEST_F(ImageSurfaceStateTestsGen11, givenGmmWithMediaCompressedWhenSetFlagsForMediaCompressionThenAuxiliarySurfaceNoneIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    castSurfaceState->setAuxiliarySurfaceMode(FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);

    mockGmm.gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = false;
    setFlagsForMediaCompression<FamilyType>(castSurfaceState, &mockGmm);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    mockGmm.gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    setFlagsForMediaCompression<FamilyType>(castSurfaceState, &mockGmm);
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}

GEN11TEST_F(ImageSurfaceStateTestsGen11, givenGmmWithMediaCompressedWhenSetMipTailStartLodThenMipTailStartLodIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    setMipTailStartLod<FamilyType>(castSurfaceState, nullptr);

    EXPECT_EQ(castSurfaceState->getMipTailStartLod(), 0u);

    setMipTailStartLod<FamilyType>(castSurfaceState, &mockGmm);

    EXPECT_EQ(castSurfaceState->getMipTailStartLod(), mockGmm.gmmResourceInfo->getMipTailStartLodSurfaceState());
}
