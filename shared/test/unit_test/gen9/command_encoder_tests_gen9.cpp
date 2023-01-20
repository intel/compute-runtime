/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen9CommandEncodeTest = testing::Test;
GEN9TEST_F(Gen9CommandEncodeTest, givenGen9PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}
