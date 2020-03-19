/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_cmds.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "test.h"

using namespace NEO;

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
