/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperPowerTest = SysmanDevicePowerFixtureI915;

constexpr uint32_t powerHandleComponentCount = 1u;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem1, realPathTelem1}};
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockReadLinkFailure(const char *path, char *buf, size_t bufsize) {
    errno = ENOENT;
    return -1;
}

static int mockOpenSuccess(const char *pathname, int flags) {
    int returnValue = -1;
    std::string strPathName(pathname);
    if ((strPathName == telem1OffsetFileName) || (strPathName == telem2OffsetFileName) || (strPathName == telem3OffsetFileName)) {
        returnValue = 4;
    } else if ((strPathName == telem1GuidFileName) || (strPathName == telem2GuidFileName) || (strPathName == telem3GuidFileName)) {
        returnValue = 5;
    } else if ((strPathName == telem1TelemFileName) || (strPathName == telem2TelemFileName) || (strPathName == telem3TelemFileName)) {
        returnValue = 6;
    } else if (strPathName.find(packageEnergyCounterNode) != std::string::npos) {
        returnValue = 7;
    }
    return returnValue;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint64_t val = 0;
    if (fd == 4) {
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 5) {
        oStream << "0x490e01";
    } else if (fd == 6) {
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 7) {
        return -1;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

inline static int mockStatSuccess(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IWUSR | S_IRUSR | S_IFREG;
    return 0;
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitValueThenCorrectValueIsReturned, IsPVC) {
    uint64_t testValue = 3000;
    int32_t expectedValue = 3000;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->getPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitValueThenCorrectValueIsReturned, IsNotPVC) {
    uint64_t testValue = 3000;
    int32_t expectedValue = 3;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->getPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingSetPowerLimitValueThenCorrectValueIsReturned, IsPVC) {
    int32_t testValue = 3000;
    uint64_t expectedValue = 3000;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->setPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingSetPowerLimitValueThenCorrectValueIsReturned, IsNotPVC) {
    int32_t testValue = 3;
    uint64_t expectedValue = 3000;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedValue, pSysmanProductHelper->setPowerLimitValue(testValue));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitUnitThenCorrectPowerLimitUnitIsReturned, IsPVC) {
    zes_limit_unit_t expectedPowerUnit = ZES_LIMIT_UNIT_CURRENT;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedPowerUnit, pSysmanProductHelper->getPowerLimitUnit());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingPackageCriticalPowerLimitFileThenFilenameIsReturned, IsPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("curr1_crit", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageCriticalPowerLimit, 0, false).c_str());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPackageCriticalPowerLimitNativeUnitThenCorrectValueIsReturned, IsPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageCriticalPowerLimit));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingPackageCriticalPowerLimitFileThenFilenameIsReturned, IsNotPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("power1_crit", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageCriticalPowerLimit, 0, false).c_str());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPackageCriticalPowerLimitNativeUnitThenCorrectValueIsReturned, IsNotPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageCriticalPowerLimit));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitUnitThenCorrectPowerLimitUnitIsReturned, IsNotPVC) {
    zes_limit_unit_t expectedPowerUnit = ZES_LIMIT_UNIT_POWER;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedPowerUnit, pSysmanProductHelper->getPowerLimitUnit());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingIsPowerSetLimitSupportedThenVerifySetRequestIsSupported, IsXeCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isPowerSetLimitSupported());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleForPackageDomainWhenGettingPowerEnergyCounterThenFailureIsReturned, IsNotDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_power_energy_counter_t energyCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanProductHelper->getPowerEnergyCounter(&energyCounter, pLinuxSysmanImp, ZES_POWER_DOMAIN_PACKAGE, 0));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleForPackageDomainWithTelemetrySupportNotAvailableAndSysfsNodeReadFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleForPackageDomainWithTelemetryDataNotAvailableAndSysfsNodeReadAlsoFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telem1Offset = 0;
        constexpr std::string_view validGuid = "0x4f9302";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            count = -1;
        }
        return count;
    });
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleForPackageDomainWithTelemetrySupportAvailableAndTelemetryOffsetReadFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 4) {
            count = -1;
        }
        return count;
    });
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    pLinuxPowerImp->isTelemetrySupportAvailable = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleForPackageDomainWithTelemetryKeyOffsetMapNotAvailableAndSysfsNodeReadAlsoFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telem1Offset = 0;
        constexpr std::string_view invalidGuid = "0xABCDEFG";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, invalidGuid.data(), count);
        }
        return count;
    });
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    pLinuxPowerImp->isTelemetrySupportAvailable = true;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportAvailableWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromPmtNode, IsDG2) {
    static uint64_t setEnergyCounter = 123456u;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telem1Offset = 0;
        constexpr std::string_view validGuid = "0x4f9302";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &setEnergyCounter, count);
        }
        return count;
    });

    auto handles = getPowerHandles(powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        uint64_t timeStampInitial = SysmanDevice::getSysmanTimestamp();
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));

        // Calculate output energyCounter value
        constexpr uint64_t fixedPointToJoule = 1048576;
        uint64_t outputEnergyCounter = static_cast<uint64_t>((setEnergyCounter / fixedPointToJoule) * convertJouleToMicroJoule);

        EXPECT_EQ(energyCounter.energy, outputEnergyCounter);
        EXPECT_GE(energyCounter.timestamp, timeStampInitial);
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidSubdevicePowerHandleForPackageDomainWithTelemetrySupportNotAvailableAndSysfsNodeReadFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    pSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, true, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned, IsDG1) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    uint32_t count = 0;
    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_sustained_limit_t sustained;
        zes_power_burst_limit_t burst;
        zes_power_peak_limit_t peak;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustained, &burst, &peak));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenComponentCountZeroWhenEnumeratingPowerDomainsThenValidPowerHandlesIsReturned, IsDG1) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSetPowerLimitsWhenGettingPowerLimitsWhenHwmonInterfaceExistThenLimitsSetEarlierAreRetrieved, IsXeCore) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};
        sustainedSet.enabled = 1;
        sustainedSet.interval = 10;
        sustainedSet.power = 300000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
        EXPECT_EQ(sustainedGet.power, sustainedSet.power);

        zes_power_burst_limit_t burstGet = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
        EXPECT_EQ(burstGet.enabled, false);
        EXPECT_EQ(burstGet.power, -1);

        zes_power_peak_limit_t peakSet = {};
        zes_power_peak_limit_t peakGet = {};
        peakSet.powerAC = 300000;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
        EXPECT_EQ(peakGet.powerAC, peakSet.powerAC);
        EXPECT_EQ(peakGet.powerDC, -1);
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        uint32_t limitCount = 0;
        const int32_t testLimit = 300000;
        const int32_t testInterval = 10;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        limitCount++;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_FALSE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
                allLimits[i].interval = testInterval;
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_TRUE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_CURRENT, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
            }
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_EQ(testInterval, allLimits[i].interval);
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                EXPECT_EQ(0, allLimits[i].interval);
            }
            EXPECT_EQ(testLimit, allLimits[i].limit);
        }
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsDG1) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        uint32_t limitCount = 0;
        const int32_t testLimit = 300000;
        const int32_t testInterval = 10;

        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        limitCount++;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
        EXPECT_EQ(limitCount, mockLimitCount);

        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_FALSE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
                allLimits[i].interval = testInterval;
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_TRUE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
            }
        }
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_EQ(testInterval, allLimits[i].interval);
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                EXPECT_EQ(0, allLimits[i].interval);
            }
            EXPECT_EQ(testLimit, allLimits[i].limit);
        }
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleAndWritingToPeakLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWritePeakLimitResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleAndWritingToPeakLimitSysNodesFailsWhenCallingSetPowerLimitsExtThenProperErrorCodesReturned, IsDG1) {
    auto handles = getPowerHandles(powerHandleComponentCount);

    pSysfsAccess->mockWritePeakLimitResult = ZE_RESULT_ERROR_NOT_AVAILABLE;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = mockLimitCount;
        std::vector<zes_power_limit_ext_desc_t> allLimits(mockLimitCount);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &count, allLimits.data()));

        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &count, allLimits.data()));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleAndPermissionsThenFirstDisableSustainedPowerLimitAndThenEnableItAndCheckSuccesIsReturned, IsXeCore) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);
    zes_power_sustained_limit_t sustainedSet = {};
    zes_power_sustained_limit_t sustainedGet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
    sustainedSet.power = 300000;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handles[0], &sustainedGet, nullptr, nullptr));
    EXPECT_EQ(sustainedGet.power, sustainedSet.power);
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenWritingToSustainedPowerEnableNodeWithoutPermissionsThenErrorIsReturned, IsXeCore) {
    auto handles = getPowerHandles(powerHandleComponentCount);
    ASSERT_NE(nullptr, handles[0]);

    pSysfsAccess->mockWriteUnsignedResult.push_back(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS);
    zes_power_sustained_limit_t sustainedSet = {};
    sustainedSet.enabled = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handles[0], &sustainedSet, nullptr, nullptr));
}

