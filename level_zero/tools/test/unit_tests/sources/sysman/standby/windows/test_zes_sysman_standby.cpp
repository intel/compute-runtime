/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/standby/standby_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/windows/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 0u;

class ZesStandbyFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesStandbyFixture, GivenValidSysmanHandleWhenRetrievingStandbyHandlesThenCorrectCountIsReported) {
    std::vector<zes_standby_handle_t> standbyHandle = {};
    uint32_t count = 0;

    ze_result_t result = zesDeviceEnumStandbyDomains(device, &count, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testCount = count + 1;

    result = zesDeviceEnumStandbyDomains(device, &testCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testCount, count);
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenRetrievingStandbyPropertiesThenSuccessIsReturned) {
    auto pStandbyImp = std::make_unique<StandbyImp>(pOsSysman, device->toHandle());
    zes_standby_properties_t properties = {};
    pStandbyImp->init();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pStandbyImp->standbyGetProperties(&properties));
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingGetModeThenFailureIsReturned) {
    auto pStandbyImp = std::make_unique<StandbyImp>(pOsSysman, device->toHandle());
    zes_standby_promo_mode_t mode = {};
    pStandbyImp->init();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pStandbyImp->standbyGetMode(&mode));
}

TEST_F(ZesStandbyFixture, GivenValidStandbyHandleWhenCallingSetModeThenFailureIsReturned) {
    auto pStandbyImp = std::make_unique<StandbyImp>(pOsSysman, device->toHandle());
    zes_standby_promo_mode_t mode = {};
    pStandbyImp->init();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pStandbyImp->standbySetMode(mode));
}

} // namespace ult
} // namespace L0