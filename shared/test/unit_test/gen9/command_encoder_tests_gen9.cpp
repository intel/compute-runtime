/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen9CommandEncodeTest = testing::Test;
GEN9TEST_F(Gen9CommandEncodeTest, givenGen9PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}

GEN9TEST_F(Gen9CommandEncodeTest, givenSurfaceStateWhenAuxParamsForMCSCCSAreSetThenAuxModeStaysTheSame) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    MockDevice device;
    auto releaseHelper = device.getReleaseHelper();

    auto originalAuxMode = surfaceState.getAuxiliarySurfaceMode();

    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, releaseHelper);

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), originalAuxMode);
}
