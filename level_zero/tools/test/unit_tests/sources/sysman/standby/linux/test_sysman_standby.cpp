/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/standby/linux/os_standby_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_sysfs_standby.h"
#include "sysman/standby/os_standby.h"

#include <cmath>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

constexpr int standbyModeDefault = 1;

class SysmanStandbyFixture : public DeviceFixture, public ::testing::Test {

  protected:
    std::unique_ptr<SysmanImp> sysmanImp;
    zet_sysman_handle_t hSysman;
    zet_sysman_standby_handle_t hSysmanStandby;
    Mock<StandbySysfsAccess> *pSysfsAccess = nullptr;

    OsStandby *pOsStandby = nullptr;
    PublicLinuxStandbyImp linuxStandbyImp;
    StandbyImp *pStandbyImp = nullptr;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = std::make_unique<SysmanImp>(device->toHandle());
        pSysfsAccess = new NiceMock<Mock<StandbySysfsAccess>>;
        linuxStandbyImp.pSysfsAccess = pSysfsAccess;
        pOsStandby = static_cast<OsStandby *>(&linuxStandbyImp);
        pStandbyImp = new StandbyImp();
        pStandbyImp->pOsStandby = pOsStandby;
        pSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        ON_CALL(*pSysfsAccess, read(_, Matcher<int &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess, &Mock<StandbySysfsAccess>::getVal));

        pStandbyImp->init();
        sysmanImp->pStandbyHandleContext->handle_list.push_back(pStandbyImp);

        hSysman = sysmanImp->toHandle();
        hSysmanStandby = pStandbyImp->toHandle();
    }

    void TearDown() override {
        pStandbyImp->pOsStandby = nullptr;

        if (pSysfsAccess != nullptr) {
            delete pSysfsAccess;
            pSysfsAccess = nullptr;
        }
        DeviceFixture::TearDown();
    }
};

TEST_F(SysmanStandbyFixture, GivenComponentCountZeroWhenCallingzetSysmanStandbyGetThenNonZeroCountIsReturnedAndVerifyzetSysmanStandbyGetCallSucceeds) {
    zet_sysman_standby_handle_t standbyHandle;
    uint32_t count;

    ze_result_t result = zetSysmanStandbyGet(hSysman, &count, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_GT(count, 0u);

    uint32_t testCount = count + 1;

    result = zetSysmanStandbyGet(hSysman, &testCount, NULL);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    result = zetSysmanStandbyGet(hSysman, &count, &standbyHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle);
    EXPECT_GT(count, 0u);
}

TEST_F(SysmanStandbyFixture, GivenValidStandbyHandleWhenCallingzetSysmanStandbyGetPropertiesThenVerifyzetSysmanStandbyGetPropertiesCallSucceeds) {
    zet_standby_properties_t properties;

    ze_result_t result = zetSysmanStandbyGetProperties(hSysmanStandby, &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_STANDBY_TYPE_GLOBAL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}

TEST_F(SysmanStandbyFixture, GivenValidStandbyHandleWhenCallingzetSysmanStandbyGetModeThenVerifyzetSysmanStandbyGetModeCallSucceeds) {
    zet_standby_promo_mode_t mode;

    ze_result_t result = zetSysmanStandbyGetMode(hSysmanStandby, &mode);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_STANDBY_PROMO_MODE_DEFAULT, mode);
}

TEST_F(SysmanStandbyFixture, GivenValidStandbyHandleWhenCallingzetSysmanStandbySetModeThenVerifySysmanzetStandbySetModeCallSucceeds) {
    ze_result_t result = zetSysmanStandbySetMode(hSysmanStandby, ZET_STANDBY_PROMO_MODE_NEVER);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0
