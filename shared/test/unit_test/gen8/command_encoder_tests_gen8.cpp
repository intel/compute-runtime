/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen8CommandEncodeTest = testing::Test;
GEN8TEST_F(Gen8CommandEncodeTest, givenGen8PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}

GEN8TEST_F(Gen8CommandEncodeTest, givenBcsCommandsHelperWhenMiArbCheckWaRequiredThenReturnTrue) {
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::miArbCheckWaRequired());
}

GEN8TEST_F(Gen8CommandEncodeTest, givenSurfaceStateWhenAuxParamsForMCSCCSAreSetThenAuxModeStaysTheSame) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    MockDevice device;
    auto releaseHelper = device.getReleaseHelper();

    auto originalAuxMode = surfaceState.getAuxiliarySurfaceMode();

    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, releaseHelper);

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), originalAuxMode);
}
