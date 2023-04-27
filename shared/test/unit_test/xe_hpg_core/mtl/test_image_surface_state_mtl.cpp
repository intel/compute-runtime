/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/image/image_surface_state_fixture.h"

using namespace NEO;

using ImageSurfaceStateTestsMtl = ImageSurfaceStateTests;

MTLTEST_F(ImageSurfaceStateTestsMtl, givenMtlWhenCallSetFilterModeThenDisallowLowQualityFliteringIsAllwaysFalse) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    setFilterMode<FamilyType>(castSurfaceState, &pDevice->getHardwareInfo());
    EXPECT_FALSE(castSurfaceState->getDisallowLowQualityFiltering());
}

MTLTEST_F(ImageSurfaceStateTestsMtl, givenMtlWhenCallSetImageSurfaceStateThenDisallowLowQualityFliteringIsAllwaysFalse) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    const uint32_t cubeFaceIndex = 2u;

    SurfaceFormatInfo surfaceFormatInfo;
    surfaceFormatInfo.genxSurfaceFormat = GFX3DSTATE_SURFACEFORMAT::GFX3DSTATE_SURFACEFORMAT_A32_FLOAT;
    imageInfo.surfaceFormat = &surfaceFormatInfo;
    SurfaceOffsets surfaceOffsets;
    surfaceOffsets.offset = 0u;
    surfaceOffsets.xOffset = 11u;
    surfaceOffsets.yOffset = 12u;
    surfaceOffsets.yOffsetForUVplane = 13u;

    const uint64_t gpuAddress = 0x000001a78a8a8000;

    setImageSurfaceState<FamilyType>(castSurfaceState, imageInfo, mockGmm.get(), *gmmHelper, cubeFaceIndex, gpuAddress, surfaceOffsets, true);
    EXPECT_FALSE(castSurfaceState->getDisallowLowQualityFiltering());
}
