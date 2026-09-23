/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"
using namespace NEO;

TEST_F(DeviceIdTests, GivenDg1SupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 5> expectedDescriptors = {{
        {0x4905, IGFX_DG1},
        {0x4906, IGFX_DG1},
        {0x4907, IGFX_DG1},
        {0x4908, IGFX_DG1},
        {0x4909, IGFX_DG1},
    }};

    testImpl(expectedDescriptors);
}
