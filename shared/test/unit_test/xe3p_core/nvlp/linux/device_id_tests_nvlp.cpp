/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenNvlpSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 11> expectedDescriptors = {{
        {0xD750, IGFX_NVL},
        {0xD751, IGFX_NVL},
        {0xD752, IGFX_NVL},
        {0xD753, IGFX_NVL},
        {0xD754, IGFX_NVL},
        {0xD755, IGFX_NVL},
        {0xD756, IGFX_NVL},
        {0xD757, IGFX_NVL},
        {0xD75F, IGFX_NVL},
        {0xD74A, IGFX_NVL},
        {0xD74B, IGFX_NVL},
    }};

    testImpl(expectedDescriptors);
}
