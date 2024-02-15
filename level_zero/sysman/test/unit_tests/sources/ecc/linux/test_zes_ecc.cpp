/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/driver_info.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/ecc/linux/mock_ecc.h"

namespace L0 {
namespace Sysman {
namespace ult {

class ZesEccFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<MockEccFwInterface> pMockFwInterface;
    L0::Sysman::FirmwareUtil *pFwUtilInterfaceOld = nullptr;
    L0::Sysman::EccImp *pEccImp;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pFwUtilInterfaceOld = pLinuxSysmanImp->pFwUtilInterface;
        pMockFwInterface = std::make_unique<MockEccFwInterface>();
        pLinuxSysmanImp->pFwUtilInterface = pMockFwInterface.get();

        pEccImp = static_cast<L0::Sysman::EccImp *>(pSysmanDeviceImp->pEcc);
    }

    void TearDown() override {
        pEccImp = nullptr;

        pLinuxSysmanImp->pFwUtilInterface = pFwUtilInterfaceOld;
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceEccAvailableThenVerifyApiCallSucceeds, IsDG2) {
    ze_bool_t eccAvailable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(true, eccAvailable);
}

TEST_F(ZesEccFixture, GivenValidSysmanHandleAndEccUnsupportedWhenCallingzesDeviceEccAvailableThenVerifyApiCallFails) {
    struct MockSysmanProductHelperEcc : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperEcc() = default;
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperEcc>();
    std::swap(pLinuxSysmanImp->pSysmanProductHelper, pSysmanProductHelper);

    ze_bool_t eccAvailable = true;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceEccAvailable(device, &eccAvailable));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceEccConfigurableThenVerifyApiCallSucceeds, IsDG2) {
    ze_bool_t eccConfigurable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(true, eccConfigurable);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsAbsentWhenCallingEccApiThenVerifyApiCallReturnFailure, IsDG2) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    L0::Sysman::EccImp *tempEccImp = new L0::Sysman::EccImp(pOsSysman);
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

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingzesDeviceEccConfigurableAndAvailableThenVerifyApiCallReturnsFailure, IsDG2) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockFwGetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccConfigurable(device, &eccConfigurable));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndCurrentStateIsNoneWhenCallingzesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned, IsDG2) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockCurrentState = eccStateNone;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndPendingStateIsNoneWhenCallingzesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned, IsDG2) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockPendingState = eccStateNone;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceGetEccStateThenApiCallSucceeds, IsDG2) {
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingzesDeviceGetEccStateThenApiCallReturnFailure, IsDG2) {
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwGetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceGetEccState(device, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceSetEccStateThenApiCallSucceeds, IsDG2) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingzesDeviceSetEccStateWithInvalidEnumThenFailureIsReturned, IsDG2) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_UNAVAILABLE};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwSetEccConfigFailsWhenCallingzesDeviceSetEccStateThenFailureIsReturned, IsDG2) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwSetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidStatesAreReturned, IsDG2) {
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

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidActionIsReturned, IsDG2) {
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
} // namespace Sysman
} // namespace L0
