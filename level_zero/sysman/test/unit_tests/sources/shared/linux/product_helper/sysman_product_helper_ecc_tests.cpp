/*
 * Copyright (C) 2025 Intel Corporation
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

using isDg2OrBmg = IsAnyProducts<IGFX_DG2, IGFX_BMG>;

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

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceEccAvailableTwiceThenVerifyApiCallSucceeds, isDg2OrBmg) {
    ze_bool_t eccAvailable = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(true, eccAvailable);

    eccAvailable = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(true, eccAvailable);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceEccConfigurableTwiceThenVerifyApiCallSucceeds, isDg2OrBmg) {
    ze_bool_t eccConfigurable = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(true, eccConfigurable);

    eccConfigurable = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(true, eccConfigurable);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsAbsentWhenCallingEccApiThenVerifyApiCallReturnFailure, isDg2OrBmg) {
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

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingZesDeviceEccConfigurableAndAvailableThenVerifyApiCallReturnsFailure, isDg2OrBmg) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockFwGetEccAvailableResult = ZE_RESULT_ERROR_UNINITIALIZED;
    pMockFwInterface->mockFwGetEccConfigurableResult = ZE_RESULT_ERROR_UNINITIALIZED;

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceEccConfigurable(device, &eccConfigurable));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndCurrentStateIsNoneWhenCallingZesDeviceEccConfigurableAndAvailableThenNotSupportedEccIsReturned, isDg2OrBmg) {
    ze_bool_t eccConfigurable = true;
    ze_bool_t eccAvailable = true;
    pMockFwInterface->mockEccAvailable = false;
    pMockFwInterface->mockEccConfigurable = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &eccAvailable));
    EXPECT_EQ(false, eccAvailable);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &eccConfigurable));
    EXPECT_EQ(false, eccConfigurable);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceGetEccStateThenApiCallSucceeds, isDg2OrBmg) {
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceGetEccStateWithDefaultExtensionStructureThenApiCallSucceeds, isDg2OrBmg) {
    zes_device_ecc_properties_t props = {};
    zes_device_ecc_default_properties_ext_t extProps = {};
    extProps.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT;
    props.pNext = &extProps;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, extProps.defaultState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceGetEccStateWithWrongDefaultExtensionStructureThenApiCallSucceeds, isDg2OrBmg) {
    zes_device_ecc_properties_t props = {};
    zes_device_ecc_default_properties_ext_t extProps = {};
    extProps.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    props.pNext = &extProps;
    extProps.defaultState = ZES_DEVICE_ECC_STATE_FORCE_UINT32;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(pSysmanDevice->toHandle(), &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, props.pendingState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_FORCE_UINT32, extProps.defaultState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, props.pendingAction);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwGetEccConfigFailsWhenCallingZesDeviceGetEccStateThenApiCallReturnFailure, isDg2OrBmg) {
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwGetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceGetEccState(device, &props));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, props.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, props.pendingState);
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceSetEccStateThenApiCallSucceeds, isDg2OrBmg) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));

    newState.state = ZES_DEVICE_ECC_STATE_DISABLED;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceSetEccStateWithUnavailableEnumThenUnsupportedIsReturned, isDg2OrBmg) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_UNAVAILABLE};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwInterfaceIsPresentWhenCallingZesDeviceSetEccStateWithInvalidEnumThenFailureIsReturned, isDg2OrBmg) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_FORCE_UINT32};
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleAndFwSetEccConfigFailsWhenCallingZesDeviceSetEccStateThenFailureIsReturned, isDg2OrBmg) {
    zes_device_ecc_desc_t newState = {ZES_STRUCTURE_TYPE_DEVICE_STATE, nullptr, ZES_DEVICE_ECC_STATE_ENABLED};
    zes_device_ecc_properties_t props = {};
    pMockFwInterface->mockFwSetEccConfigResult = ZE_RESULT_ERROR_UNINITIALIZED;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDeviceSetEccState(device, &newState, &props));
}

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidStatesAreReturned, isDg2OrBmg) {
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

HWTEST2_F(ZesEccFixture, GivenValidSysmanHandleWhenCallingEccSetStateAndEccGetStateThenVerifyApiCallSuccedsAndValidActionIsReturned, isDg2OrBmg) {
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
