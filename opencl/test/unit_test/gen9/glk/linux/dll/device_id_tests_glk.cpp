/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenGlkSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 2> expectedDescriptors = {{
        {0x3184, &GLK_1x3x6::hwInfo, &GLK_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x3185, &GLK_1x2x6::hwInfo, &GLK_1x2x6::setupHardwareInfo, GTTYPE_GTA},
    }};

    testImpl(expectedDescriptors);
}
