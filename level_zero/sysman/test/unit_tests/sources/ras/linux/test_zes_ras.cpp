/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/ras/linux/mock_sysman_ras.h"

class OsRas;

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockHandleCount = 2u;
constexpr uint32_t mockHandleCountForSubDevice = 4u;
struct SysmanRasFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockRasFsAccess> pFsAccess;
    std::unique_ptr<MockRasSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockRasPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    MockRasNeoDrm *pDrm = nullptr;
    L0::Sysman::FsAccess *pFsAccessOriginal = nullptr;
    L0::Sysman::SysfsAccess *pSysfsAccessOriginal = nullptr;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    L0::Sysman::FirmwareUtil *pFwUtilOriginal = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockRasFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pRasFwUtilInterface = std::make_unique<MockRasFwInterface>();

        pDrm = new MockRasNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);

        pFwUtilOriginal = pLinuxSysmanImp->pFwUtilInterface;
        pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_HBM2e);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockRasNeoDrm>(pDrm));

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        device = pSysmanDevice;
    }
    void TearDown() override {
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilOriginal;
        SysmanDeviceFixture::TearDown();
    }
    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesInThenSuccessReturn) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenGettingRasPropertiesThenSuccessIsReturned) {
    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;

    for (auto handle : handles) {
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        EXPECT_EQ(properties.pNext, nullptr);
        EXPECT_EQ(properties.onSubdevice, false);
        EXPECT_EQ(properties.subdeviceId, 0u);
        if (correctable == true) {
            EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_CORRECTABLE);
            correctable = false;
        } else {
            EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
        }
    }
}

