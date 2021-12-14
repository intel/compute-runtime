/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen12LpCommandEncodeTest = testing::Test;
GEN12LPTEST_F(Gen12LpCommandEncodeTest, givenGen12LpPlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_FALSE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}
