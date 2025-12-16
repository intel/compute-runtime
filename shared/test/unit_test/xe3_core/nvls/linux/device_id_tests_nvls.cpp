/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenNvlsSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 6> expectedDescriptors = {{
        {0xD740, &NvlsHwConfig::hwInfo, &NvlsHwConfig::setupHardwareInfo},
        {0xD741, &NvlsHwConfig::hwInfo, &NvlsHwConfig::setupHardwareInfo},
        {0xD742, &NvlsHwConfig::hwInfo, &NvlsHwConfig::setupHardwareInfo},
        {0xD743, &NvlsHwConfig::hwInfo, &NvlsHwConfig::setupHardwareInfo},
        {0xD744, &NvlsHwConfig::hwInfo, &NvlsHwConfig::setupHardwareInfo},
        {0xD745, &NvlsHwConfig::hwInfo, &NvlsHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