TEST_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForGtAndIfReadSymLinkFailsThenNoSupportedErrorTypeIsReturned) {
    std::set<zes_ras_error_type_t> errorType = {};

    pSysfsAccess->mockReadSymLinkResult = true;

    L0::Sysman::LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, false, 0);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForGtAndIfListDirectoryFailsThenNoSupportedErrorTypeIsReturned) {
    std::set<zes_ras_error_type_t> errorType = {};

    pFsAccess->mockReadDirectoryFailure = true;

    L0::Sysman::LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, false, 0);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForHbmAndFwInterfaceIsAbsentThenNoSupportedErrorTypeIsReturned) {
    std::set<zes_ras_error_type_t> errorType = {};
    pLinuxSysmanImp->pFwUtilInterface = nullptr;

    L0::Sysman::LinuxRasSourceHbm::getSupportedRasErrorTypes(errorType, pOsSysman, false, 0);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAreAbsentThenZeroHandlesAreCreated) {
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, 0u);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAndHbmAreAbsentThenZeroHandlesAreCreated) {
    pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_LPDDR4);
    pRasFwUtilInterface->mockMemorySuccess = true;
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;

    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfHbmAndFwInterfaceArePresentThenSuccessIsReturned) {
    pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_HBM2);
    pRasFwUtilInterface->mockMemorySuccess = true;

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAreAbsentAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenZeroHandlesAreCreated) {
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;
    pDrm->mockQuerySystemInfoReturnValue.push_back(true);

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadCorrectable = true;
    pRasFwUtilInterface->mockMemorySuccess = false;

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        if (correctable == true) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
            correctable = false;
        } else {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], fatalTlb + initialUncorrectableCacheErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], fatalEngineResetCount + initialEngineReset);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], euAttention + initialProgrammingErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], fatalEuErrorCount + initialUncorrectableComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPsfCsc0Count + initialUncorrectableNonComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors);
        }
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasGetStateForGtAfterClearThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadAfterClear = true;

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    ze_bool_t clear = 0;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, clear, &state));
        if (correctable == true) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            correctable = false;
        } else {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], fatalEngineResetCount + initialEngineReset);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], euAttention + initialProgrammingErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], fatalEuErrorCount + initialUncorrectableComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPsfCsc0Count + initialUncorrectableNonComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], fatalTlb + initialUncorrectableCacheErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
        }
    }
    correctable = true;
    clear = 1;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, clear, &state));
        if (correctable == true) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
            correctable = false;
        } else {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
        }
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForHbmThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        if (correctable == true) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmCorrectableErrorCount);
            correctable = false;
        } else {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmUncorrectableErrorCount);
        }
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForHbmWithClearThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    ze_bool_t clear = 0;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, clear, &state));
        if (correctable == true) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmCorrectableErrorCount);
            correctable = false;
        } else {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmUncorrectableErrorCount);
        }
    }

    correctable = true;
    clear = 1;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, clear, &state));
        if (correctable == true) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
            correctable = false;
        } else {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
        }
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGeStateWithClearOptionWithoutPermissionsThenFailureIsReturned) {

    pFsAccess->mockRootUser = true;

    auto handles = getRasHandles(mockHandleCount);
    ze_bool_t clear = 1;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesRasGetState(handle, clear, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndUnableToRetrieveConfigValuesAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pFsAccess->mockReadFileFailure = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndPerfEventOpenFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pPmuInterface->mockPerfEvent = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndPmuReadFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pPmuInterface->mockPmuReadResult = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceWithClearAndPmuReadFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pPmuInterface->mockPmuReadResult = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 1, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesGetRasStateForGtInterfaceAndPMUGetEventTypeFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pFsAccess->mockReadVal = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesGetRasStateAndFirmwareInterfaceIsAbsentOtherInterfacesAreAlsoAbsentThenFailureIsReturned) {

    pFsAccess->mockReadVal = true;

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetConfigAfterzesRasSetConfigThenSuccessIsReturned) {
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_config_t setConfig = {};
        zes_ras_config_t getConfig = {};
        setConfig.totalThreshold = 50;
        memset(setConfig.detailedThresholds.category, 1, maxRasErrorCategoryCount * sizeof(uint64_t));

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasSetConfig(handle, &setConfig));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetConfig(handle, &getConfig));
        EXPECT_EQ(setConfig.totalThreshold, getConfig.totalThreshold);
        int compare = std::memcmp(setConfig.detailedThresholds.category, getConfig.detailedThresholds.category, maxRasErrorCategoryCount * sizeof(uint64_t));
        EXPECT_EQ(0, compare);
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasSetConfigWithoutPermissionThenFailureIsReturned) {

    pFsAccess->mockRootUser = true;

    auto handles = getRasHandles(mockHandleCount);

    for (auto handle : handles) {
        zes_ras_config_t setConfig = {};
        setConfig.totalThreshold = 50;
        memset(setConfig.detailedThresholds.category, 1, maxRasErrorCategoryCount * sizeof(uint64_t));
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesRasSetConfig(handle, &setConfig));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndReadSymLinkFailsDuringInitAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pSysfsAccess->mockReadSymLinkStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndReadSymLinkFailsInsideGetEventOpenAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pSysfsAccess->mockReadSymLinkStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndListDirectoryFailsDuringInitAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pFsAccess->mockListDirectoryStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumRasErrorSetsSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(pOsSysman->getSubDeviceCount());

    count = 0;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
}

struct SysmanRasMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockRasFsAccess> pFsAccess;
    std::unique_ptr<MockRasSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockRasPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    MockRasNeoDrm *pDrm = nullptr;
    L0::Sysman::FsAccess *pFsAccessOriginal = nullptr;
    L0::Sysman::SysfsAccess *pSysfsAccessOriginal = nullptr;
    L0::Sysman::PmuInterface *pOriginalPmuInterface = nullptr;
    L0::Sysman::FirmwareUtil *pFwUtilOriginal = nullptr;
    Drm *pOriginalDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pDrm = new MockRasNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockRasFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFwUtilOriginal = pLinuxSysmanImp->pFwUtilInterface;
        pRasFwUtilInterface = std::make_unique<MockRasFwInterface>();
        pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_HBM2e);
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockRasNeoDrm>(pDrm));
        device = pSysmanDevice;

        pFsAccess->mockReadDirectoryForMultiDevice = true;

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    }
    void TearDown() override {
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilOriginal;
        SysmanMultiDeviceFixture::TearDown();
    }
    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};
