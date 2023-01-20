/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, givenMtlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 5> expectedDescriptors = {{{0x7D40, &MtlHwConfig::hwInfo, &MtlHwConfig::setupHardwareInfo},
                                                            {0x7D55, &MtlHwConfig::hwInfo, &MtlHwConfig::setupHardwareInfo},
                                                            {0x7DD5, &MtlHwConfig::hwInfo, &MtlHwConfig::setupHardwareInfo},
                                                            {0x7D45, &MtlHwConfig::hwInfo, &MtlHwConfig::setupHardwareInfo},
                                                            {0x7D60, &MtlHwConfig::hwInfo, &MtlHwConfig::setupHardwareInfo}}};

    testImpl(expectedDescriptors);
}
