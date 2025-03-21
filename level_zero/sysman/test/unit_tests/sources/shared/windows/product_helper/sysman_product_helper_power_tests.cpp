/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/power/windows/sysman_os_power_imp.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/power/windows/mock_power.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::wstring pmtInterfaceName = L"TEST\0";
std::vector<wchar_t> pmtInterfacePower(pmtInterfaceName.begin(), pmtInterfaceName.end());

const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMap = {
    {{"XTAL_CLK_FREQUENCY", {1, 0}},
     {"XTAL_COUNT", {128, 0}},
     {"VCCGT_ENERGY_ACCUMULATOR", {407, 0}},
     {"VCCDDR_ENERGY_ACCUMULATOR", {410, 0}},
     {"PACKAGE_ENERGY_STATUS_SKU", {34, 1}},
     {"PLATFORM_ENERGY_STATUS", {35, 1}}}};

const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMapToGetEnergyCounterFromPunit = {
    {{"XTAL_CLK_FREQUENCY", {1, 0}},
     {"ACCUM_PACKAGE_ENERGY", {12, 0}},
     {"ACCUM_PSYS_ENERGY", {13, 0}},
     {"XTAL_COUNT", {128, 0}},
     {"VCCGT_ENERGY_ACCUMULATOR", {407, 0}},
     {"VCCDDR_ENERGY_ACCUMULATOR", {410, 0}},
     {"PACKAGE_ENERGY_STATUS_SKU", {34, 1}},
     {"PLATFORM_ENERGY_STATUS", {35, 1}}}};

static const std::vector<double> indexToXtalClockFrequecyMap = {24, 19.2, 38.4, 25};
constexpr uint32_t powerHandleDomainCount = 4u;

class SysmanProductHelperPowerTest : public SysmanDeviceFixture {

