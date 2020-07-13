/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

using SysmanGetTest = Test<DeviceFixture>;

TEST_F(SysmanGetTest, whenCallingZetSysmanGetWithoutCallingZetInitThenUnitializedIsReturned) {
    zet_sysman_handle_t hSysman;
    zet_sysman_version_t version = ZET_SYSMAN_VERSION_CURRENT;
    ze_result_t res = zetSysmanGet(device->toHandle(), version, &hSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
}

using MockDeviceSysmanGetTest = Test<DeviceFixture>;
TEST_F(MockDeviceSysmanGetTest, GivenValidSysmanHandleSetInDeviceStructWhenGetThisSysmanHandleThenHandlesShouldBeSimilar) {
    SysmanDeviceImp *sysman = new SysmanDeviceImp(device->toHandle());
    device->setSysmanHandle(sysman);
    EXPECT_EQ(sysman, device->getSysmanHandle());
}

TEST_F(MockDeviceSysmanGetTest, GivenNULLDeviceHandleWhenCreatingSymanHandleThenNullSysmanHandleIsReturned) {
    ze_device_handle_t handle = nullptr;
    setenv("ZES_ENABLE_SYSMAN", "1", 1);
    EXPECT_EQ(nullptr, L0::SysmanDeviceHandleContext::init(handle));
    unsetenv("ZES_ENABLE_SYSMAN");
}

TEST_F(SysmanDeviceFixture, GivenSysmanEnvironmentSetWhenEnumertatingSysmanHandlesThenValidSysmanHandleReceived) {
    zes_device_handle_t hSysman = device->toHandle();
    auto pSysmanDevice = L0::SysmanDeviceHandleContext::init(hSysman);
    EXPECT_NE(pSysmanDevice, nullptr);
    delete pSysmanDevice;
    pSysmanDevice = nullptr;
}

TEST_F(SysmanDeviceFixture, GivenSysmanEnvironmentNotSetWhenEnumertatingSysmanHandlesThenNullSysmanHandleReceived) {
    zes_device_handle_t hSysman = device->toHandle();
    unsetenv("ZES_ENABLE_SYSMAN");
    EXPECT_EQ(L0::SysmanDeviceHandleContext::init(hSysman), nullptr);
}

TEST_F(SysmanDeviceFixture, GivenSetValidDrmHandleForDeviceWhenDoingOsSysmanDeviceInitThenSameDrmHandleIsRetrieved) {
    EXPECT_EQ(&pLinuxSysmanImp->getDrm(), device->getOsInterface().get()->getDrm());
}

TEST_F(SysmanDeviceFixture, GivenCreateFsAccessHandleWhenCallinggetFsAccessThenCreatedFsAccessHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pFsAccess != nullptr) {
        //delete previously allocated pFsAccess
        delete pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = nullptr;
    }
    pLinuxSysmanImp->pFsAccess = FsAccess::create();
    EXPECT_EQ(&pLinuxSysmanImp->getFsAccess(), pLinuxSysmanImp->pFsAccess);
}

TEST_F(SysmanDeviceFixture, GivenCreateSysfsAccessHandleWhenCallinggetSysfsAccessThenCreatedSysfsAccessHandleHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pSysfsAccess != nullptr) {
        //delete previously allocated pSysfsAccess
        delete pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = nullptr;
    }
    pLinuxSysmanImp->pSysfsAccess = SysfsAccess::create("");
    EXPECT_EQ(&pLinuxSysmanImp->getSysfsAccess(), pLinuxSysmanImp->pSysfsAccess);
}

TEST_F(SysmanDeviceFixture, GivenCreateProcfsAccessHandleWhenCallinggetProcfsAccessThenCreatedProcfsAccessHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pProcfsAccess != nullptr) {
        //delete previously allocated pProcfsAccess
        delete pLinuxSysmanImp->pProcfsAccess;
        pLinuxSysmanImp->pProcfsAccess = nullptr;
    }
    pLinuxSysmanImp->pProcfsAccess = ProcfsAccess::create();
    EXPECT_EQ(&pLinuxSysmanImp->getProcfsAccess(), pLinuxSysmanImp->pProcfsAccess);
}

TEST_F(SysmanDeviceFixture, GivenPmtHandleWhenCallinggetPlatformMonitoringTechAccessThenCreatedPmtHandleWillBeRetrieved) {
    if (pLinuxSysmanImp->pPmt != nullptr) {
        //delete previously allocated pPmt
        delete pLinuxSysmanImp->pPmt;
        pLinuxSysmanImp->pPmt = nullptr;
    }
    pLinuxSysmanImp->pPmt = new PlatformMonitoringTech();
    EXPECT_EQ(&pLinuxSysmanImp->getPlatformMonitoringTechAccess(), pLinuxSysmanImp->pPmt);
}

} // namespace ult
} // namespace L0