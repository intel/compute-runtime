/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using L3ControlTests = ::testing::Test;

HWTEST2_F(L3ControlTests, givenL3ControlWhenAdjustCalledThenItIsNotChanged, IsXeHpCore) {
    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto l3Control = FamilyType::cmdInitL3Control;
    auto l3ControlOnStart = l3Control;

    adjustL3ControlField<FamilyType>(&l3Control);
    EXPECT_EQ(0, memcmp(&l3ControlOnStart, &l3Control, sizeof(L3_CONTROL))); // no change
}
