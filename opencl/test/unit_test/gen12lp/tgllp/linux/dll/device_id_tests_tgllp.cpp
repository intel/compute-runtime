/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenTgllpSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0xFF20, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo},
        {0x9A49, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo},
        {0x9A40, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo},
        {0x9A59, &TGLLP_1x6x16::hwInfo, &TGLLP_1x6x16::setupHardwareInfo},
        {0x9A60, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo},
        {0x9A68, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo},
        {0x9A70, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo},
        {0x9A78, &TGLLP_1x2x16::hwInfo, &TGLLP_1x2x16::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
