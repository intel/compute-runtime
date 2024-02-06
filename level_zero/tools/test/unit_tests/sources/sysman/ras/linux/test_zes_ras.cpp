/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_const.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/ras/linux/mock_sysman_ras.h"

extern bool sysmanUltsEnable;

class OsRas;
namespace L0 {
namespace ult {
constexpr uint32_t mockHandleCount = 2u;
constexpr uint32_t mockHandleCountForSubDevice = 4u;
struct SysmanRasFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockRasFsAccess> pFsAccess;
    std::unique_ptr<MockRasSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockRasPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    std::unique_ptr<MockRasNeoDrm> pDrm;
    MemoryManager *pMemoryManagerOriginal = nullptr;
    std::unique_ptr<MockMemoryManagerInRasSysman> pMemoryManager;
    FsAccess *pFsAccessOriginal = nullptr;
    Drm *pOriginalDrm = nullptr;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    PmuInterface *pOriginalPmuInterface = nullptr;
    FirmwareUtil *pFwUtilOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pMemoryManagerOriginal = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = std::make_unique<MockMemoryManagerInRasSysman>(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = true;
        device->getDriverHandle()->setMemoryManager(pMemoryManager.get());
        pFsAccess = std::make_unique<MockRasFsAccess>();
        pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
        pRasFwUtilInterface = std::make_unique<MockRasFwInterface>();
        pDrm = std::make_unique<MockRasNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20>(*pDrm));
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pFwUtilOriginal = pLinuxSysmanImp->pFwUtilInterface;
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();
        pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pLinuxSysmanImp->pDrm = pDrm.get();

        for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pRasHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOriginal);
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilOriginal;
        pLinuxSysmanImp->pDrm = pOriginalDrm;
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
        ASSERT_NE(nullptr, handle);
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

    LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, device->toHandle());
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForGtAndIfListDirectoryFailsThenNoSupportedErrorTypeIsReturned) {
    std::set<zes_ras_error_type_t> errorType = {};

    pFsAccess->mockReadDirectoryFailure = true;

    LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, device);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForHbmAndFwInterfaceIsAbsentThenNoSupportedErrorTypeIsReturned) {
    std::set<zes_ras_error_type_t> errorType = {};
    pLinuxSysmanImp->pFwUtilInterface = nullptr;

    LinuxRasSourceHbm::getSupportedRasErrorTypes(errorType, pOsSysman, device);
    EXPECT_EQ(errorType.size(), 0u);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAreAbsentThenZeroHandlesAreCreated) {
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
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
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr4);
    pRasFwUtilInterface->mockMemorySuccess = true;
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfHbmAndFwInterfaceArePresentThenSuccessIsReturned) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2);
    pRasFwUtilInterface->mockMemorySuccess = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAreAbsentAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenZeroHandlesAreCreated) {
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;
    pDrm->mockQuerySystemInfoReturnValue.push_back(true);

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadCorrectable = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGeStateForGtAfterClearThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadAfterClear = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }

    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    ze_bool_t clear = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetState(handle, clear, &state));
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

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGeStateForHbmThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }

    pSysmanDeviceImp->pRasHandleContext->handleList.clear();

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGeStateForHbmWithClearThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    ze_bool_t clear = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesRasGetState(handle, clear, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndUnableToRetrieveConfigValuesAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pFsAccess->mockReadFileFailure = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndPerfEventOpenFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pPmuInterface->mockPerfEvent = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndPmuReadFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pPmuInterface->mockPmuReadResult = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceWithClearAndPmuReadFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pPmuInterface->mockPmuReadResult = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 1, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesGetRasStateForGtInterfaceAndPMUGetEventTypeFailsAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pFsAccess->mockReadVal = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesGetRasStateAndFirmwareInterfaceIsAbsentOtherInterfacesAreAlsoAbsentThenFailureIsReturned) {

    pFsAccess->mockReadVal = true;

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetConfigAfterzesRasSetConfigThenSuccessIsReturned) {
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
        zes_ras_config_t setConfig = {};
        setConfig.totalThreshold = 50;
        memset(setConfig.detailedThresholds.category, 1, maxRasErrorCategoryCount * sizeof(uint64_t));
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesRasSetConfig(handle, &setConfig));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndReadSymLinkFailsDuringInitAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pSysfsAccess->mockReadSymLinkStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndReadSymLinkFailsInsideGetEventOpenAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pSysfsAccess->mockReadSymLinkStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

