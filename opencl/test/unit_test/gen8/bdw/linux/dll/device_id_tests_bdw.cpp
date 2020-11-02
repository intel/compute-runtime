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

TEST(BdwDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 19> expectedDescriptors = {{
        {0x0BD1, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x1602, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x160A, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x1606, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x160E, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x160D, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {0x0BD2, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x1612, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x161A, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x1616, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x161E, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x161D, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {0x0BD3, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x1622, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x162A, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x1626, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x162B, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x162E, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x162D, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
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
