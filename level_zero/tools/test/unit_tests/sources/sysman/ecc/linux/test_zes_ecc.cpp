/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/ecc/linux/mock_ecc.h"

extern bool sysmanUltsEnable;

using ::testing::Matcher;

namespace L0 {
namespace ult {

static const uint8_t eccStateNone = 0xFF;
static const uint8_t eccStateEnable = 0x1;

class ZesEccFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<Mock<EccFwInterface>> pMockFwInterface;
    FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    L0::EccImp *pEccImp;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<NiceMock<Mock<EccFwInterface>>>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();

        pEccImp = static_cast<L0::EccImp *>(pSysmanDeviceImp->pEcc);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pEccImp = nullptr;

        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceEccAvailableThenVerifyApiCallSucceeds) {
    ze_bool_t eccAvailable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(true, eccAvailable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceEccConfigurableThenVerifyApiCallSucceeds) {
    ze_bool_t eccConfigurable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(true, eccConfigurable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsAbsentWhenCallingEccApiThenVerifyApiCallReturnFailure) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    EccImp *tempEccImp = new EccImp(pOsSysman);
    auto deviceImp = static_cast<L0::DeviceImp *>(pLinuxSysmanImp->getDeviceHandle());
    deviceImp->driverInfo.reset(nullptr);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    tempEccImp->init();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->deviceEccAvailable(&eccAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->deviceEccConfigurable(&eccConfigurable));

    zes_device_ecc_desc_t newState = {};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->setEccState(&newState, &props));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, tempEccImp->getEccState(&props));
    delete tempEccImp;
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwDeviceInitFailsThenVerifyApiCallReturnFailure) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockFwDeviceInit = ZE_RESULT_ERROR_UNKNOWN;

    EccImp *tempEccImp = new EccImp(pOsSysman);
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

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccConfigurable(device, &eccConfigurable));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndCurrentStateIsNoneWhenCallingzesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockCurrentState = eccStateNone;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndPendingStateIsNoneWhenCallingzesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockPendingState = eccStateNone;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceGetEccStateThenApiCallSucceeds) {
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingzesDeviceGetEccStateThenApiCallReturnFailure) {
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwGetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceGetEccState(device, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceSetEccStateThenApiCallSucceeds) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceSetEccStateWithInvalidEnumThenFailureIsReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_UNAVAILABLE};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesDeviceSetEccState(device, &newState, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndFwSetEccConfigFailsWhenCallingzesDeviceSetEccStateThenFailureIsReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwSetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceSetEccState(device, &newState, &props));
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidStatesAreReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, props.pendingState);

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);

    pMockFwInterface->mockSetConfig = false;
    pMockFwInterface->mockCurrentState = eccStateNone;
    pMockFwInterface->mockPendingState = eccStateNone;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, props.currentState);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidActionIsReturned) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockCurrentState = eccStateEnable;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_WARM_CARD_RESET, props.pendingAction);
}

} // namespace ult
} // namespace L0
