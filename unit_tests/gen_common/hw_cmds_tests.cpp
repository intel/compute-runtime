/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen_common/hw_cmds.h"
#include "test.h"

using InterfaceDescriptorDataTests = ::testing::Test;

HWTEST_F(InterfaceDescriptorDataTests, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValueIsSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;

    idd.setBarrierEnable(0);
    EXPECT_FALSE(idd.getBarrierEnable());

    idd.setBarrierEnable(1);
    EXPECT_TRUE(idd.getBarrierEnable());

    idd.setBarrierEnable(2);
    EXPECT_TRUE(idd.getBarrierEnable());
}
