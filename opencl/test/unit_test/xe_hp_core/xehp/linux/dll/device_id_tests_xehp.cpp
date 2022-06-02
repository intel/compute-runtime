/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenXeHpSdvSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 16> expectedDescriptors = {{{0x0201, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0202, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0203, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0204, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0205, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0206, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0207, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0208, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0209, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x020A, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x020B, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x020C, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x020D, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x020E, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x020F, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo},
                                                             {0x0210, &XehpSdvHwConfig::hwInfo, &XehpSdvHwConfig::setupHardwareInfo}}};

    testImpl(expectedDescriptors);
}
