/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenKblSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 26> expectedDescriptors = {{
        {0x5902, &KblHw1x2x6::hwInfo, &KblHw1x2x6::setupHardwareInfo},
        {0x590B, &KblHw1x2x6::hwInfo, &KblHw1x2x6::setupHardwareInfo},
        {0x590A, &KblHw1x2x6::hwInfo, &KblHw1x2x6::setupHardwareInfo},
        {0x5906, &KblHw1x2x6::hwInfo, &KblHw1x2x6::setupHardwareInfo},
        {0x590E, &KblHw1x3x6::hwInfo, &KblHw1x3x6::setupHardwareInfo},
        {0x5908, &KblHw1x2x6::hwInfo, &KblHw1x2x6::setupHardwareInfo},

        {0x5913, &KblHw1x3x6::hwInfo, &KblHw1x3x6::setupHardwareInfo},
        {0x5915, &KblHw1x2x6::hwInfo, &KblHw1x2x6::setupHardwareInfo},

        {0x5912, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x591B, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x5917, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x591A, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x5916, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x591E, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x591D, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x591C, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},
        {0x5921, &KblHw1x3x8::hwInfo, &KblHw1x3x8::setupHardwareInfo},

        {0x5926, &KblHw2x3x8::hwInfo, &KblHw2x3x8::setupHardwareInfo},
        {0x5927, &KblHw2x3x8::hwInfo, &KblHw2x3x8::setupHardwareInfo},
        {0x592B, &KblHw2x3x8::hwInfo, &KblHw2x3x8::setupHardwareInfo},
        {0x592A, &KblHw2x3x8::hwInfo, &KblHw2x3x8::setupHardwareInfo},
        {0x5923, &KblHw2x3x8::hwInfo, &KblHw2x3x8::setupHardwareInfo},

        {0x5932, &KblHw3x3x8::hwInfo, &KblHw3x3x8::setupHardwareInfo},
        {0x593B, &KblHw3x3x8::hwInfo, &KblHw3x3x8::setupHardwareInfo},
        {0x593A, &KblHw3x3x8::hwInfo, &KblHw3x3x8::setupHardwareInfo},
        {0x593D, &KblHw3x3x8::hwInfo, &KblHw3x3x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
