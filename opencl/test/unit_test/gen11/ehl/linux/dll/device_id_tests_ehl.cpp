/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenEhlSpportedDeviceIdWhenCheckingHwSetupThenItIsCorrect) {
    std::array<DeviceDescriptor, 9> expectedDescriptors = {{
        {0x4500, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4541, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4551, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4571, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4555, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4E51, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4E61, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4E71, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
        {0x4E55, &EhlHwConfig::hwInfo, &EhlHwConfig::setupHardwareInfo},
    }};

    testImpl(expectedDescriptors);
}
