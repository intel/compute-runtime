/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adln.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/test_macros/test.h"

#include <array>

using namespace NEO;

TEST(AdlnDeviceIdTest, GivenSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 5> expectedDescriptors = {{
        {0x46D0, IGFX_ALDERLAKE_N},
        {0x46D1, IGFX_ALDERLAKE_N},
        {0x46D2, IGFX_ALDERLAKE_N},
        {0x46D3, IGFX_ALDERLAKE_N},
        {0x46D4, IGFX_ALDERLAKE_N},
    }};

    auto compareStructs = [](const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->productFamily == second->productFamily;
    };

    size_t startIndex = 0;
    while (!compareStructs(&expectedDescriptors[0], &deviceDescriptorTable[startIndex]) &&
           deviceDescriptorTable[startIndex].deviceId != 0) {
        startIndex++;
    };
    EXPECT_NE(0u, deviceDescriptorTable[startIndex].deviceId);

    for (auto &expected : expectedDescriptors) {
        EXPECT_TRUE(compareStructs(&expected, &deviceDescriptorTable[startIndex]));
        startIndex++;
    }
}
