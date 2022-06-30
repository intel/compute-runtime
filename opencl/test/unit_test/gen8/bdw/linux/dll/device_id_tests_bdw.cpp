/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenBdwSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 16> expectedDescriptors = {{
        {0x1602, &BdwHw1x2x6::hwInfo, &BdwHw1x2x6::setupHardwareInfo},
        {0x160A, &BdwHw1x2x6::hwInfo, &BdwHw1x2x6::setupHardwareInfo},
        {0x1606, &BdwHw1x2x6::hwInfo, &BdwHw1x2x6::setupHardwareInfo},
        {0x160E, &BdwHw1x2x6::hwInfo, &BdwHw1x2x6::setupHardwareInfo},
        {0x160D, &BdwHw1x2x6::hwInfo, &BdwHw1x2x6::setupHardwareInfo},

        {0x1612, &BdwHw1x3x8::hwInfo, &BdwHw1x3x8::setupHardwareInfo},
        {0x161A, &BdwHw1x3x8::hwInfo, &BdwHw1x3x8::setupHardwareInfo},
        {0x1616, &BdwHw1x3x8::hwInfo, &BdwHw1x3x8::setupHardwareInfo},
        {0x161E, &BdwHw1x3x8::hwInfo, &BdwHw1x3x8::setupHardwareInfo},
        {0x161D, &BdwHw1x3x8::hwInfo, &BdwHw1x3x8::setupHardwareInfo},

        {0x1622, &BdwHw2x3x8::hwInfo, &BdwHw2x3x8::setupHardwareInfo},
        {0x162A, &BdwHw2x3x8::hwInfo, &BdwHw2x3x8::setupHardwareInfo},
        {0x1626, &BdwHw2x3x8::hwInfo, &BdwHw2x3x8::setupHardwareInfo},
        {0x162B, &BdwHw2x3x8::hwInfo, &BdwHw2x3x8::setupHardwareInfo},
        {0x162E, &BdwHw2x3x8::hwInfo, &BdwHw2x3x8::setupHardwareInfo},
        {0x162D, &BdwHw2x3x8::hwInfo, &BdwHw2x3x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
