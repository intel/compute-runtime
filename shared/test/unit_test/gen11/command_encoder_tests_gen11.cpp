/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "test.h"

using namespace NEO;

using Gen11CommandEncodeTest = testing::Test;
GEN11TEST_F(Gen11CommandEncodeTest, givenGen11PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsFalse) {
    EXPECT_FALSE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}