  protected:
    std::unique_ptr<PowerKmdSysManager> pKmdSysManager;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }

    void init(bool allowSetCalls) {
        pKmdSysManager.reset(new PowerKmdSysManager);
        pKmdSysManager->allowSetCalls = allowSetCalls;
        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        for (auto handle : pSysmanDeviceImp->pPowerHandleContext->handleList) {
            delete handle;
        }

        auto pPmt = new PublicPlatformMonitoringTech(pmtInterfacePower, pWddmSysmanImp->getSysmanProductHelper());
        pPmt->keyOffsetMap = dummyKeyOffsetMap;
        pWddmSysmanImp->pPmt.reset(pPmt);
        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    }

    void updatePmtKeyOffsetMap(const std::map<std::string, std::pair<uint32_t, uint32_t>> keyOffsetMap) {
        auto pPmt = new PublicPlatformMonitoringTech(pmtInterfacePower, pWddmSysmanImp->getSysmanProductHelper());
        pPmt->keyOffsetMap = keyOffsetMap;
        pWddmSysmanImp->pPmt.reset(pPmt);
        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
    }

    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanProductHelperPowerTest, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds, IsBMG) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleDomainCount);
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenInvalidComponentCountWhenEnumeratingPowerDomainsThenValidCountIsReturnedAndVerifySysmanPowerGetCallSucceeds, IsBMG) {
    init(true);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleDomainCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleDomainCount);
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrieved, IsBMG) {
    static constexpr uint32_t mockPmtEnergyCounterVariableBackupValue = 100;
    static constexpr uint32_t mockPmtTimestampVariableBackupValue = 10000000;
    static constexpr uint32_t mockPmtFrequencyIndexVariableBackupValue = 2;

    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 1:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockPmtFrequencyIndexVariableBackupValue;
            return true;
        case 128:
            *lpBytesReturned = 8;
            *static_cast<uint64_t *>(lpOutBuffer) = mockPmtTimestampVariableBackupValue;
            return true;
        default:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockPmtEnergyCounterVariableBackupValue;
            return true;
        }
    });
    // Setting allow set calls or not
    init(true);

    // Calculate the expected energy counter value from the mockPmtEnergyCounterVariableBackupValue
    uint32_t integerPart = static_cast<uint32_t>(mockPmtEnergyCounterVariableBackupValue >> 14);
    uint32_t decimalBits = static_cast<uint32_t>((mockPmtEnergyCounterVariableBackupValue & 0x3FFF));
    double decimalPart = static_cast<double>(decimalBits) / (1 << 14);
    double result = static_cast<double>(integerPart + decimalPart);
    uint64_t expectedEnergyCounterValue = static_cast<uint64_t>((result * convertJouleToMicroJoule));

    auto handles = getPowerHandles(powerHandleDomainCount);
    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter;

        ze_result_t result = zesPowerGetEnergyCounter(handle, &energyCounter);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounterValue);
        EXPECT_EQ(energyCounter.timestamp, static_cast<uint64_t>(mockPmtTimestampVariableBackupValue / indexToXtalClockFrequecyMap[mockPmtFrequencyIndexVariableBackupValue]));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenGettingPowerEnergyCounterAndBothPunitAndOobmsmBasedEnergyCounterIsAvailableThenValidPowerReadingsRetrieved, IsBMG) {

    static constexpr uint32_t mockPmtEnergyCounterVariableFromOobmsmBackupValue = 100;
    static constexpr uint32_t mockPmtEnergyCounterVariableFromPunitBackupValue = 90;
    static constexpr uint32_t mockPmtTimestampVariableBackupValue = 10000000;
    static constexpr uint32_t mockPmtFrequencyIndexVariableBackupValue = 2;

    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 1:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockPmtFrequencyIndexVariableBackupValue;
            return true;
        case 12:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockPmtEnergyCounterVariableFromPunitBackupValue;
            return true;
        case 13:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockPmtEnergyCounterVariableFromPunitBackupValue;
            return true;
        case 128:
            *lpBytesReturned = 8;
            *static_cast<uint64_t *>(lpOutBuffer) = mockPmtTimestampVariableBackupValue;
            return true;
        default:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockPmtEnergyCounterVariableFromOobmsmBackupValue;
            return true;
        }
    });
    // Setting allow set calls or not
    init(true);
    updatePmtKeyOffsetMap(dummyKeyOffsetMapToGetEnergyCounterFromPunit);

    // Calculate the expected energy counter value from the mockPmtEnergyCounterVariableFromOobmsmBackupValue
    uint32_t integerPart = static_cast<uint32_t>(mockPmtEnergyCounterVariableFromOobmsmBackupValue >> 14);
    uint32_t decimalBits = static_cast<uint32_t>((mockPmtEnergyCounterVariableFromOobmsmBackupValue & 0x3FFF));
    double decimalPart = static_cast<double>(decimalBits) / (1 << 14);
    double result = static_cast<double>(integerPart + decimalPart);
    uint64_t expectedEnergyCounterValueFromOobmsm = static_cast<uint64_t>((result * convertJouleToMicroJoule));

    // Calculate the expected energy counter value from the mockPmtEnergyCounterVariableFromPunitBackupValue
    integerPart = static_cast<uint32_t>(mockPmtEnergyCounterVariableFromPunitBackupValue >> 14);
    decimalBits = static_cast<uint32_t>((mockPmtEnergyCounterVariableFromPunitBackupValue & 0x3FFF));
    decimalPart = static_cast<double>(decimalBits) / (1 << 14);
    result = static_cast<double>(integerPart + decimalPart);
    uint64_t expectedEnergyCounterValueFromPunit = static_cast<uint64_t>((result * convertJouleToMicroJoule));

    auto handles = getPowerHandles(powerHandleDomainCount);
    for (auto handle : handles) {
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};
        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);

        zes_power_energy_counter_t energyCounter;
        result = zesPowerGetEnergyCounter(handle, &energyCounter);

        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (extProperties.domain == ZES_POWER_DOMAIN_CARD || extProperties.domain == ZES_POWER_DOMAIN_PACKAGE) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterValueFromPunit);
        } else {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterValueFromOobmsm);
        }
        EXPECT_EQ(energyCounter.timestamp, static_cast<uint64_t>(mockPmtTimestampVariableBackupValue / indexToXtalClockFrequecyMap[mockPmtFrequencyIndexVariableBackupValue]));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenInValidPowerDomainWhenGettingPowerEnergyCounterThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_UNKNOWN);
    zes_power_energy_counter_t energyCounter = {};
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPmtHandleWhenCallingZesPowerGetEnergyCounterAndIoctlFailsThenCallsFails, IsBMG) {
    static uint32_t count = 3;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 1:
            *lpBytesReturned = 4;
            return (count == 3) ? false : true;
        case 128:
            *lpBytesReturned = 8;
            return (count == 2) ? false : true;
        default:
            *lpBytesReturned = 4;
            return (count == 1) ? false : true;
        }
    });

    init(true);
    auto handles = getPowerHandles(powerHandleDomainCount);
    while (count) {
        zes_power_energy_counter_t energyCounter;
        ze_result_t result = zesPowerGetEnergyCounter(handles[0], &energyCounter);
        EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
        count--;
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenNullPmtHandleWhenCallingZesPowerGetEnergyCounterThenCallFails, IsBMG) {
    init(true);
    auto handles = getPowerHandles(powerHandleDomainCount);
    pWddmSysmanImp->pPmt.reset(nullptr);
    zes_power_energy_counter_t energyCounter;
    ze_result_t result = zesPowerGetEnergyCounter(handles[0], &energyCounter);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesThenCallSucceeds, IsBMG) {
    // Setting allow set calls or not
    init(true);
    auto handles = getPowerHandles(powerHandleDomainCount);
    for (auto handle : handles) {
        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        zes_power_limit_ext_desc_t defaultLimit = {};
        extProperties.defaultLimit = &defaultLimit;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_TRUE(defaultLimit.limitValueLocked);
        EXPECT_TRUE(defaultLimit.enabledStateLocked);
        EXPECT_EQ(ZES_POWER_SOURCE_ANY, defaultLimit.source);
        EXPECT_EQ(ZES_LIMIT_UNIT_POWER, defaultLimit.limitUnit);

        if (extProperties.domain == ZES_POWER_DOMAIN_CARD || extProperties.domain == ZES_POWER_DOMAIN_PACKAGE) {
            EXPECT_TRUE(properties.canControl);
            EXPECT_TRUE(properties.isEnergyThresholdSupported);
            EXPECT_EQ(static_cast<uint32_t>(properties.maxLimit), pKmdSysManager->mockMaxPowerLimit);
            EXPECT_EQ(static_cast<uint32_t>(properties.minLimit), pKmdSysManager->mockMinPowerLimit);
            EXPECT_EQ(static_cast<uint32_t>(properties.defaultLimit), pKmdSysManager->mockTpdDefault);
            EXPECT_EQ(defaultLimit.limit, static_cast<int32_t>(pKmdSysManager->mockTpdDefault));
        } else if (extProperties.domain == ZES_POWER_DOMAIN_GPU || extProperties.domain == ZES_POWER_DOMAIN_MEMORY) {
            EXPECT_FALSE(properties.canControl);
            EXPECT_FALSE(properties.isEnergyThresholdSupported);
            EXPECT_EQ(properties.maxLimit, -1);
            EXPECT_EQ(properties.minLimit, -1);
            EXPECT_EQ(properties.defaultLimit, -1);
            EXPECT_EQ(defaultLimit.limit, -1);
        }
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenGettingPowerPropertiesAndExtPropertiesWithUnsetDefaultLimitThenCallSucceeds, IsBMG) {
    // Setting allow set calls or not
    init(true);

    auto handles = getPowerHandles(powerHandleDomainCount);

    for (auto handle : handles) {

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        properties.pNext = &extProperties;

        ze_result_t result = zesPowerGetProperties(handle, &properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_FALSE(properties.onSubdevice);

        if (extProperties.domain == ZES_POWER_DOMAIN_CARD || extProperties.domain == ZES_POWER_DOMAIN_PACKAGE) {
            EXPECT_TRUE(properties.canControl);
            EXPECT_TRUE(properties.isEnergyThresholdSupported);
            EXPECT_EQ(static_cast<uint32_t>(properties.maxLimit), pKmdSysManager->mockMaxPowerLimit);
            EXPECT_EQ(static_cast<uint32_t>(properties.minLimit), pKmdSysManager->mockMinPowerLimit);
            EXPECT_EQ(static_cast<uint32_t>(properties.defaultLimit), pKmdSysManager->mockTpdDefault);
        } else if (extProperties.domain == ZES_POWER_DOMAIN_GPU || extProperties.domain == ZES_POWER_DOMAIN_MEMORY) {
            EXPECT_FALSE(properties.canControl);
            EXPECT_FALSE(properties.isEnergyThresholdSupported);
            EXPECT_EQ(properties.maxLimit, -1);
            EXPECT_EQ(properties.minLimit, -1);
            EXPECT_EQ(properties.defaultLimit, -1);
        }
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidEnergyCounterOnlyPowerDomainWhenGettingPowerGetPropertiesThenUnsupportedIsReturned, IsAtMostDg2) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_MEMORY);
    zes_power_properties_t powerProperties = {};
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getProperties(&powerProperties));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidEnergyCounterOnlyPowerDomainWhenGettingPowerGetPropertiesExtThenUnsupportedIsReturned, IsAtMostDg2) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_MEMORY);
    zes_power_ext_properties_t powerProperties = {};
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getPropertiesExt(&powerProperties));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenInValidPowerDomainWhenGettingPowerGetPropertiesThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_UNKNOWN);
    zes_power_properties_t powerProperties = {};
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getProperties(&powerProperties));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenInValidPowerDomainWhenGettingPowerGetPropertiesExtThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_UNKNOWN);
    zes_power_ext_properties_t powerProperties = {};
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getPropertiesExt(&powerProperties));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenEnergyCounterOnlyPowerDomainWhenGettingPowerLimtsThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_GPU);
    zes_power_sustained_limit_t sustained;
    zes_power_burst_limit_t burst;
    zes_power_peak_limit_t peak;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getLimits(&sustained, &burst, &peak));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenEnergyCounterOnlyPowerDomainWhenCallingSetPowerLimtsThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_GPU);
    zes_power_sustained_limit_t sustained;
    zes_power_burst_limit_t burst;
    zes_power_peak_limit_t peak;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->setLimits(&sustained, &burst, &peak));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenEnergyCounterOnlyPowerDomainWhenCallingGetPowerExtLimtsThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_GPU);
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getLimitsExt(&count, nullptr));
}
HWTEST2_F(SysmanProductHelperPowerTest, GivenEnergyCounterOnlyPowerDomainWhenCallingSetPowerExtLimtsThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_GPU);
    uint32_t count = 0;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->setLimitsExt(&count, nullptr));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenEnergyCounterOnlyPowerDomainWhenCallingGetEnergyThresholdThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_GPU);
    zes_energy_threshold_t energyThreshold;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->getEnergyThreshold(&energyThreshold));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenEnergyCounterOnlyPowerDomainWhenCallingSetEnergyThresholdThenUnsupportedIsReturned, IsBMG) {
    // Setting allow set calls or not
    init(true);
    std::unique_ptr<WddmPowerImp> pWddmPowerImp = std::make_unique<WddmPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_GPU);
    double energyThreshold = 0;
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pWddmPowerImp->setEnergyThreshold(energyThreshold));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
