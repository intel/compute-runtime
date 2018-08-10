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

#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/os_interface/linux/device_factory_tests.h"

TEST_F(DeviceFactoryLinuxTest, GetDevicesCheckEUCntSSCnt) {
    HardwareInfo *hwInfo = nullptr;
    const HardwareInfo *refHwinfo = *platformDevices;
    size_t numDevices = 0;

    pDrm->StoredEUVal = 11;
    pDrm->StoredSSVal = 8;

    bool success = DeviceFactory::getDevices(&hwInfo, numDevices, executionEnvironment);

    EXPECT_TRUE(success);
    EXPECT_EQ((int)numDevices, 1);
    EXPECT_NE(hwInfo, nullptr);
    EXPECT_NE(hwInfo->pPlatform, nullptr);
    EXPECT_NE(hwInfo->pSysInfo, nullptr);
    EXPECT_EQ(refHwinfo->pPlatform->eDisplayCoreFamily, hwInfo->pPlatform->eDisplayCoreFamily);
    EXPECT_EQ((int)hwInfo->pSysInfo->EUCount, 11);
    EXPECT_EQ((int)hwInfo->pSysInfo->SubSliceCount, 8);

    //temporararily return GT2.
    EXPECT_EQ(1u, hwInfo->pSkuTable->ftrGT2);

    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryLinuxTest, GetDevicesDrmCreateFailed) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;

    pushDrmMock(nullptr);
    bool success = DeviceFactory::getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_FALSE(success);

    popDrmMock();
}

TEST_F(DeviceFactoryLinuxTest, GetDevicesDrmCreateFailedConfigureHwInfo) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;

    pDrm->StoredRetValForDeviceID = -1;

    bool success = DeviceFactory::getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_FALSE(success);

    pDrm->StoredRetValForDeviceID = 0;
}

TEST_F(DeviceFactoryLinuxTest, ReleaseDevices) {
    MockDeviceFactory mockDeviceFactory;
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;

    pDrm->StoredDeviceID = 0x5A84;
    pDrm->StoredDeviceRevID = 0x0B;
    pDrm->StoredEUVal = 18;
    pDrm->StoredSSVal = 3;
    pDrm->StoredHasPooledEU = 1;
    pDrm->StoredMinEUinPool = 9;
    pDrm->StoredRetVal = -1;

    bool success = mockDeviceFactory.getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_TRUE(success);

    mockDeviceFactory.releaseDevices();
    EXPECT_TRUE(mockDeviceFactory.getNumDevices() == 0);
    EXPECT_TRUE(pDrm->getFileDescriptor() == -1);
}

TEST_F(DeviceFactoryLinuxTest, givenGetDeviceCallWhenItIsDoneThenOsInterfaceIsAllocatedAndItContainDrm) {
    MockDeviceFactory mockDeviceFactory;
    HardwareInfo *hwInfo = nullptr;
    size_t numDevices = 0;
    bool success = mockDeviceFactory.getDevices(&hwInfo, numDevices, executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment.osInterface);
    EXPECT_EQ(pDrm, executionEnvironment.osInterface->get()->getDrm());
}
