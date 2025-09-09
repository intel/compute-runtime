/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
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
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOriginal = nullptr;
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

HWTEST2_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesInThenSuccessReturn, IsGtRasSupportedProduct) {
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

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenGettingRasPropertiesThenSuccessIsReturned, IsGtRasSupportedProduct) {
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

HWTEST2_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForGtAndIfReadSymLinkFailsThenNoSupportedErrorTypeIsReturned, IsGtRasSupportedProduct) {
    std::set<zes_ras_error_type_t> errorType = {};

    pSysfsAccess->mockReadSymLinkResult = true;

    L0::Sysman::LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, false, 0);
    EXPECT_EQ(errorType.size(), 0u);
}

HWTEST2_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForGtAndIfListDirectoryFailsThenNoSupportedErrorTypeIsReturned, IsGtRasSupportedProduct) {
    std::set<zes_ras_error_type_t> errorType = {};

    pFsAccess->mockReadDirectoryFailure = true;

    L0::Sysman::LinuxRasSourceGt::getSupportedRasErrorTypes(errorType, pOsSysman, false, 0);
    EXPECT_EQ(errorType.size(), 0u);
}

HWTEST2_F(SysmanRasFixture, GivenValidOsSysmanPointerWhenRetrievingSupportedRasErrorsForHbmAndFwInterfaceIsAbsentThenNoSupportedErrorTypeIsReturned, IsPVC) {
    std::set<zes_ras_error_type_t> errorType = {};
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;

    L0::Sysman::LinuxRasSourceHbm::getSupportedRasErrorTypes(errorType, pOsSysman, false, 0);
    EXPECT_EQ(errorType.size(), 0u);
}

HWTEST2_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAreAbsentThenZeroHandlesAreCreated, IsGtRasSupportedProduct) {
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

HWTEST2_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAndHbmAreAbsentThenZeroHandlesAreCreated, IsPVC) {
    pRasFwUtilInterface->mockMemorySuccess = true;
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;

    pSysmanDeviceImp->pRasHandleContext->handleList.clear();
    pSysmanDeviceImp->pRasHandleContext->init(pOsSysman->getSubDeviceCount());
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfHbmAndFwInterfaceArePresentThenSuccessIsReturned, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pRasFwUtilInterface->mockMemorySuccess = true;

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, mockHandleCount);
}

HWTEST2_F(SysmanRasFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesIfRasEventsAreAbsentAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenZeroHandlesAreCreated, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pFsAccess->mockReadDirectoryWithoutRasEvents = true;
    pDrm->mockQuerySystemInfoReturnValue.push_back(true);

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtThenSuccessIsReturned, IsGtRasSupportedProduct) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPmuReadCorrectable = true;
    pRasFwUtilInterface->mockMemorySuccess = false;

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
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPsfCsc0Count + socFatalIosfPciaer + initialUncorrectableNonComputeErrors);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS], 0u);
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS], driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors);
        }
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingRasGetStateForGtAfterClearThenSuccessIsReturned, IsGtRasSupportedProduct) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPmuReadAfterClear = true;

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    ze_bool_t clear = 0;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPsfCsc0Count + socFatalIosfPciaer + initialUncorrectableNonComputeErrors);
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

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForHbmThenSuccessIsReturned, IsPVC) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);

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

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForHbmWithClearThenSuccessIsReturned, IsPVC) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);

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

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGeStateWithClearOptionWithoutPermissionsThenFailureIsReturned, IsGtRasSupportedProduct) {

    pFsAccess->mockRootUser = true;

    auto handles = getRasHandles(mockHandleCount);
    ze_bool_t clear = 1;
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesRasGetState(handle, clear, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndUnableToRetrieveConfigValuesAndOtherInterfacesAreAbsentThenFailureIsReturned, IsGtRasSupportedProduct) {

    pFsAccess->mockReadFileFailure = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndPerfEventOpenFailsAndOtherInterfacesAreAbsentThenFailureIsReturned, IsGtRasSupportedProduct) {

    pPmuInterface->mockPerfEvent = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceAndPmuReadFailsAndOtherInterfacesAreAbsentThenFailureIsReturned, IsGtRasSupportedProduct) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPmuReadResult = true;
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtInterfaceWithClearAndPmuReadFailsAndOtherInterfacesAreAbsentThenFailureIsReturned, IsGtRasSupportedProduct) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPmuReadResult = true;
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 1, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesGetRasStateForGtInterfaceAndPMUGetEventTypeFailsAndOtherInterfacesAreAbsentThenFailureIsReturned, IsGtRasSupportedProduct) {

    pFsAccess->mockReadVal = true;

    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesGetRasStateAndFirmwareInterfaceIsAbsentOtherInterfacesAreAlsoAbsentThenFailureIsReturned, IsGtRasSupportedProduct) {

    pFsAccess->mockReadVal = true;

    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    auto handles = getRasHandles(mockHandleCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 0, &state));
    }
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetConfigAfterzesRasSetConfigThenSuccessIsReturned, IsGtRasSupportedProduct) {
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

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasSetConfigWithoutPermissionThenFailureIsReturned, IsGtRasSupportedProduct) {

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

HWTEST2_F(SysmanRasFixture, SysmanRasFixture_GivenValidRasHandleWhenCallingzesRasGetStateAndReadSymLinkFailsDuringInitAndOtherInterfacesAreAbsentThenVerifyNoHandlesAreReturned, IsGtRasSupportedProduct) {

    pSysfsAccess->mockReadSymLinkStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, count);
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingzesRasGetStateAndListDirectoryFailsDuringInitAndOtherInterfacesAreAbsentThenVerifyNoHandlesAreReturned, IsGtRasSupportedProduct) {

    pFsAccess->mockListDirectoryStatus = ZE_RESULT_ERROR_NOT_AVAILABLE;
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumRasErrorSets(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, count);
}

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumRasErrorSetsSucceeds, IsGtRasSupportedProduct) {
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

HWTEST2_F(SysmanRasFixture, GivenValidRasHandleWhenCallingZesGetRasStateAndFirmwareInterfaceIsAbsentOtherInterfacesAreAlsoAbsentThenCallFails, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pFsAccess->mockReadVal = true;
    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_ras_state_t state = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasGetState(handle, 1, &state));
    }
}

