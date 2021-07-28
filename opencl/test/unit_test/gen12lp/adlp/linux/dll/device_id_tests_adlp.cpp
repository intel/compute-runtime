/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "test.h"

#include <array>

using namespace NEO;

TEST(AdlpDeviceIdTest, GivenSupportedDeviceIdThenDeviceDescriptorTableExists) {
    std::array<DeviceDescriptor, 18> expectedDescriptors = {{
        {0x46A0, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46B0, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46A1, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46A2, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46A3, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46A6, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46A8, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46AA, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x462A, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4626, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x4628, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46B1, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46B2, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46B3, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46C0, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46C1, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46C2, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
        {0x46C3, &ADLP_CONFIG::hwInfo, &ADLP_CONFIG::setupHardwareInfo, GTTYPE_GT2},
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
