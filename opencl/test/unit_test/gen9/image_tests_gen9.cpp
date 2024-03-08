/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

typedef ::testing::Test gen9ImageTests;

GEN9TEST_F(gen9ImageTests, appendSurfaceSWhenAppendingSurfaceStateParamsThenSurfaceStateDoesNotChangetateParamsDoesNothing) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceStateAfter = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->appendSurfaceStateParams(&surfaceStateAfter, context.getDevice(0)->getRootDeviceIndex());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}
