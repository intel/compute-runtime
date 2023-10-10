/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/global_operations/windows/sysman_os_global_operations_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/global_operations/windows/mock_global_operations.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanGlobalOperationsFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::OsGlobalOperations *pOsGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperations *pGlobalOperationsPrev = nullptr;
    L0::Sysman::GlobalOperationsImp *pGlobalOperationsImp;

    std::unique_ptr<MockGlobalOpsKmdSysManager> pKmdSysManager = nullptr;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new MockGlobalOpsKmdSysManager);

        pKmdSysManager->allowSetCalls = allowSetCalls;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        pGlobalOperationsImp = static_cast<L0::Sysman::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
    }

    void TearDown() override {
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
    ze_result_t result = zesDeviceReset(pSysmanDevice->toHandle(), true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceGetStateThenFailureIsReturned) {
    init(true);
    zes_device_state_t pState = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetState(pSysmanDevice->toHandle(), &pState));
}

TEST_F(SysmanGlobalOperationsFixture, GivenValidDeviceHandleWhenCallingzesDeviceProcessesGetStateThenFailureIsReturned) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceProcessesGetState(pSysmanDevice->toHandle(), &count, nullptr));
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {
    init(false);
    ze_result_t result = zesDeviceReset(pSysmanDevice->toHandle(), true);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenDeviceInUseWhenCallingzesDeviceResetExtThenUnsupportedFeatureErrorIsReturned) {
    init(true);
    ze_result_t result = zesDeviceResetExt(pSysmanDevice->toHandle(), nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
