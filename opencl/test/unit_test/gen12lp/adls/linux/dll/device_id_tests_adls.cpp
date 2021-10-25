/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adls.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "test.h"

#include <array>

using namespace NEO;

TEST(AdlsDeviceIdTest, GivenSupportedDeviceIdThenHardwareInfoIsCorrect) {
    std::array<DeviceDescriptor, 7> expectedDescriptors = {{
        {0x4680, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {0x4682, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {0x4688, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {0x468A, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {0x4690, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {0x4692, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {0x4693, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
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
