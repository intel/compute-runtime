/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adls.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "opencl/source/dll/linux/devices/device_ids.h"
#include "test.h"

#include <array>

using namespace NEO;

TEST(AdlsDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 3> expectedDescriptors = {{
        {DEVICE_ID_4680, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {DEVICE_ID_46FF, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
        {DEVICE_ID_4600, &ADLS_HW_CONFIG::hwInfo, &ADLS_HW_CONFIG::setupHardwareInfo, GTTYPE_GT1},
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
