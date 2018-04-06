/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

TEST(CnlDeviceIdTest, supportedDeviceId) {
    std::array<DeviceDescriptor, 20> expectedDescriptors = {{
        {ICNL_5x8_DESK_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_5x8_DESKTOP_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_5x8_HALO_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_5x8_SUPERSKU_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_5x8_ULX_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupGtSystemInfo, GTTYPE_GT2},

        {ICNL_5x8_ULT_DEVICE_F0_ID, &CNL_2x5x8::hwInfo, &CNL_2x5x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_4x8_ULT_DEVICE_F0_ID, &CNL_2x4x8::hwInfo, &CNL_2x4x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_4x8_ULX_DEVICE_F0_ID, &CNL_2x4x8::hwInfo, &CNL_2x4x8::setupGtSystemInfo, GTTYPE_GT2},
        {ICNL_4x8_HALO_DEVICE_F0_ID, &CNL_2x4x8::hwInfo, &CNL_2x4x8::setupGtSystemInfo, GTTYPE_GT2},

        {ICNL_3x8_DESK_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_3x8_ULT_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_2x8_ULT_DEVICE_F0_ID, &CNL_1x2x8::hwInfo, &CNL_1x2x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_3x8_HALO_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_3x8_DESKTOP_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_3x8_ULX_DEVICE_F0_ID, &CNL_1x3x8::hwInfo, &CNL_1x3x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_2x8_ULX_DEVICE_F0_ID, &CNL_1x2x8::hwInfo, &CNL_1x2x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_2x8_HALO_DEVICE_F0_ID, &CNL_1x2x8::hwInfo, &CNL_1x2x8::setupGtSystemInfo, GTTYPE_GT1},
        {ICNL_9x8_DESK_DEVICE_F0_ID, &CNL_4x9x8::hwInfo, &CNL_4x9x8::setupGtSystemInfo, GTTYPE_GT3},
        {ICNL_9x8_ULT_DEVICE_F0_ID, &CNL_4x9x8::hwInfo, &CNL_4x9x8::setupGtSystemInfo, GTTYPE_GT3},
        {ICNL_9x8_SUPERSKU_DEVICE_F0_ID, &CNL_4x9x8::hwInfo, &CNL_4x9x8::setupGtSystemInfo, GTTYPE_GT3},
    }};

    auto compareStructs = [](const DeviceDescriptor *first, const DeviceDescriptor *second) {
        return first->deviceId == second->deviceId && first->pHwInfo == second->pHwInfo &&
               first->setupGtSystemInfo == second->setupGtSystemInfo && first->eGtType == second->eGtType;
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
