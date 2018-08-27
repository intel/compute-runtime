/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "hw_cmds.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "test.h"

#include <array>

using namespace OCLRT;

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
