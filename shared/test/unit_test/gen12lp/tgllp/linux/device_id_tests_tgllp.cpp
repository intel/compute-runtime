/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenTgllpSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 7> expectedDescriptors = {{
        {0x9A49, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
        {0x9A40, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
        {0x9A59, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
        {0x9A60, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
        {0x9A68, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
        {0x9A70, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
        {0x9A78, &TgllpHwConfig::hwInfo, &TgllpHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
