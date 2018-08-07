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

#include "unit_tests/os_interface/linux/device_factory_tests.h"

typedef DeviceFactoryLinuxTest DeviceFactoryLinuxTestCnl;

GEN10TEST_F(DeviceFactoryLinuxTestCnl, queryWhitelistedPreemptionRegister) {
    pDrm->StoredPreemptionSupport = 1;
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_TRUE(success);
#if defined(I915_PARAM_HAS_PREEMPTION)
    EXPECT_TRUE(hwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580);
#else
    EXPECT_FALSE(hwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580);
#endif

    DeviceFactory::releaseDevices();
}

GEN10TEST_F(DeviceFactoryLinuxTestCnl, queryNotWhitelistedPreemptionRegister) {
    pDrm->StoredPreemptionSupport = 0;
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;

    bool success = DeviceFactory::getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_FALSE(hwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580);

    DeviceFactory::releaseDevices();
}