using SysmanProductHelperPowerMultiDeviceTest = SysmanDevicePowerMultiDeviceFixture;
constexpr uint32_t powerHandleComponentCountMultiDevice = 3u;

HWTEST2_F(SysmanProductHelperPowerMultiDeviceTest, GivenValidPowerHandlesWithTelemetrySupportAvailableButNoTelemDataWhenGettingPowerEnergyCounterThenEnergyCounterIsRetrievedThroughSysfsNode, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telem1Offset = 0;
        constexpr std::string_view validGuid = "0xb15a0edd";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        }
        return count;
    });

    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));
        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        const uint64_t timeStampInitial = SysmanDevice::getSysmanTimestamp();
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));

        if (!properties.onSubdevice) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
        } else if (properties.subdeviceId == 0u) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterTile0);
        } else if (properties.subdeviceId == 1u) {
            EXPECT_EQ(energyCounter.energy, expectedEnergyCounterTile1);
        }

        EXPECT_GE(energyCounter.timestamp, timeStampInitial);
    }
}

HWTEST2_F(SysmanProductHelperPowerMultiDeviceTest, GivenSetPowerLimitsWhenGettingPowerLimitsThenLimitsSetEarlierAreRetrieved, IsXeCore) {
    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_power_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        zes_power_sustained_limit_t sustainedSet = {};
        zes_power_sustained_limit_t sustainedGet = {};
        sustainedSet.power = 300000;
        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
            EXPECT_EQ(sustainedGet.power, sustainedSet.power);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimits(handle, &sustainedSet, nullptr, nullptr));
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustainedGet, nullptr, nullptr));
        }

        zes_power_burst_limit_t burstGet = {};
        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
            EXPECT_EQ(burstGet.enabled, false);
            EXPECT_EQ(burstGet.power, -1);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, &burstGet, nullptr));
        }

        zes_power_peak_limit_t peakSet = {};
        zes_power_peak_limit_t peakGet = {};
        peakSet.powerAC = 300000;
        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimits(handle, nullptr, nullptr, &peakSet));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
            EXPECT_EQ(peakGet.powerAC, peakSet.powerAC);
            EXPECT_EQ(peakGet.powerDC, -1);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, nullptr, nullptr, &peakGet));
        }
    }
}

