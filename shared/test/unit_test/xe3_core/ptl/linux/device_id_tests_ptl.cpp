/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/linux/device_id_fixture.h"

using namespace NEO;

TEST_F(DeviceIdTests, GivenPtlSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 14> expectedDescriptors = {{{0xB080, IGFX_PTL},
                                                             {0xB081, IGFX_PTL},
                                                             {0xB082, IGFX_PTL},
                                                             {0xB083, IGFX_PTL},
                                                             {0xB084, IGFX_PTL},
                                                             {0xB085, IGFX_PTL},
                                                             {0xB086, IGFX_PTL},
                                                             {0xB087, IGFX_PTL},
                                                             {0xB08F, IGFX_PTL},
                                                             {0xB090, IGFX_PTL},
                                                             {0xB0A0, IGFX_PTL},
                                                             {0xB0B0, IGFX_PTL},
                                                             {0xFD80, IGFX_PTL},
                                                             {0xFD81, IGFX_PTL}}};

    testImpl(expectedDescriptors);
}