TEST_F(SysmanRasFixture, GivenRasUtilAsNoneWhenCallingRasGetStateAndRasGetCategoryCountThenErrorAndNoCategoryCountIsReturned) {
    auto pRasUtil = std::make_unique<RasUtilNone>();
    zes_ras_state_t state = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasUtil->rasGetState(state, false));
    EXPECT_EQ(0u, pRasUtil->rasGetCategoryCount());
}

struct SysmanRasMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockRasFsAccess> pFsAccess;
    std::unique_ptr<MockRasSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockRasPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    MockRasNeoDrm *pDrm = nullptr;
    L0::Sysman::FsAccessInterface *pFsAccessOriginal = nullptr;
    L0::Sysman::SysFsAccessInterface *pSysfsAccessOriginal = nullptr;
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
HWTEST2_F(SysmanRasMultiDeviceFixture, GivenValidSysmanHandleWithMultiDeviceWhenRetrievingRasHandlesThenSuccessIsReturned, IsGtRasSupportedProduct) {

    L0::Sysman::RasHandleContext *pRasHandleContext = new L0::Sysman::RasHandleContext(pSysmanDeviceImp->pOsSysman);
    uint32_t count = 0;
    ze_result_t result = pRasHandleContext->rasGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ((count > 0), true);
    delete pRasHandleContext;
}
HWTEST2_F(SysmanRasMultiDeviceFixture, GivenValidSysmanHandleWhenRetrievingRasHandlesThenSuccessIsReturned, IsGtRasSupportedProduct) {
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
HWTEST2_F(SysmanRasMultiDeviceFixture, GivenValidHandleWhenGettingRasPropertiesThenSuccessIsReturned, IsGtRasSupportedProduct) {
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

HWTEST2_F(SysmanRasMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasGetStateForGtThenSuccessIsReturned, IsGtRasSupportedProduct) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
        return sizeofPath;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << pmuDriverType;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    pPmuInterface->mockPmuReadTile = true;
    pSysfsAccess->isMultiTileArch = true;

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
            EXPECT_EQ(state.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS], socFatalPsfCsc0Count + socFatalIosfPciaer + nonFatalGscAonParity + nonFataGscSelfmBist + initialUncorrectableNonComputeErrorsTile0);
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

HWTEST2_F(SysmanRasMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasGetStateForHbmThenSuccessIsReturned, IsPVC) {

    pPmuInterface->mockPmuReadResult = true;
    pRasFwUtilInterface->mockMemorySuccess = true;
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
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

} // namespace ult
} // namespace Sysman
} // namespace L0