HWTEST2_F(SysmanProductHelperPowerMultiDeviceTest, GivenValidPowerHandlesWhenCallingSetAndGetPowerLimitExtThenLimitsSetEarlierAreRetrieved, IsPVC) {
    auto handles = getPowerHandles(powerHandleComponentCountMultiDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        uint32_t limitCount = 0;
        const int32_t testLimit = 300000;
        const int32_t testInterval = 10;

        zes_power_properties_t properties = {};
        zes_power_limit_ext_desc_t limits = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, mockLimitCount);
        } else {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, &limits));
            EXPECT_EQ(limitCount, 0u);
        }

        limitCount++;
        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, mockLimitCount);
        } else {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, nullptr));
            EXPECT_EQ(limitCount, 0u);
        }

        std::vector<zes_power_limit_ext_desc_t> allLimits(limitCount);
        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
        } else {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
            EXPECT_EQ(limitCount, 0u);
        }
        for (uint32_t i = 0; i < limitCount; i++) {
            if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_FALSE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_POWER, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
                allLimits[i].interval = testInterval;
            } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                EXPECT_FALSE(allLimits[i].limitValueLocked);
                EXPECT_TRUE(allLimits[i].enabledStateLocked);
                EXPECT_TRUE(allLimits[i].intervalValueLocked);
                EXPECT_EQ(ZES_POWER_SOURCE_ANY, allLimits[i].source);
                EXPECT_EQ(ZES_LIMIT_UNIT_CURRENT, allLimits[i].limitUnit);
                allLimits[i].limit = testLimit;
            }
        }
        if (!properties.onSubdevice) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetLimitsExt(handle, &limitCount, allLimits.data()));
            for (uint32_t i = 0; i < limitCount; i++) {
                if (allLimits[i].level == ZES_POWER_LEVEL_SUSTAINED) {
                    EXPECT_EQ(testInterval, allLimits[i].interval);
                } else if (allLimits[i].level == ZES_POWER_LEVEL_PEAK) {
                    EXPECT_EQ(0, allLimits[i].interval);
                }
                EXPECT_EQ(testLimit, allLimits[i].limit);
            }
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerSetLimitsExt(handle, &limitCount, allLimits.data()));
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
