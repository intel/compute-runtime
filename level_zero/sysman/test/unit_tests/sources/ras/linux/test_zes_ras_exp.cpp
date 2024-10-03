/*
 * Copyright (C) 2023-2024 Intel Corporation
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
struct SysmanRasExpFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockRasFsAccess> pFsAccess;
    std::unique_ptr<MockRasSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockRasPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    MockRasNeoDrm *pDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pFsAccess = std::make_unique<MockRasFsAccess>();
        pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
        pRasFwUtilInterface = std::make_unique<MockRasFwInterface>();

        pDrm = new MockRasNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);

        pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);

        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockRasNeoDrm>(pDrm));

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        device = pSysmanDevice;
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpThenSuccessIsReturned, IsPVC) {
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
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);

    pRasFwUtilInterface->mockMemorySuccess = true;
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    uint32_t expectedErrCount = correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                }
            }
            correctable = false;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                uint32_t expectedErrCount = 0u;
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalTlb + initialUncorrectableCacheErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalEuErrorCount + initialUncorrectableComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalPsfCsc0Count + initialUncorrectableNonComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttention + initialProgrammingErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                }
            }
        }
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForGtAndLowerPCountRequestedThenSuccessIsReturned, IsGtRasSupportedProduct) {
    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

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
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        for (uint32_t i = 0; i < count; i++) {
            rasStates[i].errorCounter = 0u;
        }
        uint32_t requestedCount = correctable ? count : (count - 2);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &requestedCount, rasStates.data()));
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    uint32_t expectedErrCount = correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                    break;
                }
            }
            correctable = false;
        } else {
            uint32_t numCategoriesRetrieved = 0;
            for (uint32_t i = 0; i < count; i++) {
                uint32_t expectedErrCount = 0u;
                if (rasStates[i].errorCounter != 0) {
                    numCategoriesRetrieved++;
                }
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalTlb + initialUncorrectableCacheErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalEuErrorCount + initialUncorrectableComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalPsfCsc0Count + initialUncorrectableNonComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttention + initialProgrammingErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    expectedErrCount = socFatalMdfiEastCount + initialUncorrectableFabricErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
            EXPECT_EQ(numCategoriesRetrieved, requestedCount);
        }
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForHbmThenSuccessIsReturned, IsPVC) {

    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pRasFwUtilInterface->mockMemorySuccess = true;
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                    break;
                }
            }
            correctable = false;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                    break;
                }
            }
        }
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForGtInterfaceAndUnableToRetrieveConfigValuesAndOtherInterfacesAreAbsentThenCallFails, IsGtRasSupportedProduct) {

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

    pFsAccess->mockReadFileFailure = true;
    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesRasGetStateExp(handle, &count, rasStates.data()));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForGtInterfaceAndPerfEventOpenFailsAndOtherInterfacesAreAbsentThenCallFails, IsGtRasSupportedProduct) {

    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();
    pPmuInterface->mockPerfEvent = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesRasGetStateExp(handle, &count, rasStates.data()));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForGtInterfaceAndPmuReadFailsAndOtherInterfacesArePresentThenCallFails, IsGtRasSupportedProduct) {

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
    pPmuInterface->mockPerfEvent = false;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesRasGetStateExp(handle, &count, rasStates.data()));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForGtInterfaceAndPerfEventOpenCallFailsThenVerifyAPICallFails, IsGtRasSupportedProduct) {
    pPmuInterface->mockPmuReadResult = false;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesRasGetStateExp(handle, &count, rasStates.data()));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingZesGetRasStateExpAndFirmwareInterfaceIsAbsentOtherInterfacesAreAlsoAbsentThenCallFails, IsPVC) {
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
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesRasGetStateExp(handle, &count, rasStates.data()));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesRasClearStateExpForGtAndRasErrorsAreNotretrievedBeforeThenVerifyCategoryIsNotAvailable, IsGtRasSupportedProduct) {

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
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS));
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS));
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesRasClearStateExpAndGetStateExpForGtThenVerifyErrorCountersAreCleared, IsGtRasSupportedProduct) {

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

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pPmuInterface->mockPmuReadAfterClear = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        uint32_t expectedErrCount = 0u;
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                    EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
                    break;
                }
            }
            correctable = false;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalTlb + initialUncorrectableCacheErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalEuErrorCount + initialUncorrectableComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalPsfCsc0Count + initialUncorrectableNonComputeErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttention + initialProgrammingErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    expectedErrCount = socFatalMdfiEastCount + initialUncorrectableFabricErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS));
        }
    }
    correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                    break;
                }
            }
            correctable = false;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                }
            }
        }
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesGetClearStateExpAndFirmwareInterfaceIsAbsentOtherInterfacesAreAlsoAbsentThenCallFails, IsPVC) {
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
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesGetClearStateExpAndGetMemoryErrorFailsAndOtherInterfacesAreAlsoAbsentThenCallFails, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesGetClearStateExpAndOtherInterfacesAreAbsentThenCallSucceeds, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pRasFwUtilInterface->mockMemorySuccess = true;
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesGetClearStateExpWithoutWritePermissionsThenCallFails, IsGtRasSupportedProduct) {
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

    pFsAccess->mockRootUser = true;
    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pPmuInterface->mockPmuReadAfterClear = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_SCALE_ERRORS));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesGetClearStateExpWithInvalidCategoryThenCallFails, IsGtRasSupportedProduct) {
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

    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ENUMERATION, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_FORCE_UINT32));
    }
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleAndRasUtilInterfaceIsNullWhenCallingzesGetClearStateExpForHbmThenCallFails, IsNotPVC) {
    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceHbm>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxRasImp->osRasClearStateExp(ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleAndRasUtilInterfaceIsNullWhenCallingzesGetClearStateExpForGtThenCallFails, isRasNotSupportedProduct) {
    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    bool isSubDevice = true;
    uint32_t subDeviceId = 0u;

    auto pLinuxRasImp = std::make_unique<PublicLinuxRasImp>(pOsSysman, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId);
    pLinuxRasImp->rasSources.clear();
    pLinuxRasImp->rasSources.push_back(std::make_unique<L0::Sysman::LinuxRasSourceGt>(pLinuxSysmanImp, ZES_RAS_ERROR_TYPE_CORRECTABLE, isSubDevice, subDeviceId));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxRasImp->osRasClearStateExp(ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS));
}

HWTEST2_F(SysmanRasExpFixture, GivenValidRasHandleWhenCallingzesRasClearStateExpAndGetStateExpForHbmThenVerifyErrorCountersAreCleared, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2);
    pPmuInterface->mockPmuReadResult = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    pRasFwUtilInterface->mockMemorySuccess = true;
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCount);
    bool correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                    break;
                }
            }
            correctable = false;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                    break;
                }
            }
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
    }

    correctable = true;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (correctable == true) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                    break;
                }
            }
            correctable = false;
        } else {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                    break;
                }
            }
        }
    }
}

TEST_F(SysmanRasExpFixture, GivenRasUtilAsNoneWhenCallingRasGetStateExpAndRasClearStateExpThenErrorAreReturned) {
    auto pRasUtil = std::make_unique<RasUtilNone>();
    std::vector<zes_ras_state_exp_t> rasStates(maxRasErrorCategoryExpCount);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasUtil->rasGetStateExp(maxRasErrorCategoryExpCount, rasStates.data()));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pRasUtil->rasClearStateExp(ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
}

struct SysmanRasExpMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockRasFsAccess> pFsAccess;
    std::unique_ptr<MockRasSysfsAccess> pSysfsAccess;
    std::unique_ptr<MockRasPmuInterfaceImp> pPmuInterface;
    std::unique_ptr<MockRasFwInterface> pRasFwUtilInterface;
    MockRasNeoDrm *pDrm = nullptr;
    Drm *pOriginalDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        pDrm = new MockRasNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);

        pFsAccess = std::make_unique<MockRasFsAccess>();
        pSysfsAccess = std::make_unique<MockRasSysfsAccess>();
        pRasFwUtilInterface = std::make_unique<MockRasFwInterface>();
        pPmuInterface = std::make_unique<MockRasPmuInterfaceImp>(pLinuxSysmanImp);

        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockRasNeoDrm>(pDrm));
        device = pSysmanDevice;

        pFsAccess->mockReadDirectoryForMultiDevice = true;

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }
    std::vector<zes_ras_handle_t> getRasHandles(uint32_t count) {
        std::vector<zes_ras_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumRasErrorSets(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanRasExpMultiDeviceFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpThenSuccessIsReturned, IsPVC) {
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
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pSysfsAccess->isMultiTileArch = true;
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (handleIndex == 0u) {
            // Correctable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                uint32_t expectedErrCount = 0u;
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = correctablel3Bank + initialCorrectableCacheErrorTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = correctableGscSramEcc + initialCorrectableNonComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
        } else if (handleIndex == 1u) {
            // Uncorrectable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                uint32_t expectedErrCount = 0u;
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalTlb + initialUncorrectableCacheErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalSubslice + fatalEuErrorCount + initialUncorrectableComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalPsfCsc0Count + nonFatalGscAonParity + nonFataGscSelfmBist + initialUncorrectableNonComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttention + initialProgrammingErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    expectedErrCount = socFatalMdfiEastCount + initialUncorrectableFabricErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
        } else if (handleIndex == 2u) {
            // Correctable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                uint32_t expectedErrCount = 0u;
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = correctableSubsliceTile1 + correctableGucErrorCountTile1 + correctableSamplerErrorCountTile1 + initialCorrectableComputeErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
        } else if (handleIndex == 3u) {
            // Uncorrectable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                uint32_t expectedErrCount = 0u;
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalL3BankTile1 + fatalIdiParityErrorCountTile1 + initialUncorrectableCacheErrorsTile1; // No. of uncorrectable error type for subdevice 1
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalGucErrorCountTile1 + initialUncorrectableComputeErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalMdfiWestCountTile1 + socFatalPunitTile1 + initialUncorrectableNonComputeErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttentionTile1 + initialProgrammingErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverEngineOther + initialUncorrectableDriverErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    expectedErrCount = socFatalMdfiWestCountTile1 + initialUncorrectableFabricErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
        }
        handleIndex++;
    }
}

HWTEST2_F(SysmanRasExpMultiDeviceFixture, GivenValidRasHandleWhenCallingZesRasGetStateExpForHbmThenSuccessIsReturned, IsPVC) {

    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pPmuInterface->mockPmuReadResult = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    pRasFwUtilInterface->mockMemorySuccess = true;
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pSysfsAccess->isMultiTileArch = true;
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;

    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (handleIndex == 0u) {
            // Correctable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                    break;
                }
            }
        } else if (handleIndex == 1u) {
            // Uncorrectable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                    break;
                }
            }
        } else if (handleIndex == 2u) {
            // Correctable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                    break;
                }
            }
        } else if (handleIndex == 3u) {
            // Uncorrectable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                    break;
                }
            }
        }
        handleIndex++;
    }
}

HWTEST2_F(SysmanRasExpMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasClearStateExpAndGetStateExpForGtThenVerifyErrorCountersAreCleared, IsGtRasSupportedProduct) {

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
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pSysfsAccess->isMultiTileArch = true;
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();
    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        uint32_t expectedErrCount = 0u;
        if (handleIndex == 0u) {
            // Correctable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = correctablel3Bank + initialCorrectableCacheErrorTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = correctableGrfErrorCount + correctableEuErrorCount + initialCorrectableComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = correctableGscSramEcc + initialCorrectableNonComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
        } else if (handleIndex == 1u) {
            // Uncorrectable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalTlb + initialUncorrectableCacheErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalSubslice + fatalEuErrorCount + initialUncorrectableComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalPsfCsc0Count + nonFatalGscAonParity + nonFataGscSelfmBist + initialUncorrectableNonComputeErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttention + initialProgrammingErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverGgtt + driverRps + initialUncorrectableDriverErrors;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    expectedErrCount = socFatalMdfiEastCount + initialUncorrectableFabricErrorsTile0;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS));
        } else if (handleIndex == 2u) {
            // Correctable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = correctableSubsliceTile1 + correctableGucErrorCountTile1 + correctableSamplerErrorCountTile1 + initialCorrectableComputeErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                    EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
                }
            }
        } else if (handleIndex == 3u) {
            // Uncorrectable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    expectedErrCount = fatalL3BankTile1 + fatalIdiParityErrorCountTile1 + initialUncorrectableCacheErrorsTile1; // No. of uncorrectable error type for subdevice 1
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    expectedErrCount = fatalGucErrorCountTile1 + initialUncorrectableComputeErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    expectedErrCount = socFatalMdfiWestCountTile1 + socFatalPunitTile1 + initialUncorrectableNonComputeErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    expectedErrCount = euAttentionTile1 + initialProgrammingErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    expectedErrCount = driverMigration + driverEngineOther + initialUncorrectableDriverErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    expectedErrCount = socFatalMdfiWestCountTile1 + initialUncorrectableFabricErrorsTile1;
                    EXPECT_EQ(rasStates[i].errorCounter, expectedErrCount);
                }
            }
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS));
        }
        handleIndex++;
    }

    handleIndex = 0u;
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        // Correctable errors for Tile 0
        if (handleIndex == 0u) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                }
            }
        } else if (handleIndex == 1u) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                }
            }
        } else if (handleIndex == 2u) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                    break;
                }
            }
        } else if (handleIndex == 3u) {
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_CACHE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_DRIVER_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                } else if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, 0u);
                }
            }
        }
        handleIndex++;
    }
}

HWTEST2_F(SysmanRasExpMultiDeviceFixture, GivenValidRasHandleWhenCallingzesRasClearStateExpAndGetStateExpForHbmThenVerifyErrorCountersAreCleared, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
    pPmuInterface->mockPmuReadResult = true;
    VariableBackup<L0::Sysman::PmuInterface *> pmuBackup(&pLinuxSysmanImp->pPmuInterface);
    pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

    pRasFwUtilInterface->mockMemorySuccess = true;
    VariableBackup<L0::Sysman::FirmwareUtil *> fwBackup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = pRasFwUtilInterface.get();

    VariableBackup<L0::Sysman::FsAccessInterface *> fsBackup(&pLinuxSysmanImp->pFsAccess);
    pLinuxSysmanImp->pFsAccess = pFsAccess.get();

    pSysfsAccess->isMultiTileArch = true;
    VariableBackup<L0::Sysman::SysFsAccessInterface *> sysfsBackup(&pLinuxSysmanImp->pSysfsAccess);
    pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

    auto handles = getRasHandles(mockHandleCountForSubDevice);
    uint32_t handleIndex = 0u;

    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        if (handleIndex == 0u) {
            // Correctable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                    break;
                }
            }
        } else if (handleIndex == 1u) {
            // Uncorrectable errors for Tile 0
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                    break;
                }
            }
        } else if (handleIndex == 2u) {
            // Correctable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmCorrectableErrorCount);
                    break;
                }
            }
        } else if (handleIndex == 3u) {
            // Uncorrectable errors for Tile 1
            for (uint32_t i = 0; i < count; i++) {
                if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                    EXPECT_EQ(rasStates[i].errorCounter, hbmUncorrectableErrorCount);
                    break;
                }
            }
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasClearStateExp(handle, ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS));
        handleIndex++;
    }

    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, nullptr));
        EXPECT_NE(0u, count);
        std::vector<zes_ras_state_exp_t> rasStates(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesRasGetStateExp(handle, &count, rasStates.data()));
        for (uint32_t i = 0; i < count; i++) {
            if (rasStates[i].category == ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS) {
                EXPECT_EQ(rasStates[i].errorCounter, 0u);
                break;
            }
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
