/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenXeHpSdvSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 16> expectedDescriptors = {{{0x0201, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0202, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0203, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0204, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0205, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0206, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0207, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0208, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0209, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x020A, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x020B, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x020C, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x020D, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x020E, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x020F, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo},
                                                             {0x0210, &XE_HP_SDV_CONFIG::hwInfo, &XE_HP_SDV_CONFIG::setupHardwareInfo}}};

    testImpl(expectedDescriptors);
}
