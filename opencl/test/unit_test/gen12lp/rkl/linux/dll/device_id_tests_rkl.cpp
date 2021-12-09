/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenRklSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 6> expectedDescriptors = {{{0x4C80, &RKL_HW_CONFIG::hwInfo, &RKL_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
                                                            {0x4C8A, &RKL_HW_CONFIG::hwInfo, &RKL_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
                                                            {0x4C8B, &RKL_HW_CONFIG::hwInfo, &RKL_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
                                                            {0x4C8C, &RKL_HW_CONFIG::hwInfo, &RKL_HW_CONFIG::setupHardwareInfo, GTTYPE_GT0_5},
                                                            {0x4C90, &RKL_HW_CONFIG::hwInfo, &RKL_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
                                                            {0x4C9A, &RKL_HW_CONFIG::hwInfo, &RKL_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1}}};

    testImpl(expectedDescriptors);
}
