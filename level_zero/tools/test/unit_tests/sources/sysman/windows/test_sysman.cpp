/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/test/common/mocks/mock_driver_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

using MockDeviceSysmanGetTest = Test<DeviceFixture>;
TEST_F(MockDeviceSysmanGetTest, GivenValidSysmanHandleSetInDeviceStructWhenGetThisSysmanHandleThenHandlesShouldBeSimilar) {
    SysmanDeviceImp *sysman = new SysmanDeviceImp(device->toHandle());
    device->setSysmanHandle(sysman);
    EXPECT_EQ(sysman, device->getSysmanHandle());
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleInSysmanInitThenValidSysmanHandleReceived) {
    ze_device_handle_t hSysman = device->toHandle();
    auto pSysmanDevice = L0::SysmanDeviceHandleContext::init(hSysman);
    EXPECT_NE(pSysmanDevice, nullptr);
    delete pSysmanDevice;
    pSysmanDevice = nullptr;
}

TEST_F(SysmanDeviceFixture, GivenMockEnvValuesWhenGettingEnvValueThenCorrectValueIsReturned) {
    ASSERT_NE(IoFunctions::mockableEnvValues, nullptr);
    EnvironmentVariableReader envVarReader;
    EXPECT_EQ(envVarReader.getSetting("ZES_ENABLE_SYSMAN", false), true);
}

TEST_F(SysmanDeviceFixture, GivenValidDeviceHandleWhenGettingFwUtilInterfaceAndGetPciBdfFailsThenFailureIsReturned) {
    auto deviceImp = static_cast<L0::DeviceImp *>(pWddmSysmanImp->getDeviceHandle());

    deviceImp->driverInfo.reset(nullptr);
    FirmwareUtil *pFwUtilInterfaceOld = pWddmSysmanImp->pFwUtilInterface;
    pWddmSysmanImp->pFwUtilInterface = nullptr;

    EXPECT_EQ(pWddmSysmanImp->getFwUtilInterface(), nullptr);
    pWddmSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
}

} // namespace ult
} // namespace L0