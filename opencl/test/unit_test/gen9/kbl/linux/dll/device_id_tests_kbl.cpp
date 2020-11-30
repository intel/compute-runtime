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

TEST(KblDeviceIdTest, GivenSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 26> expectedDescriptors = {{
        {0x5902, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x590B, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x590A, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x5906, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x590E, &KBL_1x3x6::hwInfo, &KBL_1x3x6::setupHardwareInfo, GTTYPE_GT1},
        {0x5908, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {0x5913, &KBL_1x3x6::hwInfo, &KBL_1x3x6::setupHardwareInfo, GTTYPE_GT1_5},
        {0x5915, &KBL_1x2x6::hwInfo, &KBL_1x2x6::setupHardwareInfo, GTTYPE_GT1_5},

        {0x5912, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591B, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x5917, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591A, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x5916, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591E, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591D, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x591C, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x5921, &KBL_1x3x8::hwInfo, &KBL_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {0x5926, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x5927, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x592B, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x592A, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x5923, &KBL_2x3x8::hwInfo, &KBL_2x3x8::setupHardwareInfo, GTTYPE_GT3},

        {0x5932, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {0x593B, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {0x593A, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
        {0x593D, &KBL_3x3x8::hwInfo, &KBL_3x3x8::setupHardwareInfo, GTTYPE_GT4},
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
