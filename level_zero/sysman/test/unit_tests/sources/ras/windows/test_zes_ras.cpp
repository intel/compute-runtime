/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/sysman_ras_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 0u;

class SysmanRasFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRasErrorSetsThenCorrectCountIsReported) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(pSysmanDevice->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(SysmanRasFixture, GivenValidRasHandleRetrievingRasPropertiesThenSuccessIsReturned) {
    auto pRasImp = std::make_unique<L0::Sysman::RasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, false, 0);
    zes_ras_properties_t properties = {};
    pRasImp->init();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pRasImp->rasGetProperties(&properties));
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasGetStateThenFailureIsReturned) {
    auto pRasImp = std::make_unique<L0::Sysman::RasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, false, 0);
    zes_ras_state_t state = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasImp->rasGetState(&state, false));
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasGetStateExpThenFailureIsReturned) {
    auto pRasImp = std::make_unique<L0::Sysman::RasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, false, 0);
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasImp->rasGetStateExp(&pCount, nullptr));
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasClearStateExpThenFailureIsReturned) {
    auto pRasImp = std::make_unique<L0::Sysman::RasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, false, 0);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasImp->rasClearStateExp(ZES_RAS_ERROR_CATEGORY_EXP_RESET));
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasGetConfigThenFailureIsReturned) {
    auto pRasImp = std::make_unique<L0::Sysman::RasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, false, 0);
    zes_ras_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasImp->rasGetConfig(&config));
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasSetConfigThenFailureIsReturned) {
    auto pRasImp = std::make_unique<L0::Sysman::RasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, false, 0);
    zes_ras_config_t config = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasImp->rasSetConfig(&config));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
