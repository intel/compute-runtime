/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenCriSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 1> expectedDescriptors = {{
        {0x674C, &CriHwConfig::hwInfo, &CriHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
