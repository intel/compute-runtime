/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

PVCTEST_F(DeviceIdTests, GivenPvcSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0x0BD0, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BD5, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BD6, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BD7, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BD8, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BD9, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BDA, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
        {0x0BDB, &PvcHwConfig::hwInfo, &PvcHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
