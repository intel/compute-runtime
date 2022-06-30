/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenIcllpSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0xFF05, &IcllpHw1x4x8::hwInfo, &IcllpHw1x4x8::setupHardwareInfo},
        {0x8A56, &IcllpHw1x4x8::hwInfo, &IcllpHw1x4x8::setupHardwareInfo},
        {0x8A58, &IcllpHw1x4x8::hwInfo, &IcllpHw1x4x8::setupHardwareInfo},

        {0x8A5C, &IcllpHw1x6x8::hwInfo, &IcllpHw1x6x8::setupHardwareInfo},
        {0x8A5A, &IcllpHw1x6x8::hwInfo, &IcllpHw1x6x8::setupHardwareInfo},

        {0x8A50, &IcllpHw1x8x8::hwInfo, &IcllpHw1x8x8::setupHardwareInfo},
        {0x8A52, &IcllpHw1x8x8::hwInfo, &IcllpHw1x8x8::setupHardwareInfo},
        {0x8A51, &IcllpHw1x8x8::hwInfo, &IcllpHw1x8x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
