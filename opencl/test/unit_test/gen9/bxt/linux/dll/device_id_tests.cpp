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

TEST(BxtDeviceIdTest, GivenSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 8> expectedDescriptors = {{
        {0x9906, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x9907, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x0A84, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x5A84, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x5A85, &BXT_1x2x6::hwInfo, &BXT_1x2x6::setupHardwareInfo, GTTYPE_GTA},
        {0x1A85, &BXT_1x2x6::hwInfo, &BXT_1x2x6::setupHardwareInfo, GTTYPE_GTA},
        {0x1A84, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
        {0x9908, &BXT_1x3x6::hwInfo, &BXT_1x3x6::setupHardwareInfo, GTTYPE_GTA},
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
