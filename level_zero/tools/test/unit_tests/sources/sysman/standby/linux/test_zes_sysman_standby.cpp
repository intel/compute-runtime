/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_sysfs_standby.h"

using ::testing::_;
using ::testing::Matcher;

constexpr uint32_t standbyHandleComponentCount = 1u;
namespace L0 {
namespace ult {

constexpr int standbyModeDefault = 1;
constexpr int standbyModeNever = 0;
constexpr int standbyModeInvalid = 0xff;
constexpr uint32_t mockHandleCount = 1;
class ZesStandbyFixture : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<Mock<StandbySysfsAccess>> ptestSysfsAccess;
    zes_standby_handle_t hSysmanStandby;
    SysfsAccess *pOriginalSysfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        ptestSysfsAccess = std::make_unique<NiceMock<Mock<StandbySysfsAccess>>>();
        pOriginalSysfsAccess = pLinuxSysmanImp->pSysfsAccess;
        pLinuxSysmanImp->pSysfsAccess = ptestSysfsAccess.get();
        ptestSysfsAccess->setVal(standbyModeFile, standbyModeDefault);
        ON_CALL(*ptestSysfsAccess.get(), read(_, Matcher<int &>(_)))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::getVal));
        ON_CALL(*ptestSysfsAccess.get(), canRead(_))
            .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::getCanReadStatus));
        pSysmanDeviceImp->pStandbyHandleContext->handleList.clear();
        pSysmanDeviceImp->pStandbyHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pOriginalSysfsAccess;
    }

    std::vector<zes_standby_handle_t> get_standby_handles(uint32_t count) {
        std::vector<zes_standby_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumStandbyDomains(device, &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesStandbyFixture, GivenComponentCountZeroWhenCallingzesStandbyGetThenNonZeroCountIsReturnedAndVerifyzesStandbyGetCallSucceeds) {
    std::vector<zes_standby_handle_t> standbyHandle;
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumStandbyDomains(device, &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);

    standbyHandle.resize(count);
    result = zesDeviceEnumStandbyDomains(device, &count, standbyHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle.data());
    EXPECT_EQ(count, mockHandleCount);

    StandbyImp *ptestStandbyImp = new StandbyImp(pSysmanDeviceImp->pStandbyHandleContext->pOsSysman);
    count = 0;
    pSysmanDeviceImp->pStandbyHandleContext->handleList.push_back(ptestStandbyImp);
    result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount + 1);

    testCount = count - 1;

    standbyHandle.resize(testCount);
    result = zesDeviceEnumStandbyDomains(device, &testCount, standbyHandle.data());

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, standbyHandle.data());
    EXPECT_EQ(count, mockHandleCount + 1);

    pSysmanDeviceImp->pStandbyHandleContext->handleList.pop_back();
    delete ptestStandbyImp;
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetPropertiesThenVerifyzesStandbyGetPropertiesCallSucceeds) {
    zes_standby_properties_t properties;
    auto handles = get_standby_handles(standbyHandleComponentCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetProperties(hSysmanStandby, &properties));
        EXPECT_EQ(ZES_STANDBY_TYPE_GLOBAL, properties.type);
        EXPECT_FALSE(properties.onSubdevice);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForDefaultMode) {
    zes_standby_promo_mode_t mode;
    auto handles = get_standby_handles(standbyHandleComponentCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_DEFAULT, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallSucceedsForNeverMode) {
    zes_standby_promo_mode_t mode;
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeNever);
    auto handles = get_standby_handles(standbyHandleComponentCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
        EXPECT_EQ(ZES_STANDBY_PROMO_MODE_NEVER, mode);
    }
}

TEST_F(ZesStandbyFixture, GivenInvalidStandbyFileWhenReadisCalledThenExpectFailure) {
    zes_standby_promo_mode_t mode;
    ON_CALL(*ptestSysfsAccess.get(), read(_, Matcher<int &>(_)))
        .WillByDefault(::testing::Invoke(ptestSysfsAccess.get(), &Mock<StandbySysfsAccess>::setValReturnError));

    auto handles = get_standby_handles(standbyHandleComponentCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_NE(ZE_RESULT_SUCCESS, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbyGetModeThenVerifyzesStandbyGetModeCallFailsForInvalidMode) {
    zes_standby_promo_mode_t mode;
    ptestSysfsAccess->setVal(standbyModeFile, standbyModeInvalid);
    auto handles = get_standby_handles(standbyHandleComponentCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, zesStandbyGetMode(hSysmanStandby, &mode));
    }
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingzesStandbySetModeThenVerifySysmanzesySetModeCallFailsWithUnsupported) {
    auto handles = get_standby_handles(standbyHandleComponentCount);

    for (auto hSysmanStandby : handles) {
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesStandbySetMode(hSysmanStandby, ZES_STANDBY_PROMO_MODE_NEVER));
    }
}

} // namespace ult
} // namespace L0
