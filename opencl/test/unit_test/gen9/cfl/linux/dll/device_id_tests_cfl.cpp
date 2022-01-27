/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenCflSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 42> expectedDescriptors = {{
        {0x3E90, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x3E93, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x3EA4, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x3E99, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x3EA1, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},

        {0x3E92, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3E9B, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3E94, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3E91, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3E96, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3E9A, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3EA3, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3EA9, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3EA0, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x3E98, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},

        {0x3E95, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo},
        {0x3EA6, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo},
        {0x3EA7, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo},
        {0x3EA8, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo},
        {0x3EA5, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo},
        {0x3EA2, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo},

        {0x9B21, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BAA, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BAB, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BAC, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BA0, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BA5, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BA8, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BA4, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9BA2, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo},
        {0x9B41, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BCA, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BCB, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BCC, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BC0, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BC5, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BC8, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BC4, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BC2, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BC6, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BE6, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
        {0x9BF6, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
