/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_standby.h"

#include <cmath>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace L0 {
namespace ult {

class SysmanStandbyFixture : public DeviceFixture, public ::testing::Test {

  protected:
    SysmanImp *sysmanImp;
    zet_sysman_handle_t hSysman;

    Mock<OsStandby> *pOsStandby;
    StandbyImp *pStandbyImp;

    zet_sysman_standby_handle_t hSysmanStandby;

    void SetUp() override {
        DeviceFixture::SetUp();
        sysmanImp = new SysmanImp(device->toHandle());
        pOsStandby = new NiceMock<Mock<OsStandby>>;
        pStandbyImp = new StandbyImp();
        pStandbyImp->pOsStandby = pOsStandby;
        pOsStandby->setMockMode(ZET_STANDBY_PROMO_MODE_DEFAULT);
        ON_CALL(*pOsStandby, getMode(_))
            .WillByDefault(Invoke(pOsStandby, &Mock<OsStandby>::getMockMode));
        ON_CALL(*pOsStandby, setMode(_))
            .WillByDefault(Invoke(pOsStandby, &Mock<OsStandby>::setMockMode));

        pStandbyImp->init();
        sysmanImp->pStandbyHandleContext->handle_list.push_back(pStandbyImp);

        hSysman = sysmanImp->toHandle();
        hSysmanStandby = pStandbyImp->toHandle();
    }

    void TearDown() override {
        if (sysmanImp != nullptr) {
            delete sysmanImp;
            sysmanImp = nullptr;
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
    zet_standby_promo_mode_t mode;

    ze_result_t result = zetSysmanStandbySetMode(hSysmanStandby, ZET_STANDBY_PROMO_MODE_NEVER);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetSysmanStandbyGetMode(hSysmanStandby, &mode);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_STANDBY_PROMO_MODE_NEVER, mode);

    result = zetSysmanStandbySetMode(hSysmanStandby, ZET_STANDBY_PROMO_MODE_DEFAULT);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zetSysmanStandbyGetMode(hSysmanStandby, &mode);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ZET_STANDBY_PROMO_MODE_DEFAULT, mode);
}

} // namespace ult
} // namespace L0
