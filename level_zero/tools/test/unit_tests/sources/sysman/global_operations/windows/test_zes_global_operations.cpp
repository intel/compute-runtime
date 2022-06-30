/*
 * Copyright (C) 2021 Intel Corporation
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

    Mock<GlobalOpsKmdSysManager> *pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager = new Mock<GlobalOpsKmdSysManager>;

        pKmdSysManager->allowSetCalls = allowSetCalls;

        EXPECT_CALL(*pKmdSysManager, escape(_, _, _, _, _))
            .WillRepeatedly(::testing::Invoke(pKmdSysManager, &Mock<GlobalOpsKmdSysManager>::mock_escape));

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager;

        pGlobalOperationsImp = static_cast<L0::GlobalOperationsImp *>(pSysmanDeviceImp->pGlobalOperations);
        pOsGlobalOperationsPrev = pGlobalOperationsImp->pOsGlobalOperations;
        pGlobalOperationsImp->pOsGlobalOperations = nullptr;
        pGlobalOperationsImp->init();
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
        SysmanDeviceFixture::TearDown();
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        if (pKmdSysManager != nullptr) {
            delete pKmdSysManager;
            pKmdSysManager = nullptr;
        }
    }
};

TEST_F(SysmanGlobalOperationsFixture, GivenForceTrueAndDeviceInUseWhenCallingResetThenSuccessIsReturned) {
    init(true);
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(SysmanGlobalOperationsFixture, GivenProcessStartsMidResetWhenCallingResetThenSuccessIsReturned) {
    init(false);
    pGlobalOperationsImp->init();
    ze_result_t result = zesDeviceReset(device, true);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
}

} // namespace ult
} // namespace L0
