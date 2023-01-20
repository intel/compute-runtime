/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/global_operations/windows/os_global_operations_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/global_operations/windows/mock_global_operations.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

class SysmanGlobalOperationsFixture : public SysmanDeviceFixture {
  protected:
    OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::GlobalOperationsImp *pGlobalOperationsImp;

    std::unique_ptr<Mock<GlobalOpsKmdSysManager>> pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new Mock<GlobalOpsKmdSysManager>);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
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
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueAndDeviceInUseWhenCallingResetThenSuccessIsReturned) {
    init(true);
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetStateThenFailureIsReturned) {
    init(true);
    zes_device_state_t pState = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetState(device, &pState));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceProcessesGetStateThenFailureIsReturned) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceProcessesGetState(device, &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {
    init(false);
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

} // namespace ult
} // namespace L0
