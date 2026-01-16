/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/linux/sysman_os_driver_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_os_driver.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include <level_zero/zes_api.h>

#include "gtest/gtest.h"

#include <bitset>
#include <cstring>

namespace L0 {
namespace Sysman {
namespace ult {

inline static int openMockReturnFailure(const char *pathname, int flags) {
    return -1;
}

inline static int openMockReturnSuccess(const char *pathname, int flags) {
    NEO::SysCalls::closeFuncCalled = 0;
    return 0;
}

struct dirent mockSurvivabilityEntries[] = {
    {0, 0, 0, 0, "0000:03:00.0"},
    {0, 0, 0, 0, "0000:09:00.0"},
};

struct dirent mockInvalidSurvivabilityEntries[] = {
    {0, 0, 0, 0, "."},
    {0, 0, 0, 0, "000000:03:00.0"},
    {0, 0, 0, 0, "000000:00:00.0"},
};

struct SysmanDriverTestSurvivabilityDevice : public ::testing::Test {};

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityDeviceConditionWhenDiscoverDevicesWithSurvivabilityModeIsCalledThenCorrectNumberOfDevicesIsReturned) {
    const uint32_t numEntries = sizeof(mockSurvivabilityEntries) / sizeof(mockSurvivabilityEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockSurvivabilityEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds = pOsDriverInterface->discoverDevicesWithSurvivabilityMode();
    EXPECT_EQ(2u, (uint32_t)hwSurvivabilityDeviceIds.size());
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityDeviceConditionWhenIncorrectPciBdfInfoIsFountThenZeroDevicesIsReported) {
    const uint32_t numEntries = sizeof(mockInvalidSurvivabilityEntries) / sizeof(mockInvalidSurvivabilityEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockInvalidSurvivabilityEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds = pOsDriverInterface->discoverDevicesWithSurvivabilityMode();
    EXPECT_EQ(0u, (uint32_t)hwSurvivabilityDeviceIds.size());
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityDeviceConditionAndInvalidSurvivabilitySysfsNodeWhenDiscoverDevicesWithSurvivabilityModeIsCalledThenZeroDevicesAreReturned) {
    const uint32_t numEntries = sizeof(mockSurvivabilityEntries) / sizeof(mockSurvivabilityEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnFailure};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockSurvivabilityEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds = pOsDriverInterface->discoverDevicesWithSurvivabilityMode();
    EXPECT_EQ(0u, (uint32_t)hwSurvivabilityDeviceIds.size());
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityModeConditionWhenCreateInSurvivabilityModeIsCalledThenSysmanDriverHandleIsCreated) {
    const uint32_t numEntries = sizeof(mockSurvivabilityEntries) / sizeof(mockSurvivabilityEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockSurvivabilityEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });
    SysmanDriverHandle *sysmanDriverHandle = nullptr;
    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    uint32_t driverCount = 0;
    sysmanDriverHandle = pOsDriverInterface->initSurvivabilityDevicesWithDriver(&result, &driverCount);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, driverCount);
    EXPECT_TRUE(sysmanDriverHandle != nullptr);

    delete sysmanDriverHandle;
    globalSysmanDriver = nullptr;
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityModeConditionWhenCreateInSurvivabilityModeIsCalledWithInvalidParamsThenSysmanDriverHandleIsNullPointer) {
    ze_result_t result;
    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();

    uint32_t driverCount = 0;
    auto sysmanDriverHandle = pOsDriverInterface->initSurvivabilityDevicesWithDriver(&result, &driverCount);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNINITIALIZED);
    EXPECT_TRUE(sysmanDriverHandle == nullptr);
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityModeConditionWhenSysmanApisAreInvokedThenSurvivabilityModeDetectedErrorIsReturned) {
    const uint32_t numEntries = sizeof(mockSurvivabilityEntries) / sizeof(mockSurvivabilityEntries[0]);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntries) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockSurvivabilityEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();
    ze_result_t result;

