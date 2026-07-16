/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenCriSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 5> expectedDescriptors = {{
        {0x674C, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo},
        {0x674D, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo},
        {0x674E, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo},
        {0x674F, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo},
        {0x6750, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