TEST_F(SysmanRasMultiDeviceFixture, GivenValidSysmanHandleWithMultiDeviceWhenRetrievingRasHandlesThenSuccessIsReturned) {
    L0::Sysman::RasHandleContext *pRasHandleContext = new L0::Sysman::RasHandleContext(pSysmanDeviceImp->pOsSysman);
    uint32_t count = 0;
    ze_result_t result = pRasHandleContext->rasGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ((count > 0), true);
    delete pRasHandleContext;
}
TEST_F(SysmanRasMultiDeviceFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesThenSuccessIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCountForSubDevice);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumRasErrorSets(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, mockHandleCountForSubDevice);
    auto handles = getRasHandles(mockHandleCountForSubDevice);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}
TEST_F(SysmanRasMultiDeviceFixture, GivenValidHandleWhenGettingRasPropertiesThenSuccessIsReturned) {
    zes_ras_properties_t properties = {};
    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;
    PublicLinuxRasImp *pLinuxRasImp = new PublicLinuxRasImp(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetProperties(properties));
    EXPECT_EQ(properties.subdeviceId, subDeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
    EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_CORRECTABLE);
    delete pLinuxRasImp;
}

TEST_F(SysmanRasMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadTile = true;
    pSysfsAccess->isMultiTileArch = true;

    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;
    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        if (handleIndex == 0u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], correctablel3Bank + initialCorrectableCacheErrorTile0); // No. of correctable error type for subdevice 0
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrorsTile0);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], correctableGscSramEcc + initialCorrectableNonComputeErrorsTile0);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
        } else if (handleIndex == 1u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], fatalTlb + initialUncorrectableCacheErrorsTile0); // No. of uncorrectable error type for subdevice 0
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], fatalEngineResetCount + initialEngineResetTile0);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], euAttention + initialProgrammingErrorsTile0);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], fatalSubslice + fatalEuErrorCount + initialUncorrectableComputeErrorsTile0);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPsfCsc0Count + nonFatalGscAonParity + nonFataGscSelfmBist + initialUncorrectableNonComputeErrorsTile0);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrorsTile0);
        } else if (handleIndex == 2u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], 0u); // No. of correctable error type for subdevice 1
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], correctableSubsliceTile1 + correctableGucErrorCountTile1 + correctableSamplerErrorCountTile1 + initialCorrectableComputeErrorsTile1);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], 0u);
        } else if (handleIndex == 3u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS], fatalL3BankTile1 + fatalIdiParityErrorCountTile1 + initialUncorrectableCacheErrorsTile1); // No. of uncorrectable error type for subdevice 1
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_RESET], fatalEngineResetCountTile1 + initialEngineResetTile1);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS], euAttentionTile1 + initialProgrammingErrorsTile1);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_COMPUTE_ERRORS], fatalGucErrorCountTile1 + initialUncorrectableComputeErrorsTile1);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalMdfiWestCountTile1 + socFatalPunitTile1 + initialUncorrectableNonComputeErrorsTile1);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], driverMigration + driverEngineOther + initialUncorrectableDriverErrorsTile1);
        }
        handleIndex++;
    }
}

TEST_F(SysmanRasMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasGetStateForHbmThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;

    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;

    for (auto handle : handles) {
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, 0, &state));
        if (handleIndex == 0u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmCorrectableErrorCount); // No. of correctable error type for subdevice 0
        } else if (handleIndex == 1u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmUncorrectableErrorCount); // No. of uncorrectable error type for subdevice 0
        } else if (handleIndex == 2u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmCorrectableErrorCount); // No. of correctable error type for subdevice 1
        } else if (handleIndex == 3u) {
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], hbmUncorrectableErrorCount); // No. of uncorrectable error type for subdevice 1
        }
        handleIndex++;
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
