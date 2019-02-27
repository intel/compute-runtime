/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"

#include "hw_cmds.h"

#include <array>

using namespace OCLRT;

TEST(CnlDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 20> expectedDescriptors = {{
        {ICNL_5x8_DESK_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_5x8_DESKTOP_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_5x8_HALO_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_5x8_SUPERSKU_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_5x8_ULX_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupHardwareInfo, GTTYPE_GT2},

        {ICNL_5x8_ULT_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_4x8_ULT_DEVICE_F0_ID, &CNL_2x4x8::hwInfo, &CNL_2x4x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_4x8_ULX_DEVICE_F0_ID, &CNL_2x4x8::hwInfo, &CNL_2x4x8::setupHardwareInfo, GTTYPE_GT2},
        {ICNL_4x8_HALO_DEVICE_F0_ID, &CNL_2x4x8::hwInfo, &CNL_2x4x8::setupHardwareInfo, GTTYPE_GT2},

        {ICNL_3x8_DESK_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_3x8_ULT_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_2x8_ULT_DEVICE_F0_ID, &CNL_1x2x8::hwInfo, &CNL_1x2x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_3x8_HALO_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_3x8_DESKTOP_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_3x8_ULX_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_2x8_ULX_DEVICE_F0_ID, &CNL_1x2x8::hwInfo, &CNL_1x2x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_2x8_HALO_DEVICE_F0_ID, &CNL_1x2x8::hwInfo, &CNL_1x2x8::setupHardwareInfo, GTTYPE_GT1},
        {ICNL_9x8_DESK_DEVICE_F0_ID, &CNL_4x9x8::hwInfo, &CNL_4x9x8::setupHardwareInfo, GTTYPE_GT3},
        {ICNL_9x8_ULT_DEVICE_F0_ID, &CNL_4x9x8::hwInfo, &CNL_4x9x8::setupHardwareInfo, GTTYPE_GT3},
        {ICNL_9x8_SUPERSKU_DEVICE_F0_ID, &CNL_4x9x8::hwInfo, &CNL_4x9x8::setupHardwareInfo, GTTYPE_GT3},
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
