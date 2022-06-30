/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenAdlsSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 13> expectedDescriptors = {{
        {0x4680, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0x4682, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0x4688, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0x468A, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0x4690, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0x4692, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0x4693, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0xA780, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0xA781, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0xA782, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0xA783, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0xA788, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
        {0xA789, &AdlsHwConfig::hwInfo, &AdlsHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
