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
        {0x1602, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo},
        {0x160A, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo},
        {0x1606, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo},
        {0x160E, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo},
        {0x160D, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo},

        {0x1612, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo},
        {0x161A, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo},
        {0x1616, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo},
        {0x161E, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo},
        {0x161D, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo},

        {0x1622, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo},
        {0x162A, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo},
        {0x1626, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo},
        {0x162B, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo},
        {0x162E, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo},
        {0x162D, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
