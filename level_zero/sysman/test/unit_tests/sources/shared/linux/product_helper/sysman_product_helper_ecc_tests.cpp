/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/header/common_matchers.h"

#include "level_zero/sysman/test/unit_tests/sources/ecc/linux/mock_ecc.h"

namespace L0 {
namespace Sysman {
namespace ult {

using isDg2OrBmg = IsAnyProducts<IGFX_DG2, IGFX_BMG>;

static int mockReadLinkEccSingleTelemNodeSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {std::string(eccSysfsPathTelem1), std::string(eccRealPathTelem1)},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        size_t toCopy = std::min(bufsize, it->second.size());
        std::memcpy(buf, it->second.c_str(), toCopy);
        return static_cast<int>(toCopy);
    }
    return -1;
}

static int mockReadLinkNoTelemNodes(const char *path, char *buf, size_t bufsize) {
    return -1;
}

inline constexpr int eccTelem1OffsetFd = 4;
inline constexpr int eccTelem1GuidFd = 5;
inline constexpr int eccTelem1TelemFd = 6;

static int mockOpenEccSuccess(const char *pathname, int flags) {
    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == std::string(eccTelem1OffsetFileName)) {
        returnValue = eccTelem1OffsetFd;
    } else if (strPathName == std::string(eccTelem1GuidFileName)) {
        returnValue = eccTelem1GuidFd;
    } else if (strPathName == std::string(eccTelem1TelemFileName)) {
        returnValue = eccTelem1TelemFd;
    }
    return returnValue;
}

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

HWTEST2_F(ZesEccFixture, GivenNoTelemNodesAvailableWhenGetEccStateIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkNoTelemNodes);
    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetEccState(device, &state));
}

HWTEST2_F(ZesEccFixture, GivenEccStateKeyMissingFromTelemMapWhenGetEccStateIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string punitGuid = "0x1e2fa030";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, punitGuid.size());
            memcpy(buf, punitGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetEccState(device, &state));
}

HWTEST2_F(ZesEccFixture, GivenGuidReadFailsWhenGetEccStateIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == eccTelem1GuidFd) {
            errno = ENOENT;
            return -1;
        }
        return count;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceGetEccState(device, &state));
}

HWTEST2_F(ZesEccFixture, GivenReadValueFailsWhenGetEccStateIsCalledThenNotAvailableIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            errno = ENOENT;
            return -1;
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesDeviceGetEccState(device, &state));
}

HWTEST2_F(ZesEccFixture, GivenEccEnabledInPmtWhenGetEccStateIsCalledThenEnabledStateIsReturned, IsCRI) {
    static uint32_t mockEccStateValue = 0x1u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, state.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_UNAVAILABLE, state.pendingState);
    EXPECT_EQ(ZES_DEVICE_ACTION_NONE, state.pendingAction);
}

HWTEST2_F(ZesEccFixture, GivenEccDisabledInPmtWhenGetEccStateIsCalledThenDisabledStateIsReturned, IsCRI) {
    static uint32_t mockEccStateValue = 0x0u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, state.currentState);
}

HWTEST2_F(ZesEccFixture, GivenOnlyNonBit0SetInEccStateWhenGetEccStateIsCalledThenDisabledStateIsReturned, IsCRI) {
    static uint32_t mockEccStateValue = 0x2u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_DISABLED, state.currentState);
}

HWTEST2_F(ZesEccFixture, GivenDefaultPropertiesExtInPNextWhenGetEccStateIsCalledThenExpectValidValues, IsCRI) {
    static uint32_t mockEccStateValue = 0x1u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    zes_device_ecc_default_properties_ext_t extProps{};
    extProps.stype = ZES_STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT;
    state.pNext = &extProps;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, state.currentState);
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_ENABLED, extProps.defaultState);
}

HWTEST2_F(ZesEccFixture, GivenWrongStypeInPNextChainWhenGetEccStateIsCalledThenDefaultStateIsNotModified, IsCRI) {
    static uint32_t mockEccStateValue = 0x1u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    zes_device_ecc_properties_t state{};
    zes_device_ecc_default_properties_ext_t extProps{};
    extProps.stype = ZES_STRUCTURE_TYPE_FORCE_UINT32;
    extProps.defaultState = ZES_DEVICE_ECC_STATE_FORCE_UINT32;
    state.pNext = &extProps;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceGetEccState(device, &state));
    EXPECT_EQ(ZES_DEVICE_ECC_STATE_FORCE_UINT32, extProps.defaultState);
}

HWTEST2_F(ZesEccFixture, GivenEccEnabledInPmtWhenGetEccAvailableIsCalledThenAvailableIsTrue, IsCRI) {
    static uint32_t mockEccStateValue = 0x1u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    ze_bool_t available = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &available));
    EXPECT_TRUE(available);
}

HWTEST2_F(ZesEccFixture, GivenEccDisabledInPmtWhenGetEccAvailableIsCalledThenAvailableIsFalse, IsCRI) {
    static uint32_t mockEccStateValue = 0x0u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkEccSingleTelemNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenEccSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validGuid = "0x5e2fa270";
        if (fd == eccTelem1OffsetFd) {
            size_t toCopy = std::min(count, sizeof(telemOffset));
            memcpy(buf, &telemOffset, toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1GuidFd) {
            size_t toCopy = std::min(count, validGuid.size());
            memcpy(buf, validGuid.data(), toCopy);
            return static_cast<ssize_t>(toCopy);
        } else if (fd == eccTelem1TelemFd) {
            size_t toCopy = std::min(count, sizeof(mockEccStateValue));
            memcpy(buf, &mockEccStateValue, toCopy);
            return static_cast<ssize_t>(toCopy);
        }
        return 0;
    });

    ze_bool_t available = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccAvailable(device, &available));
    EXPECT_FALSE(available);
}

HWTEST2_F(ZesEccFixture, GivenNoTelemNodesAvailableWhenGetEccAvailableIsCalledThenUnsupportedFeatureIsReturned, IsCRI) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkNoTelemNodes);
    ze_bool_t available = true;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDeviceEccAvailable(device, &available));
}

HWTEST2_F(ZesEccFixture, GivenCriPlatformWhenGetEccConfigurableIsCalledThenReturnsFalse, IsCRI) {
    ze_bool_t configurable = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEccConfigurable(device, &configurable));
    EXPECT_FALSE(configurable);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
