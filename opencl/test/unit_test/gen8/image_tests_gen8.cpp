/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

typedef ::testing::Test gen8ImageTests;

GEN8TEST_F(gen8ImageTests, WhenAppendingSurfaceStateParamsThenSurfaceStateDoesNotChange) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceStateAfter = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->appendSurfaceStateParams(&surfaceStateAfter, context.getDevice(0)->getRootDeviceIndex(), false);

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}
