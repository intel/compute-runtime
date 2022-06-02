/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenSklSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 26> expectedDescriptors = {{
        {0x1902, &SklHw1x2x6::hwInfo, &SklHw1x2x6::setupHardwareInfo},
        {0x190B, &SklHw1x2x6::hwInfo, &SklHw1x2x6::setupHardwareInfo},
        {0x190A, &SklHw1x2x6::hwInfo, &SklHw1x2x6::setupHardwareInfo},
        {0x1906, &SklHw1x2x6::hwInfo, &SklHw1x2x6::setupHardwareInfo},
        {0x190E, &SklHw1x2x6::hwInfo, &SklHw1x2x6::setupHardwareInfo},

        {0x1917, &SklHw1x3x6::hwInfo, &SklHw1x3x6::setupHardwareInfo},
        {0x1913, &SklHw1x3x6::hwInfo, &SklHw1x3x6::setupHardwareInfo},
        {0x1915, &SklHw1x3x6::hwInfo, &SklHw1x3x6::setupHardwareInfo},

        {0x1912, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x191B, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x191A, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x1916, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x191E, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x191D, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x1921, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},
        {0x9905, &SklHw1x3x8::hwInfo, &SklHw1x3x8::setupHardwareInfo},

        {0x192B, &SklHw2x3x8::hwInfo, &SklHw2x3x8::setupHardwareInfo},
        {0x192D, &SklHw2x3x8::hwInfo, &SklHw2x3x8::setupHardwareInfo},
        {0x192A, &SklHw2x3x8::hwInfo, &SklHw2x3x8::setupHardwareInfo},
        {0x1923, &SklHw2x3x8::hwInfo, &SklHw2x3x8::setupHardwareInfo},
        {0x1926, &SklHw2x3x8::hwInfo, &SklHw2x3x8::setupHardwareInfo},
        {0x1927, &SklHw2x3x8::hwInfo, &SklHw2x3x8::setupHardwareInfo},

        {0x1932, &SklHw3x3x8::hwInfo, &SklHw3x3x8::setupHardwareInfo},
        {0x193B, &SklHw3x3x8::hwInfo, &SklHw3x3x8::setupHardwareInfo},
        {0x193A, &SklHw3x3x8::hwInfo, &SklHw3x3x8::setupHardwareInfo},
        {0x193D, &SklHw3x3x8::hwInfo, &SklHw3x3x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
