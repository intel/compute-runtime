/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_global_operations.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

class SysmanGlobalOperationsHelperFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockGlobalOperationsSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockGlobalOperationsProcfsAccess> pProcfsAccess;
    std::unique_ptr<MockGlobalOperationsFsAccess> pFsAccess;
    std::unique_ptr<MockGlobalOpsFwInterface> pMockFwInterface;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    SysfsAccess *pSysfsAccessOld = nullptr;
    ProcfsAccess *pProcfsAccessOld = nullptr;
    FsAccess *pFsAccessOld = nullptr;
    OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::GlobalOperationsImp *pGlobalOperationsImp;
    PRODUCT_FAMILY productFamily;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
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
        pSysfsAccess->mockReadVal = "0x8086";
        pFsAccess->mockReadVal = driverVersion;
        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
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

TEST_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateThenZesResetReasonFlagRepairedIsReturned) {
    pMockFwInterface->mockIfrStatus = true;
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    if (productFamily == IGFX_PVC) {
        EXPECT_EQ(ZES_RESET_REASON_FLAG_REPAIR, deviceState.reset);
        EXPECT_EQ(ZES_REPAIR_STATUS_PERFORMED, deviceState.repaired);
    } else {
        EXPECT_EQ(ZES_REPAIR_STATUS_UNSUPPORTED, deviceState.repaired);
    }
}

TEST_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateAndFirmwareRepairStatusIsFalseThenZesResetReasonFlagRepairedIsNotReturned) {
    pMockFwInterface->mockIfrStatus = false;
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
    if (productFamily == IGFX_PVC) {
        EXPECT_EQ(ZES_REPAIR_STATUS_NOT_PERFORMED, deviceState.repaired);
    } else {
        EXPECT_EQ(ZES_REPAIR_STATUS_UNSUPPORTED, deviceState.repaired);
    }
}

TEST_F(SysmanGlobalOperationsHelperFixture, GivenDeviceIsRepairedWhenCallingGetDeviceStateAndFirmwareRepairStatusFailsThenZesResetReasonFlagRepairedIsNotReturned) {
    pMockFwInterface->mockIfrError = ZE_RESULT_ERROR_UNKNOWN;
    zes_device_state_t deviceState;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetState(device, &deviceState));
    EXPECT_EQ(0u, deviceState.reset);
    EXPECT_EQ(ZES_REPAIR_STATUS_UNSUPPORTED, deviceState.repaired);
}

} // namespace ult
} // namespace L0
