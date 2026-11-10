/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/xe3_core/hw_cmds_base.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/image/image_surface_state_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

using ImageSurfaceStateTestsXe3Core = ImageSurfaceStateTests;

XE3_CORETEST_F(ImageSurfaceStateTestsXe3Core, givenGmmWithMediaCompressedWhenSetMipTailStartLodThenMipTailStartLodIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    ImageSurfaceStateHelper<FamilyType>::setMipTailStartLOD(castSurfaceState, nullptr);

    EXPECT_EQ(castSurfaceState->getMipTailStartLOD(), 0u);

    ImageSurfaceStateHelper<FamilyType>::setMipTailStartLOD(castSurfaceState, mockGmm.get());

    EXPECT_EQ(castSurfaceState->getMipTailStartLOD(), mockGmm->gmmResourceInfo->getMipTailStartLODSurfaceState());
}
