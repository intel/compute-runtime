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

using SysmanProductHelperPowerTest = SysmanDeviceFixture;

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

static int mockMultiDeviceReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem1, realPathTelem1},
        {sysfsPathTelem2, realPathTelem2},
        {sysfsPathTelem3, realPathTelem3}};
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
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
    } else if (strPathName.find(energyCounterNode) != std::string::npos) {
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

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingPackageCriticalPowerLimitFileThenEmptyFilenameIsReturned, IsNotPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNamePackageCriticalPowerLimit, 0, false).c_str());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingCardCriticalPowerLimitFileThenEmptyFilenameIsReturned, IsPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardCriticalPowerLimit, 0, false).c_str());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingGetPackageCriticalPowerLimitNativeUnitThenCorrectValueIsReturned, IsPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageCriticalPowerLimit));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenFetchingCardCriticalPowerLimitFileThenEmptyFilenameIsReturned, IsNotPVC) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCardCriticalPowerLimit, 0, false).c_str());
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

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidProductHelperHandleWhenCallingIsPowerSetLimitSupportedThenVerifySetRequestIsSupported, IsXeHpOrXeHpcOrXeHpgCore) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(pSysmanProductHelper->isPowerSetLimitSupported());
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidRootDevicePowerHandleForPackageDomainWithTelemetrySupportNotAvailableAndSysfsNodeReadFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidRootDevicePowerHandleForPackageDomainWithTelemetryDataNotAvailableAndSysfsNodeReadAlsoFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0xb15a0ede";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        }
        return count;
    });
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidSubdevicePowerHandleForPackagePackageDomainWithTelemetrySupportNotAvailableAndSysfsNodeReadFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, true, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidSubdevicePowerHandleForPackageDomainWithTelemetrySupportAvailableAndSysfsNodeReadFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0xb15a0ede";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        }
        return count;
    });
    zes_power_energy_counter_t energyCounter = {};
    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, true, 0, ZES_POWER_DOMAIN_PACKAGE));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0xb15a0ede";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            count = -1;
        }
        return count;
    });

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

using SysmanProductHelperPowerMultiDeviceTest = SysmanDevicePowerMultiDeviceFixture;
constexpr uint32_t i915PowerHandleComponentCount = 3u;
HWTEST2_F(SysmanProductHelperPowerMultiDeviceTest, GivenValidPowerHandlesWithTelemetryDataNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockMultiDeviceReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0xb15a0ede";
        uint32_t mockKeyValue = 0x3;

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &mockKeyValue, count);
        }
        return count;
    });

    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, i915PowerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleForPackageDomainAndTelemetryDataNotAvailableAndSysfsReadAlsoFailsWhenGettingPowerEnergyCounterThenFailureIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x490e01";
        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            count = -1;
        }
        return count;
    });

    std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE));
    zes_power_energy_counter_t energyCounter = {};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pLinuxPowerImp->getEnergyCounter(&energyCounter));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleAndHwMonDoesNotExistWhenGettingPowerLimitsThenUnsupportedFeatureErrorIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

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

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportAvailableWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromPmtNode, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x490e01";
        uint32_t mockKeyValue = 0x3;

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &mockKeyValue, count);
        }
        return count;
    });

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x490e01";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            count = -1;
        }
        return count;
    });

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportAvailableWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromPmtNode, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x4f9302";
        uint32_t mockKeyValue = 0x3;

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, &mockKeyValue, count);
        }
        return count;
    });

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pMockSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_SUCCESS);
    pMockSysfsAccess->mockReadValUnsignedLongResult.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telem1Offset = 0;
        std::string validGuid = "0x4f9302";

        if (fd == 4) {
            memcpy(buf, &telem1Offset, count);
        } else if (fd == 5) {
            memcpy(buf, validGuid.data(), count);
        } else if (fd == 6) {
            count = -1;
        }
        return count;
    });

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);

        zes_power_properties_t properties = {};
        zes_power_ext_properties_t extProperties = {};

        properties.pNext = &extProperties;
        extProperties.stype = ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetProperties(handle, &properties));

        EXPECT_EQ(ZES_POWER_DOMAIN_PACKAGE, extProperties.domain);

        zes_power_energy_counter_t energyCounter = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesPowerGetEnergyCounter(handle, &energyCounter));
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenValidPowerHandleWhenSettingPowerLimitsThenUnsupportedFeatureErrorIsReturned, IsDG1) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

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

    MockPowerSysfsAccessInterface *pMockSysfsAccess = new MockPowerSysfsAccessInterface();
    std::unique_ptr<MockPowerFsAccessInterface> pMockFsAccess = std::make_unique<MockPowerFsAccessInterface>();
    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());

    pMockSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
    pSysmanKmdInterface->pSysfsAccess.reset(pMockSysfsAccess);
    pLinuxSysmanImp->pFsAccess = pMockFsAccess.get();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, powerHandleComponentCount);

    std::vector<zes_pwr_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumPowerDomains(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenCheckingAvailabiityOfPmtNodeForPowerDomainThenValidResultIsReturnedForDifferentPowerDomain, IsDG1) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_CARD));
    EXPECT_TRUE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_PACKAGE));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenCheckingAvailabiityOfPmtNodeForPowerDomainThenValidResultIsReturnedForDifferentPowerDomain, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_CARD));
    EXPECT_TRUE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_PACKAGE));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenCheckingAvailabiityOfPmtNodeForPowerDomainThenValidResultIsReturnedForDifferentPowerDomain, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_CARD));
    EXPECT_FALSE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_PACKAGE));
}

HWTEST2_F(SysmanProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenCheckingAvailabiityOfPmtNodeForPowerDomainThenValidResultIsReturnedForDifferentPowerDomain, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_CARD));
    EXPECT_FALSE(pSysmanProductHelper->isPmtNodeAvailableForEnergyCounter(ZES_POWER_DOMAIN_PACKAGE));
}

} // namespace ult
} // namespace Sysman
} // namespace L0
