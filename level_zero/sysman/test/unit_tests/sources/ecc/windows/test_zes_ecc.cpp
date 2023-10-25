/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/driver_info.h"

#include "level_zero/sysman/test/unit_tests/sources/ecc/windows/mock_ecc.h"

using ::testing::Matcher;

namespace L0 {
namespace Sysman {
namespace ult {

static const uint8_t eccStateNone = 0xFF;
static const uint8_t eccStateEnable = 0x1;

class ZesEccFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<EccFwInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    L0::Sysman::EccImp *pEccImp;

    void SetUp() override {

        SysmanDeviceFixture::SetUp();
        pFwUtilInterfaceOld = pWddmSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<EccFwInterface>();
        pWddmSysmanImp->pFwUtilInterface = pMockFwInterface.get();

        pEccImp = static_cast<L0::Sysman::EccImp *>(pSysmanDeviceImp->pEcc);
    }

    void TearDown() override {

        pEccImp = nullptr;

        pWddmSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceEccAvailableThenVerifyApiCallSucceeds) {
    ze_bool_t eccAvailable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(pSysmanDevice->toHandle(), &eccAvailable));
    EXPECT_EQ(true, eccAvailable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceEccConfigurableThenVerifyApiCallSucceeds) {
    ze_bool_t eccConfigurable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(pSysmanDevice->toHandle(), &eccConfigurable));
    EXPECT_EQ(true, eccConfigurable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsAbsentWhenCallingEccApiThenVerifyApiCallReturnFailure) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    L0::Sysman::EccImp *tempEccImp = new L0::Sysman::EccImp(pOsSysman);
    pWddmSysmanImp->pFwUtilInterface = nullptr;
    tempEccImp->init();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->deviceEccAvailable(&eccAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->deviceEccConfigurable(&eccConfigurable));

    zes_device_ecc_desc_t newState = {};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->setEccState(&newState, &props));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->getEccState(&props));
    delete tempEccImp;
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingzesDeviceEccConfigurableAndAvailableThenVerifyApiCallReturnsFailure) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockFwGetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccAvailable(pSysmanDevice->toHandle(), &eccAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccConfigurable(pSysmanDevice->toHandle(), &eccConfigurable));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndCurrentStateIsNoneWhenCallingzesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockCurrentState = eccStateNone;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(pSysmanDevice->toHandle(), &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(pSysmanDevice->toHandle(), &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndPendingStateIsNoneWhenCallingzesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockPendingState = eccStateNone;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(pSysmanDevice->toHandle(), &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(pSysmanDevice->toHandle(), &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceGetEccStateThenApiCallSucceeds) {
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingzesDeviceGetEccStateThenApiCallReturnFailure) {
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwGetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceSetEccStateThenApiCallSucceeds) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceSetEccStateWithInvalidEnumThenFailureIsReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_UNAVAILABLE};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwSetEccConfigFailsWhenCallingzesDeviceSetEccStateThenFailureIsReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwSetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidStatesAreReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, props.pendingState);

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);

    pMockFwInterface->mockSetConfig = false;
    pMockFwInterface->mockCurrentState = eccStateNone;
    pMockFwInterface->mockPendingState = eccStateNone;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, props.currentState);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidActionIsReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockCurrentState = eccStateEnable;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(pSysmanDevice->toHandle(), &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_WARM_CARD_RESET, props.pendingAction);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
