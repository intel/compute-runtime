/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_fs_ras.h"

using ::testing::_;
using ::testing::Matcher;
using ::testing::NiceMock;

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 0;
struct SysmanRasFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pSysmanDeviceImp->pRasHandleContext->handleList.clear();
        pSysmanDeviceImp->pRasHandleContext->init();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_ras_handle_t> get_ras_handles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasZeroHandlesInReturn) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);

    count = 0;
    std::vector<zes_ras_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount);

    RasImp *pTestRasImp = new RasImp(pSysmanDeviceImp->pRasHandleContext->pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE);
    pSysmanDeviceImp->pRasHandleContext->handleList.push_back(pTestRasImp);
    EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, mockHandleCount + 1);

    testcount = count;

    handles.resize(testcount);
    EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(testcount, mockHandleCount + 1);
    EXPECT_NE(nullptr, handles.data());

    pSysmanDeviceImp->pRasHandleContext->handleList.pop_back();
    delete pTestRasImp;
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenGettingRasPropertiesThenSuccessIsReturned) {
    RasImp *pTestRasImp = new RasImp(pSysmanDeviceImp->pRasHandleContext->pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE);
    pSysmanDeviceImp->pRasHandleContext->handleList.push_back(pTestRasImp);

    auto handles = get_ras_handles(mockHandleCount + 1);

    for (auto handle : handles) {
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        EXPECT_EQ(properties.pNext, nullptr);
        EXPECT_EQ(properties.onSubdevice, false);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_CORRECTABLE);
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.pop_back();
    delete pTestRasImp;
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhileCallingzesRasGetStateThenFailureIsReturned) {
    RasImp *pTestRasImp = new RasImp(pSysmanDeviceImp->pRasHandleContext->pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE);
    pSysmanDeviceImp->pRasHandleContext->handleList.push_back(pTestRasImp);

    auto handles = get_ras_handles(mockHandleCount + 1);

    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.pop_back();
    delete pTestRasImp;
}

} // namespace ult
} // namespace L0
