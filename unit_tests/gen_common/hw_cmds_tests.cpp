/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen_common/hw_cmds.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "test.h"
#include "unit_tests/mocks/mock_device.h"

using InterfaceDescriptorDataTests = ::testing::Test;

HWCMDTEST_F(IGFX_GEN8_CORE, InterfaceDescriptorDataTests, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValueIsSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    MockDevice device;
    auto hwInfo = device.getHardwareInfo();

    HardwareCommandsHelper<FamilyType>::programBarrierEnable(&idd, 0, hwInfo);
    EXPECT_FALSE(idd.getBarrierEnable());

    HardwareCommandsHelper<FamilyType>::programBarrierEnable(&idd, 1, hwInfo);
    EXPECT_TRUE(idd.getBarrierEnable());

    HardwareCommandsHelper<FamilyType>::programBarrierEnable(&idd, 2, hwInfo);
    EXPECT_TRUE(idd.getBarrierEnable());
}
