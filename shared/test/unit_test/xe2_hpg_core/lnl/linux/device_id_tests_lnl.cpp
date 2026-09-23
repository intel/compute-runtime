/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenLnlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 3> expectedDescriptors = {{
        {0x6420, IGFX_LUNARLAKE},
        {0x64A0, IGFX_LUNARLAKE},
        {0x64B0, IGFX_LUNARLAKE},
    }};

    testImpl(expectedDescriptors);
}
