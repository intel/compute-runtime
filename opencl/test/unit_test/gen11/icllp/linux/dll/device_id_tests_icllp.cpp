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
        {0xFF05, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo},
        {0x8A56, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo},
        {0x8A58, &ICLLP_1x4x8::hwInfo, &ICLLP_1x4x8::setupHardwareInfo},

        {0x8A5C, &ICLLP_1x6x8::hwInfo, &ICLLP_1x6x8::setupHardwareInfo},
        {0x8A5A, &ICLLP_1x6x8::hwInfo, &ICLLP_1x6x8::setupHardwareInfo},

        {0x8A50, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo},
        {0x8A52, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo},
        {0x8A51, &ICLLP_1x8x8::hwInfo, &ICLLP_1x8x8::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
