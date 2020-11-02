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

TEST(CflDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 41> expectedDescriptors = {{
        {0x3E90, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x3E93, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x3EA4, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x3E99, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x3EA1, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {0x3E92, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3E9B, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3E94, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3E91, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3E96, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3E9A, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3EA3, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3EA9, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3EA0, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x3E98, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {0x3E95, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x3EA6, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x3EA7, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x3EA8, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x3EA5, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {0x3EA2, &CFL_2x3x8::hwInfo, &CFL_2x3x8::setupHardwareInfo, GTTYPE_GT3},

        // CML GT1
        {0x9B21, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BAA, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BAB, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BAC, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BA0, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BA5, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BA8, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BA4, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {0x9BA2, &CFL_1x2x6::hwInfo, &CFL_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        // CML GT2
        {0x9B41, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BCA, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BCB, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BCC, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BC0, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BC5, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BC8, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BC4, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BC2, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        // CML WORKSTATION
        {0x9BC6, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {0x9BE6, &CFL_1x3x8::hwInfo, &CFL_1x3x8::setupHardwareInfo, GTTYPE_GT2},
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
