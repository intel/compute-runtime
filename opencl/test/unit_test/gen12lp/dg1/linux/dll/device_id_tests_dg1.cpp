/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "test.h"

#include <array>

using namespace NEO;

TEST(Dg1DeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 4> expectedDescriptors = {{
        {0x4905, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4906, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4907, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4908, &DG1_CONFIG::hwInfo, &DG1_CONFIG::setupHardwareInfo, GTTYPE_GT2},
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
