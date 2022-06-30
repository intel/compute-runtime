/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/firmware_util/mock_fw_util_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {

namespace ult {

TEST(LinuxFwEccTest, GivenGetProcAddrCallFailsWhenFirmwareUtilChecksEccGetAndSetConfigThenCorrespondingCallFails) {

    if (!sysmanUltsEnable) {
        GTEST_SKIP();
    }

    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(0, 0, 0, 0);
    pFwUtilImp->libraryHandle = static_cast<OsLibrary *>(new MockOsLibrary());
    uint8_t currentState = 0;
    uint8_t pendingState = 0;
    uint8_t newState = 0;
    auto ret = pFwUtilImp->fwGetEccConfig(&currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
    ret = pFwUtilImp->fwSetEccConfig(newState, &currentState, &pendingState);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ret);
    delete pFwUtilImp->libraryHandle;
    pFwUtilImp->libraryHandle = nullptr;
    delete pFwUtilImp;
}

} // namespace ult
} // namespace L0
