/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/ras/linux/mock_sysman_ras.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperRasXeTest = SysmanDeviceFixture;

HWTEST2_F(SysmanProductHelperRasXeTest, GivenCperOnQueryNodeIsClearedWhenCallingGetRasCperOnQueryThenFalseIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    pSysfsAccess->mockCperOnQueryVal = 0u;

    ze_bool_t enabled = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getRasCperOnQuery(pSysfsAccess.get(), &enabled));
    EXPECT_FALSE(enabled);
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenCperOnQueryNodeIsSetWhenCallingGetRasCperOnQueryThenTrueIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    pSysfsAccess->mockCperOnQueryVal = 1u;

    ze_bool_t enabled = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->getRasCperOnQuery(pSysfsAccess.get(), &enabled));
    EXPECT_TRUE(enabled);
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenSysfsReadFailsWhenCallingGetRasCperOnQueryThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    pSysfsAccess->mockCperOnQueryReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    ze_bool_t enabled = false;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pSysmanProductHelper->getRasCperOnQuery(pSysfsAccess.get(), &enabled));
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenCperOnQueryIsEnabledAndDisabledWhenCallingSetRasCperOnQueryThenNodeIsUpdated, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->setRasCperOnQuery(pSysfsAccess.get(), true));
    EXPECT_EQ(1u, pSysfsAccess->mockCperOnQueryVal);

    EXPECT_EQ(ZE_RESULT_SUCCESS, pSysmanProductHelper->setRasCperOnQuery(pSysfsAccess.get(), false));
    EXPECT_EQ(0u, pSysfsAccess->mockCperOnQueryVal);
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenSysfsWriteFailsWhenCallingSetRasCperOnQueryThenErrorIsReturned, IsCRI) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(NEO::defaultHwInfo->platform.eProductFamily);
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    pSysfsAccess->mockCperOnQueryWriteResult = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;

    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, pSysmanProductHelper->setRasCperOnQuery(pSysfsAccess.get(), true));
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenCperOnQueryNodeIsSetWhenCallingRasGetCperOnQueryExpThenTrueIsReturned, IsCRI) {
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    pSysfsAccess->mockCperOnQueryVal = 1u;

    ze_bool_t enabled = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::rasGetCperOnQueryExp(pSysmanDevice->toHandle(), &enabled));
    EXPECT_TRUE(enabled);
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenCperOnQueryIsStartedAndStoppedWhenCallingRasSetCperOnQueryExpThenNodeIsUpdated, IsCRI) {
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::rasSetCperOnQueryExp(pSysmanDevice->toHandle(), true));
    EXPECT_EQ(1u, pSysfsAccess->mockCperOnQueryVal);

    EXPECT_EQ(ZE_RESULT_SUCCESS, SysmanDevice::rasSetCperOnQueryExp(pSysmanDevice->toHandle(), false));
    EXPECT_EQ(0u, pSysfsAccess->mockCperOnQueryVal);
}

HWTEST2_F(SysmanProductHelperRasXeTest, GivenSysfsReadFailsWhenCallingRasGetCperOnQueryExpThenErrorIsReturned, IsCRI) {
    auto pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccess.get());
    pSysfsAccess->mockCperOnQueryReadResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    ze_bool_t enabled = false;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, SysmanDevice::rasGetCperOnQueryExp(pSysmanDevice->toHandle(), &enabled));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
