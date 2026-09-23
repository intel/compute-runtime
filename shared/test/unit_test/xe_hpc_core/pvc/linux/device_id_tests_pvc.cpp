/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

PVCTEST_F(DeviceIdTests, GivenPvcSupportedDeviceIdThenConfigIsCorrect) {
    std::array<DeviceDescriptor, 11> expectedDescriptors = {{
        {0x0BD0, IGFX_PVC},
        {0x0BD5, IGFX_PVC},
        {0x0BD6, IGFX_PVC},
        {0x0BD7, IGFX_PVC},
        {0x0BD8, IGFX_PVC},
        {0x0BD9, IGFX_PVC},
        {0x0BDA, IGFX_PVC},
        {0x0BDB, IGFX_PVC},
        {0x0B69, IGFX_PVC},
        {0x0B6E, IGFX_PVC},
        {0x0BD4, IGFX_PVC},
    }};

    testImpl(expectedDescriptors);
}
