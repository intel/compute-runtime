/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/image/image_surface_state_fixture.h"

using namespace NEO;

using ImageSurfaceStateTestsXe2HpgCore = ImageSurfaceStateTests;

XE2_HPG_CORETEST_F(ImageSurfaceStateTestsXe2HpgCore, givenGmmWithMediaCompressedWhenSetMipTailStartLodThenMipTailStartLodIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    ImageSurfaceStateHelper<FamilyType>::setMipTailStartLOD(castSurfaceState, nullptr);

    EXPECT_EQ(castSurfaceState->getMipTailStartLOD(), 0u);

    ImageSurfaceStateHelper<FamilyType>::setMipTailStartLOD(castSurfaceState, mockGmm.get());

    EXPECT_EQ(castSurfaceState->getMipTailStartLOD(), mockGmm->gmmResourceInfo->getMipTailStartLODSurfaceState());
}

XE2_HPG_CORETEST_F(ImageSurfaceStateTestsXe2HpgCore, givenNotMediaCompressedImageWhenAppendingSurfaceStateParamsForCompressionThenCallAppriopriateFunction) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE rss = {};

    mockGmm->setCompressionEnabled(true);

    MockGraphicsAllocation allocation;
    allocation.setDefaultGmm(mockGmm.get());

    auto gmmClientContext = static_cast<MockGmmClientContext *>(pDevice->getGmmHelper()->getClientContext());

    EncodeSurfaceState<FamilyType>::appendImageCompressionParams(&rss, &allocation, pDevice->getGmmHelper(), false, GMM_NO_PLANE);

    EXPECT_EQ(gmmClientContext->compressionFormatToReturn, rss.getCompressionFormat());
}
