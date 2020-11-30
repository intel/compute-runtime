/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "test.h"

#include <array>

using namespace NEO;

TEST(GlkDeviceIdTest, GivenSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 2> expectedDescriptors = {{
        {0x3184, &GLK_1x3x6::hwInfo, &GLK_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x3185, &GLK_1x2x6::hwInfo, &GLK_1x2x6::setupHardwareInfo, GTTYPE_GTA},
    }};

    auto compareStructs = [](const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->pHwInfo == second->pHwInfo &&
               first->setupHardwareInfo == second->setupHardwareInfo && first->eGtType == second->eGtType;
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
