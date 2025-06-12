/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenPtlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 10> expectedDescriptors = {{{0xB080, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB081, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB082, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB083, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB08F, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB090, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB0A0, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xB0B0, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xFD80, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo},
                                                             {0xFD81, &PtlHwConfig::hwInfo, &PtlHwConfig::setupHardwareInfo}}};

    testImpl(expectedDescriptors);
}