TEST_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndListDirectoryFailsDuringInitAndOtherInterfacesAreAbsentThenFailureIsReturned) {

    pFsAccess->mockListDirectoryStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

    pLinuxSysmanImp->reInitSysmanDeviceResources();

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
    MemoryManager *pMemoryManagerOriginal = nullptr;
    std::unique_ptr<MockMemoryManagerInRasSysman> pMemoryManager;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    std::unique_ptr<MockRasNeoDrm> pDrm;
    FsAccess *pFsAccessOriginal = nullptr;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    PmuInterface *pOriginalPmuInterface = nullptr;
    FirmwareUtil *pFwUtilOriginal = nullptr;
    Drm *pOriginalDrm = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        pMemoryManagerOriginal = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = std::make_unique<MockMemoryManagerInRasSysman>(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = true;
        device->getDriverHandle()->setMemoryManager(pMemoryManager.get());
        pDrm = std::make_unique<MockRasNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20>(*pDrm));
        pFsAccess = std::make_unique<MockRasFsAccess>();
        pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
        pRasFwUtilInterface = std::make_unique<MockRasFwInterface>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pFwUtilOriginal = pLinuxSysmanImp->pFwUtilInterface;
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();
        pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pLinuxSysmanImp->pDrm = pDrm.get();

        pFsAccess->mockReadDirectoryForMultiDevice = true;

        for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pRasHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOriginal);
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pFwUtilInterface = pFwUtilOriginal;
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        SysmanMultiDeviceFixture::TearDown();
    }
    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};
TEST_F(SysmanRasMultiDeviceFixture, GivenValidSysmanHandleWithMultiDeviceWhenRetrievingRasHandlesThenSuccessIsReturned) {
    RasHandleContext *pRasHandleContext = new RasHandleContext(pSysmanDeviceImp->pOsSysman);
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
    for (auto deviceHandle : deviceHandles) {
        zes_ras_properties_t properties = {};
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        bool isSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
        PublicLinuxRasImp *pLinuxRasImp = new PublicLinuxRasImp(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, deviceProperties.subdeviceId);
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxRasImp->osRasGetProperties(properties));
        EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
        EXPECT_EQ(properties.onSubdevice, isSubDevice);
        EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_CORRECTABLE);
        delete pLinuxRasImp;
    }
}

TEST_F(SysmanRasMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasGeStateForGtThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadTile = true;
    pSysfsAccess->isMultiTileArch = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPunitTile1 + initialUncorrectableNonComputeErrorsTile1);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], driverMigration + driverEngineOther + initialUncorrectableDriverErrorsTile1);
        }
        handleIndex++;
    }
}

TEST_F(SysmanRasMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasGeStateForHbmThenSuccessIsReturned) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;

    for (const auto &handle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        delete handle;
    }
    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

class SysmanRasAffinityMaskFixture : public SysmanRasMultiDeviceFixture {
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0.1");
        SysmanRasMultiDeviceFixture::SetUp();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanRasMultiDeviceFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

TEST_F(SysmanRasAffinityMaskFixture, GivenAffinityMaskIsSetWhenCallingRasPropertiesThenPropertiesAreReturnedForTheSubDevicesAccordingToAffinityMask) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
    auto handles = getRasHandles(mockHandleCount);
    uint32_t handleIndex = 0u;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetProperties(handle, &properties));
        EXPECT_EQ(properties.pNext, nullptr);
        EXPECT_EQ(properties.onSubdevice, true);
        EXPECT_EQ(properties.subdeviceId, 1u); // Affinity mask 0.1 is set which means only subdevice 1 is exposed
        if (handleIndex == 0u) {
            EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_CORRECTABLE);

        } else if (handleIndex == 1u) {
            EXPECT_EQ(properties.type, ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
        }
        handleIndex++;
    }
}

} // namespace ult
} // namespace L0
