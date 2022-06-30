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
        {0x3E90, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x3E93, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x3EA4, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x3E99, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x3EA1, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},

        {0x3E92, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3E9B, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3E94, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3E91, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3E96, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3E9A, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3EA3, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3EA9, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3EA0, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x3E98, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},

        {0x3E95, &CflHw2x3x8::hwInfo, &CflHw2x3x8::setupHardwareInfo},
        {0x3EA6, &CflHw2x3x8::hwInfo, &CflHw2x3x8::setupHardwareInfo},
        {0x3EA7, &CflHw2x3x8::hwInfo, &CflHw2x3x8::setupHardwareInfo},
        {0x3EA8, &CflHw2x3x8::hwInfo, &CflHw2x3x8::setupHardwareInfo},
        {0x3EA5, &CflHw2x3x8::hwInfo, &CflHw2x3x8::setupHardwareInfo},
        {0x3EA2, &CflHw2x3x8::hwInfo, &CflHw2x3x8::setupHardwareInfo},

        {0x9B21, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BAA, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BAB, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BAC, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BA0, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BA5, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BA8, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BA4, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9BA2, &CflHw1x2x6::hwInfo, &CflHw1x2x6::setupHardwareInfo},
        {0x9B41, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BCA, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BCB, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BCC, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BC0, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BC5, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BC8, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BC4, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BC2, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BC6, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BE6, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
        {0x9BF6, &CflHw1x3x8::hwInfo, &CflHw1x3x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
