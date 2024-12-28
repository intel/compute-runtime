/*
 * Copyright (C) 2023-2024 Intel Corporation
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

using SysmanProductHelperPowerTest = SysmanDeviceFixture;

constexpr uint32_t powerHandleComponentCount = 1u;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem, realPathTelem},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
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
        if (offset == mockKeyOffset) {
            val = setEnergyCounter;
        }
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

static int mockOpenSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telemOffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telemGuidFileName) {
        returnValue = 5;
    } else if (strPathName == telemFileName) {
        returnValue = 6;
    } else if (strPathName.find(energyCounterNode) != std::string::npos) {
        returnValue = 7;
    }
    return returnValue;
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

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingCardCriticalPowerLimitFileThenFilenameIsReturned, IsPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("curr1_crit", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCriticalPowerLimit, 0, false).c_str());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetCardCriticalPowerLimitNativeUnitThenCorrectValueIsReturned, IsPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingCardCriticalPowerLimitFileThenFilenameIsReturned, IsNotPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("power1_crit", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCriticalPowerLimit, 0, false).c_str());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetCardCriticalPowerLimitNativeUnitThenCorrectValueIsReturned, IsNotPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPowerLimitUnitThenCorrectPowerLimitUnitIsReturned, IsNotPVC) {
    zes_limit_unit_t expectedPowerUnit = ZES_LIMIT_UNIT_POWER;
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(expectedPowerUnit, pSysmanProductHelper->getPowerLimitUnit());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingIsPowerSetLimitSupportedThenVerifySetRequestIsSupported, IsXeHpOrXeHpcOrXeHpgCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isPowerSetLimitSupported());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysfsReadFailsAndKeyOffsetMapNotAvailableForGuidWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint64_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0xABCDE";
        } else if (fd == 7) {
            return -1;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD));
    pLinuxPowerImp->isTelemetrySupportAvailable = true;
    zes_power_energy_counter_t energyCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysfsReadFailsAndPmtReadValueFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint64_t val = 0;
        if (fd == 4) {
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 5) {
            oStream << "0x490e01";
        } else if (fd == 6) {
            if (offset == mockKeyOffset) {
                errno = ENOENT;
                return -1;
            }
        } else if (fd == 7) {
            return -1;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD));
    pLinuxPowerImp->isTelemetrySupportAvailable = true;
    zes_power_energy_counter_t energyCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysfsReadFailsWhenGettingPowerEnergyCounterThenSuccesIsReturned, IsDG1) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD));
    pLinuxPowerImp->isTelemetrySupportAvailable = true;
    zes_power_energy_counter_t energyCounter = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
    uint64_t expectedEnergyCounter = convertJouleToMicroJoule * (setEnergyCounter / 1048576);
    EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleAndHwMonDoesNotExistWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned, IsDG1) {

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
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesPowerGetLimits(handle, &sustained, &burst, &peak));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenScanDirectoriesFailAndTelemetrySupportAvailableThenPowerModuleIsSupported, IsDG1) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    pSysmanDeviceImp->pPowerHandleContext->init(subDeviceCount);
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, ZES_POWER_DOMAIN_CARD));
    EXPECT_TRUE(pLinuxPowerImp->isPowerModuleSupported());
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

} // namespace ult
} // namespace Sysman
} // namespace L0
