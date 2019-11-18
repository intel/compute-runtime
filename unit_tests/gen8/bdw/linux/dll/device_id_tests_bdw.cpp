/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"

#include <array>

using namespace NEO;

TEST(BdwDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 19> expectedDescriptors = {{
        {IBDW_GT1_DESK_DEVICE_F0_ID, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IBDW_GT1_HALO_MOBL_DEVICE_F0_ID, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IBDW_GT1_SERV_DEVICE_F0_ID, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IBDW_GT1_ULT_MOBL_DEVICE_F0_ID, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IBDW_GT1_ULX_DEVICE_F0_ID, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},
        {IBDW_GT1_WRK_DEVICE_F0_ID, &BDW_1x2x6::hwInfo, &BDW_1x2x6::setupHardwareInfo, GTTYPE_GT1},

        {IBDW_GT2_DESK_DEVICE_F0_ID, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IBDW_GT2_HALO_MOBL_DEVICE_F0_ID, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IBDW_GT2_SERV_DEVICE_F0_ID, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IBDW_GT2_ULT_MOBL_DEVICE_F0_ID, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IBDW_GT2_ULX_DEVICE_F0_ID, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},
        {IBDW_GT2_WRK_DEVICE_F0_ID, &BDW_1x3x8::hwInfo, &BDW_1x3x8::setupHardwareInfo, GTTYPE_GT2},

        {IBDW_GT3_DESK_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IBDW_GT3_HALO_MOBL_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IBDW_GT3_SERV_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IBDW_GT3_ULT_MOBL_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IBDW_GT3_ULT25W_MOBL_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IBDW_GT3_ULX_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
        {IBDW_GT3_WRK_DEVICE_F0_ID, &BDW_2x3x8::hwInfo, &BDW_2x3x8::setupHardwareInfo, GTTYPE_GT3},
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
