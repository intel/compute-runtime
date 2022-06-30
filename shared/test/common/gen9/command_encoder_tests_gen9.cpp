/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen9CommandEncodeTest = testing::Test;
GEN9TEST_F(Gen9CommandEncodeTest, givenGen9PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}
