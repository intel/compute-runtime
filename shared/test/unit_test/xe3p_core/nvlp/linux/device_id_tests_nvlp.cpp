/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenNvlpSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 11> expectedDescriptors = {{
        {0xD750, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD751, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD752, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD753, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD754, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD755, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD756, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD757, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD75F, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD74A, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
        {0xD74B, &NvlHwConfig::hwInfo, &NvlHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
