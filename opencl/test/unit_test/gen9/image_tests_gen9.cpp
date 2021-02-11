/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

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

    imageHw->appendSurfaceStateParams(&surfaceStateAfter, context.getDevice(0)->getRootDeviceIndex(), false);

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}

using Gen9RenderSurfaceStateDataTests = ::testing::Test;

GEN9TEST_F(Gen9RenderSurfaceStateDataTests, WhenMemoryObjectControlStateIndexToMocsTablesIsSetThenValueIsShifted) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;

    uint32_t value = 4;
    surfaceState.setMemoryObjectControlStateIndexToMocsTables(value);

    EXPECT_EQ(surfaceState.TheStructure.Common.MemoryObjectControlState_IndexToMocsTables, value >> 1);
    EXPECT_EQ(surfaceState.getMemoryObjectControlStateIndexToMocsTables(), value);
}
