/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenRklSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 6> expectedDescriptors = {{{0x4C80, &RklHwConfig::hwInfo, &RklHwConfig::setupHardwareInfo},
                                                            {0x4C8A, &RklHwConfig::hwInfo, &RklHwConfig::setupHardwareInfo},
                                                            {0x4C8B, &RklHwConfig::hwInfo, &RklHwConfig::setupHardwareInfo},
                                                            {0x4C8C, &RklHwConfig::hwInfo, &RklHwConfig::setupHardwareInfo},
                                                            {0x4C90, &RklHwConfig::hwInfo, &RklHwConfig::setupHardwareInfo},
                                                            {0x4C9A, &RklHwConfig::hwInfo, &RklHwConfig::setupHardwareInfo}}};

    testImpl(expectedDescriptors);
}