    uint32_t driverCount = 0;
    auto sysmanDriverHandle = pOsDriverInterface->initSurvivabilityDevicesWithDriver(&result, &driverCount);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(sysmanDriverHandle != nullptr);
    uint32_t count = 2;
    std::vector<zes_device_handle_t> phDevices;
    phDevices.resize(2u);
    result = sysmanDriverHandle->getDevice(&count, phDevices.data());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::performanceGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::powerGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::powerGetCardDomain(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::frequencyGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::fabricPortGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::temperatureGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::standbyGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetProperties(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetSubDeviceProperties(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::processesGetState(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetState(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceReset(phDevices[0], false));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::engineGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetState(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetBars(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::pciGetStats(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::schedulerGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::rasGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::memoryGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::fanGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::diagnosticsGet(phDevices[0], &count, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEventRegister(phDevices[0], u_int32_t(0)));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEccAvailable(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEccConfigurable(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceGetEccState(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceSetEccState(phDevices[0], nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceResetExt(phDevices[0], nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::fabricPortGetMultiPortThroughput(phDevices[0], count, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED, SysmanDevice::deviceEnumEnabledVF(phDevices[0], &count, nullptr));

    delete sysmanDriverHandle;
    globalSysmanDriver = nullptr;
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSysmanDriverHandleWhenSurvivabilityDevicesAreAddedToSameDriverHandleThenSuccessIsReturned) {
    const uint32_t numEntriesAllowed = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir(&NEO::SysCalls::sysCallsOpendir, [](const char *name) -> DIR * {
        return reinterpret_cast<DIR *>(0xc001);
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, openMockReturnSuccess};
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR * dir) -> struct dirent * {
            static uint32_t entryIndex = 0u;
            if (entryIndex >= numEntriesAllowed) {
                entryIndex = 0;
                return nullptr;
            }
            return &mockSurvivabilityEntries[entryIndex++];
        });

    VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)> mockClosedir(&NEO::SysCalls::sysCallsClosedir, [](DIR *dir) -> int {
        return 0;
    });

    std::unique_ptr<OsDriver> pOsDriverInterface = OsDriver::create();

    ze_result_t result;
    uint32_t driverCount = 0;
    auto sysmanDriverHandle = pOsDriverInterface->initSurvivabilityDevicesWithDriver(&result, &driverCount);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(sysmanDriverHandle != nullptr);
    uint32_t count = 0;
    result = sysmanDriverHandle->getDevice(&count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 1u);

    pOsDriverInterface->initSurvivabilityDevices(sysmanDriverHandle, &result);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    count = 0;
    result = sysmanDriverHandle->getDevice(&count, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 2u);

    delete sysmanDriverHandle;
    globalSysmanDriver = nullptr;
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenSurvivabilityModeConditionWhenCreateInSurvivabilityModeIsCalledWithInvalidPciBdfInfoThenSysmanDriverHandleIsNullPointer) {
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds;
    hwSurvivabilityDeviceIds.push_back(std::make_unique<NEO::HwDeviceIdDrm>(0, "000000:03:00.0", "dummy file path"));
    ze_result_t result;

    std::unique_ptr<LinuxDriverImp> pLinuxDriverImp = std::make_unique<LinuxDriverImp>();
    auto sysmanDriverHandle = pLinuxDriverImp->createInSurvivabilityMode(std::move(hwSurvivabilityDeviceIds), &result);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNINITIALIZED);
    EXPECT_TRUE(sysmanDriverHandle == nullptr);
}

TEST_F(SysmanDriverTestSurvivabilityDevice, GivenValidSurvivabilityModeDeviceWhenCalllingGetFwUtilInterfaceWithNoValidFwUtilInterfaceSupportThenReturnsNullPointer) {
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds;
    hwSurvivabilityDeviceIds.push_back(std::make_unique<NEO::HwDeviceIdDrm>(0, "0000:03:00.0", "dummy file path"));
    ze_result_t result;
    std::unique_ptr<LinuxDriverImp> pLinuxDriverImp = std::make_unique<LinuxDriverImp>();
    auto sysmanDriverHandle = pLinuxDriverImp->createInSurvivabilityMode(std::move(hwSurvivabilityDeviceIds), &result);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(sysmanDriverHandle != nullptr);
    SysmanDriverHandleImp *pSysmanDriverHandle = static_cast<SysmanDriverHandleImp *>(sysmanDriverHandle);
    auto pSysmanDevice = pSysmanDriverHandle->sysmanDevices[0];
    auto pSysmanDeviceImp = static_cast<L0::Sysman::SysmanDeviceImp *>(pSysmanDevice);
    auto pOsSysman = pSysmanDeviceImp->pOsSysman;
    auto pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    EXPECT_EQ(nullptr, pLinuxSysmanImp->getFwUtilInterface());
    delete sysmanDriverHandle;
    globalSysmanDriver = nullptr;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
