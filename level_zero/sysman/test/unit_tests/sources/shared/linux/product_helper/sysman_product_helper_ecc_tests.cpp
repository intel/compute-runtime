/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/header/common_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/test/unit_tests/sources/ecc/linux/mock_ecc.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/fwctl/mock_fwctl.h"

namespace L0 {
namespace Sysman {
namespace ult {

using isDg2OrBmg = IsAnyProducts<IGFX_DG2, IGFX_BMG>;

struct EccFwctlBackup {
    EccFwctlBackup() {
        resetMockFwctlSysCalls();
    }
    ~EccFwctlBackup() {
        clearMockFwctlSysCalls();
    }

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> opendirBackup{&NEO::SysCalls::sysCallsOpendir, &mockFwctlOpendir};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> readdirBackup{&NEO::SysCalls::sysCallsReaddir, &mockFwctlReaddir};
    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> closedirBackup{&NEO::SysCalls::sysCallsClosedir, &mockFwctlClosedir};
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup{&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> ioctlBackup{&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess};
};

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

HWTEST2_F(ZesEccFixture, GivenEccSupportedAndEnabledWhenGetEccStateIsCalledThenEnabledStateIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 1u;

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, state.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, state.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, state.pendingAction);

    EXPECT_EQ(static_cast<uint32_t>(FWCTL_XE_SYSCTRL_OP_ECC_STATUS_GET), mockFwctlSysCalls.capturedRpcOp);
    EXPECT_EQ(static_cast<uint32_t>(FWCTL_RPC_CONFIGURATION), mockFwctlSysCalls.capturedRpcScope);
}

HWTEST2_F(ZesEccFixture, GivenEccSupportedButDisabledWhenGetEccStateIsCalledThenDisabledStateIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 0u;

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, state.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, state.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, state.pendingAction);
}

HWTEST2_F(ZesEccFixture, GivenEccNotSupportedByTheProductWhenGetEccStateIsCalledThenUnavailableStateIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 0u;
    mockFwctlSysCalls.responseData1 = 0u;

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, state.currentState);
}

HWTEST2_F(ZesEccFixture, GivenDefaultPropertiesExtInPNextWhenGetEccStateIsCalledThenDefaultStateIsReportedUnavailable, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 1u;

    zes_device_ecc_default_properties_ext_t defaultProps = {};
    defaultProps.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT;
    defaultProps.defaultState = ZES_DEVICE_ECC_STATE_ENABLED;

    zes_device_ecc_properties_t state{};
    state.pNext = &defaultProps;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, state.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, defaultProps.defaultState);
}

HWTEST2_F(ZesEccFixture, GivenWrongStypeInPNextChainWhenGetEccStateIsCalledThenDefaultStateIsNotModified, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 1u;

    zes_device_ecc_default_properties_ext_t defaultProps = {};
    defaultProps.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_PROPERTIES;
    defaultProps.defaultState = ZES_DEVICE_ECC_STATE_ENABLED;

    zes_device_ecc_properties_t state{};
    state.pNext = &defaultProps;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, defaultProps.defaultState);
}

HWTEST2_F(ZesEccFixture, GivenEccSupportedButDisabledWhenGetEccAvailableIsCalledThenAvailableIsTrue, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 0u;

    ze_bool_t available = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &available));
    EXPECT_TRUE(available);
}

HWTEST2_F(ZesEccFixture, GivenEccSupportedAndEnabledWhenGetEccAvailableIsCalledThenAvailableIsTrue, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 1u;

    ze_bool_t available = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &available));
    EXPECT_TRUE(available);
}

HWTEST2_F(ZesEccFixture, GivenEccNotSupportedByTheProductWhenGetEccAvailableIsCalledThenAvailableIsFalse, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.responseData0 = 0u;
    mockFwctlSysCalls.responseData1 = 0u;

    ze_bool_t available = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &available));
    EXPECT_FALSE(available);
}

HWTEST2_F(ZesEccFixture, GivenNoFwctlNodesAvailableWhenEccApisAreCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.opendirFails = true;

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetEccState(device, &state));

    ze_bool_t available = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceEccAvailable(device, &available));
}

HWTEST2_F(ZesEccFixture, GivenOnlyNonSysctrlFwctlNodeIsPresentWhenGetEccStateIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.nodeNames = {mockFwctlNonSysctrlNodeName};

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(0u, mockFwctlSysCalls.rpcCount);
}

HWTEST2_F(ZesEccFixture, GivenEccCapabilityIsNotReportedWhenGetEccStateIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    mockFwctlSysCalls.uctxCaps = FWCTL_XE_SYSCTRL_CAP_RAS_ERROR_INJECT;

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(0u, mockFwctlSysCalls.rpcCount);
}

HWTEST2_F(ZesEccFixture, GivenNodeCannotBeOpenedWhenGetEccStateIsCalledThenInsufficientPermissionsIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EACCES;
        return -1;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesDeviceGetEccState(device, &state));
}

HWTEST2_F(ZesEccFixture, GivenRpcFailsWhenGetEccStateIsCalledThenErrorIsPropagated, IsCRI) {
    EccFwctlBackup fwctlBackup;
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (request == fwctlRpcCmd) {
            errno = EBADMSG;
            return -1;
        }
        return mockFwctlIoctlSuccess(fd, request, arg);
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, zesDeviceGetEccState(device, &state));
}

HWTEST2_F(ZesEccFixture, GivenCriPlatformWhenGetEccConfigurableIsCalledThenReturnsFalse, IsCRI) {
    ze_bool_t configurable = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &configurable));
    EXPECT_FALSE(configurable);
}

HWTEST2_F(ZesEccFixture, GivenEccIsReadOverFwctlWhenSetEccStateIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    EccFwctlBackup fwctlBackup;

    zes_device_ecc_desc_t newState = {};
    newState.state = ZES_DEVICE_ECC_STATE_ENABLED;
    zes_device_ecc_properties_t props = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceSetEccState(device, &newState, &props));
    EXPECT_EQ(0u, mockFwctlSysCalls.openCount);
    EXPECT_EQ(0u, mockFwctlSysCalls.rpcCount);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
