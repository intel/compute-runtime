/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenNvlsSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 6> expectedDescriptors = {{
        {0xD740, IGFX_NVL_XE3G},
        {0xD741, IGFX_NVL_XE3G},
        {0xD742, IGFX_NVL_XE3G},
        {0xD743, IGFX_NVL_XE3G},
        {0xD744, IGFX_NVL_XE3G},
        {0xD745, IGFX_NVL_XE3G},
    }};

    testImpl(expectedDescriptors);
}
