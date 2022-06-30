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
        {0xFF20, &TgllpHw1x6x16::hwInfo, &TgllpHw1x6x16::setupHardwareInfo},
        {0x9A49, &TgllpHw1x6x16::hwInfo, &TgllpHw1x6x16::setupHardwareInfo},
        {0x9A40, &TgllpHw1x6x16::hwInfo, &TgllpHw1x6x16::setupHardwareInfo},
        {0x9A59, &TgllpHw1x6x16::hwInfo, &TgllpHw1x6x16::setupHardwareInfo},
        {0x9A60, &TgllpHw1x2x16::hwInfo, &TgllpHw1x2x16::setupHardwareInfo},
        {0x9A68, &TgllpHw1x2x16::hwInfo, &TgllpHw1x2x16::setupHardwareInfo},
        {0x9A70, &TgllpHw1x2x16::hwInfo, &TgllpHw1x2x16::setupHardwareInfo},
        {0x9A78, &TgllpHw1x2x16::hwInfo, &TgllpHw1x2x16::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
