/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen8CommandEncodeTest = testing::Test;
GEN8TEST_F(Gen8CommandEncodeTest, givenGen8PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}

GEN8TEST_F(Gen8CommandEncodeTest, givenBcsCommandsHelperWhenMiArbCheckWaRequiredThenReturnTrue) {
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::miArbCheckWaRequired());
}
