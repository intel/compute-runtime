/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, givenArlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 1> expectedDescriptors = {{
        {0x7D67, IGFX_ARROWLAKE},
    }};

    testImpl(expectedDescriptors);
}
