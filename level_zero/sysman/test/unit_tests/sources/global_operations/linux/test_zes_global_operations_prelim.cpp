/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/global_operations/linux/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanGlobalOperationsHelperFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockGlobalOperationsSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockGlobalOperationsProcfsAccess> pProcfsAccess;
    std::unique_ptr<MockGlobalOperationsFsAccess> pFsAccess;
    std::unique_ptr<MockGlobalOpsFwInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOld = nullptr;
    L0::Sysman::ProcFsAccessInterface *pProcfsAccessOld = nullptr;
    L0::Sysman::FsAccessInterface *pFsAccessOld = nullptr;
    L0::Sysman::OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;
    L0::Sysman::SysmanDeviceImp *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pProcfsAccessOld = pLinuxSysmanImp->pProcfsAccess;
        pFsAccessOld = pLinuxSysmanImp->pFsAccess;
        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pSysfsAccess = std::make_unique<MockGlobalOperationsSysfsAccess>();
        pProcfsAccess = std::make_unique<MockGlobalOperationsProcfsAccess>();
        pFsAccess = std::make_unique<MockGlobalOperationsFsAccess>();
        pMockFwInterface = std::make_unique<MockGlobalOpsFwInterface>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccess.get();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        auto pDrmLocal = new DrmGlobalOpsMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrmLocal->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        auto &osInterfaceLocal = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterfaceLocal->setDriverModel(std::unique_ptr<DrmGlobalOpsMock>(pDrmLocal));

        pFsAccess->mockReadVal = driverVersion;
        pGlobalOperationsImp = static_cast<L0::Sysman::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        if (nullptr != pGlobalOperationsImp->pOsGlobalOperations) {
            delete pGlobalOperationsImp->pOsGlobalOperations;
        }
        pGlobalOperationsImp->pOsGlobalOperations = pOsGlobalOperationsPrev;
        pGlobalOperationsImp = nullptr;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pProcfsAccess = pProcfsAccessOld;
        pLinuxSysmanImp->pFsAccess = pFsAccessOld;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateThenZesResetReasonFlagRepairedIsReturned, IsPVC) {
    pMockFwInterface->mockIfrStatus = true;
    zes_device_state_t deviceState;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(ZES_RESET_REASON_FLAG_REPAIR, deviceState.reset);
    EXPECT_EQ(ZES_REPAIR_STATUS_PERFORMED, deviceState.repaired);
}

HWTEST2_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateThenZesResetReasonFlagRepairedIsReturned, IsNotPVC) {
    pMockFwInterface->mockIfrStatus = true;

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(ZES_REPAIR_STATUS_UNSUPPORTED, deviceState.repaired);
}

HWTEST2_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateAndFirmwareRepairStatusIsFalseThenZesResetReasonFlagRepairedIsNotReturned, IsPVC) {
    pMockFwInterface->mockIfrStatus = false;

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
    EXPECT_EQ(ZES_REPAIR_STATUS_NOT_PERFORMED, deviceState.repaired);
}

HWTEST2_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateAndFirmwareRepairStatusIsFalseThenZesResetReasonFlagRepairedIsNotReturned, IsNotPVC) {
    pMockFwInterface->mockIfrStatus = false;

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
    EXPECT_EQ(ZES_REPAIR_STATUS_UNSUPPORTED, deviceState.repaired);
}

TEST_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateAndFirmwareRepairStatusFailsThenZesResetReasonFlagRepairedIsNotReturned) {
    pMockFwInterface->mockIfrError = ZE_RESULT_ERROR_UNKNOWN;

    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
    EXPECT_EQ(ZES_REPAIR_STATUS_UNSUPPORTED, deviceState.repaired);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
