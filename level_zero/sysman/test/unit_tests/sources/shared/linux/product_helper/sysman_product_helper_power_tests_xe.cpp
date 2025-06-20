/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/power/linux/mock_sysfs_power_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string_view telem2OffsetFileName("/sys/class/intel_pmt/telem2/offset");
const std::string_view telem2GuidFileName("/sys/class/intel_pmt/telem2/guid");
const std::string_view telem2TelemFileName("/sys/class/intel_pmt/telem2/telem");
const std::string_view telem3OffsetFileName("/sys/class/intel_pmt/telem3/offset");
const std::string_view telem3GuidFileName("/sys/class/intel_pmt/telem3/guid");
const std::string_view telem3TelemFileName("/sys/class/intel_pmt/telem3/telem");

using SysmanXeProductHelperPowerTest = SysmanDevicePowerFixtureXe;
constexpr uint32_t bmgPowerHandleComponentCount = 4u;
constexpr uint32_t bmgPowerLimitSupportedCount = 3u;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {

    const std::string sysfsPathTelem1 = "/sys/class/intel_pmt/telem1";
    const std::string sysfsPathTelem2 = "/sys/class/intel_pmt/telem2";
    const std::string sysfsPathTelem3 = "/sys/class/intel_pmt/telem3";
    const std::string realPathTelem1 = "/sys/devices/pci0000:00/0000:00:0a.0/intel_vsec.telemetry.0/intel_pmt/telem1";
    const std::string realPathTelem2 = "/sys/devices/pci0000:00/0000:00:1c.4/0000:06:00.0/0000:07:01.0/0000:08:00.0/intel_vsec.telemetry.1/intel_pmt/telem2";
    const std::string realPathTelem3 = "/sys/devices/pci0000:00/0000:00:1c.4/0000:06:00.0/0000:07:01.0/0000:08:00.0/intel_vsec.telemetry.1/intel_pmt/telem3";

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
    if ((strPathName == telem2OffsetFileName) || (strPathName == telem3OffsetFileName)) {
        returnValue = 4;
    } else if (strPathName == telem2GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem3GuidFileName) {
        returnValue = 6;
    } else if (strPathName == telem2TelemFileName) {
        returnValue = 7;
    } else if (strPathName == telem3TelemFileName) {
        returnValue = 8;
    }
    return returnValue;
}

inline static int mockStatSuccess(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IWUSR | S_IRUSR | S_IFREG;
    return 0;
}

HWTEST2_F(SysmanDevicePowerFixtureXe, GivenVariousPowerLimitFileExistanceStatesWhenIsPowerModuleSupportedIsCalledForRootDeviceHandleThenCorrectSupportStatusIsReturnedForCardDomain, IsBMG) {
    // Loop through all combinations of the three boolean flags (false/true) for the file existence
    for (bool isEnergyCounterFilePresent : {false, true}) {
        for (bool isTelemetrySupportAvailable : {false, true}) {
            for (bool isSustainedPowerLimitFilePresent : {false, true}) {
                for (bool isCriticalPowerLimitPresent : {false, true}) {
                    for (bool isBurstPowerLimitPresent : {false, true}) {
                        // Set the file existence flags based on the current combination
                        pSysfsAccess->isCardEnergyCounterFilePresent = isEnergyCounterFilePresent;
                        pSysfsAccess->isCardSustainedPowerLimitFilePresent = isSustainedPowerLimitFilePresent;
                        pSysfsAccess->isCardCriticalPowerLimitFilePresent = isCriticalPowerLimitPresent;
                        pSysfsAccess->isCardBurstPowerLimitFilePresent = isBurstPowerLimitPresent;
                        pSysfsAccess->isPackageEnergyCounterFilePresent = isEnergyCounterFilePresent;
                        pSysfsAccess->isPackageSustainedPowerLimitFilePresent = isSustainedPowerLimitFilePresent;
                        pSysfsAccess->isPackageCriticalPowerLimitFilePresent = isCriticalPowerLimitPresent;
                        pSysfsAccess->isPackageBurstPowerLimitFilePresent = isBurstPowerLimitPresent;

                        // The expected result is true if at least one of the files is present
                        bool expected = (isTelemetrySupportAvailable || isEnergyCounterFilePresent || isSustainedPowerLimitFilePresent ||
                                         isCriticalPowerLimitPresent || isBurstPowerLimitPresent);

                        // Verify if the power module is supported as expected
                        auto pPowerImpForCard = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_CARD);
                        pPowerImpForCard->isTelemetrySupportAvailable = isTelemetrySupportAvailable;
                        EXPECT_EQ(pPowerImpForCard->isPowerModuleSupported(), expected);

                        auto pPowerImpForPackage = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, ZES_POWER_DOMAIN_PACKAGE);
                        pPowerImpForPackage->isTelemetrySupportAvailable = isTelemetrySupportAvailable;
                        EXPECT_EQ(pPowerImpForPackage->isPowerModuleSupported(), expected);
                    }
                }
            }
        }
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenPmtSupportIsAvailableThenValidCountIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telemOffset = 0;
        constexpr std::string_view validOobmsmGuid = "0x5e2f8211";
        constexpr std::string_view validPunitGuid = "0x1e2f8200";

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, validPunitGuid.data(), count);
        }
        return count;
    });

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, bmgPowerHandleComponentCount);
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenGettingPowerEnergyCounterForUnknownPowerDomainThenFailureIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_power_energy_counter_t energyCounter = {};
    auto result = pSysmanProductHelper->getPowerEnergyCounter(&energyCounter, pLinuxSysmanImp, ZES_POWER_DOMAIN_UNKNOWN, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenGettingPowerEnergyCounterAndNoTelemNodesAreAvailableThenFailureIsReturned, IsBMG) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_power_energy_counter_t energyCounter = {};
    auto result = pSysmanProductHelper->getPowerEnergyCounter(&energyCounter, pLinuxSysmanImp, ZES_POWER_DOMAIN_CARD, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenGettingPowerEnergyCounterAndReadGuidFailsFromPmtUtilThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        if (fd == 5 || fd == 6) {
            count = -1;
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_power_energy_counter_t energyCounter = {};
    auto result = pSysmanProductHelper->getPowerEnergyCounter(&energyCounter, pLinuxSysmanImp, ZES_POWER_DOMAIN_CARD, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenGettingPowerEnergyCounterAndKeyOffsetMapIsNotAvailableThenFailureIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telemOffset = 0;
        constexpr std::string_view dummyGuid = "0xABCDEF";

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5 || fd == 6) {
            memcpy(buf, dummyGuid.data(), count);
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_power_energy_counter_t energyCounter = {};
    auto result = pSysmanProductHelper->getPowerEnergyCounter(&energyCounter, pLinuxSysmanImp, ZES_POWER_DOMAIN_CARD, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenSysmanProductHelperInstanceWhenGettingPowerEnergyCounterAndReadValueFailsForDifferentKeysThenFailureIsReturned, IsBMG) {
    static int readFailCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr uint64_t telemOffset = 0;
        constexpr std::string_view validOobmsmGuid = "0x5e2f8211";
        constexpr std::string_view validPunitGuid = "0x1e2f8200";

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 7) {
            switch (offset) {
            case 140:
                count = (readFailCount == 1) ? -1 : sizeof(uint32_t);
                break;
            default:
                break;
            }
        } else if (fd == 8) {
            switch (offset) {
            case 4:
                count = (readFailCount == 2) ? -1 : sizeof(uint32_t);
                break;
            case 1024:
                count = (readFailCount == 3) ? -1 : sizeof(uint64_t);
                break;
            default:
                break;
            }
        }
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_power_energy_counter_t energyCounter = {};
    while (readFailCount <= 3) {
        auto result = pSysmanProductHelper->getPowerEnergyCounter(&energyCounter, pLinuxSysmanImp, ZES_POWER_DOMAIN_CARD, 0u);
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, result);
        readFailCount++;
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandlesWhenGettingPowerEnergyCounterThenValidValuesAreReturnedFromBothOobmsmAndPunitPath, IsBMG) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        constexpr std::string_view validOobmsmGuid = "0x5e2f8211";
        constexpr std::string_view validPunitGuid = "0x1e2f8200";
        constexpr uint32_t mockEnergyCounter = 0xabcd;
        constexpr uint32_t mockXtalFrequency = 0xef;
        constexpr uint64_t mockTimestamp = 0xabef;

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 7) {
            switch (offset) {
            case 136:
            case 140:
                memcpy(buf, &mockEnergyCounter, count);
                break;
            default:
                break;
            }
        } else if (fd == 8) {
            switch (offset) {
            case 4:
                memcpy(buf, &mockXtalFrequency, count);
                break;
            case 1024:
                memcpy(buf, &mockTimestamp, count);
                break;
            case 1628:
            case 1640:
                memcpy(buf, &mockEnergyCounter, count);
                break;
            default:
                break;
            }
        }
        return count;
    });

    constexpr uint32_t mockEnergyCounter = 0xabcd;
    constexpr uint32_t mockXtalFrequency = 0xef;
    constexpr uint64_t mockTimestamp = 0xabef;
    constexpr double indexToXtalClockFrequecyMap[4] = {24, 19.2, 38.4, 25};

    auto handles = getPowerHandles();
    EXPECT_EQ(bmgPowerHandleComponentCount, handles.size());

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        auto result = zesPowerGetEnergyCounter(handle, &energyCounter);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        const uint32_t integerPart = static_cast<uint32_t>(mockEnergyCounter >> 14);
        const uint32_t decimalBits = static_cast<uint32_t>(mockEnergyCounter & 0x3FFF);
        const double decimalPart = static_cast<double>(decimalBits) / (1 << 14);
        const double finalValue = static_cast<double>(integerPart + decimalPart);
        const uint64_t expectedEnergyCounter = static_cast<uint64_t>((finalValue * convertJouleToMicroJoule));
        EXPECT_EQ(expectedEnergyCounter, energyCounter.energy);

        const double timestamp = mockTimestamp / indexToXtalClockFrequecyMap[mockXtalFrequency & 0x2];
        const uint64_t expectedTimestamp = static_cast<uint64_t>(timestamp);
        EXPECT_EQ(expectedTimestamp, energyCounter.timestamp);
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandlesWhenGettingPowerEnergyCounterThenAllValidValuesAreReturnedFromPunitPath, IsBMG) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        constexpr std::string_view validOobmsmGuid = "0x5e2f8211";
        constexpr std::string_view validPunitGuid = "0x1e2f8201";
        constexpr uint32_t mockEnergyCounter = 0xabcd;
        constexpr uint32_t mockXtalFrequency = 0xef;
        constexpr uint64_t mockTimestamp = 0xabef;

        if (fd == 4) {
            memcpy(buf, &telemOffset, count);
        } else if (fd == 5) {
            memcpy(buf, validOobmsmGuid.data(), count);
        } else if (fd == 6) {
            memcpy(buf, validPunitGuid.data(), count);
        } else if (fd == 7) {
            switch (offset) {
            case 136:
            case 140:
                memcpy(buf, &mockEnergyCounter, count);
                break;
            default:
                break;
            }
        } else if (fd == 8) {
            switch (offset) {
            case 4:
                memcpy(buf, &mockXtalFrequency, count);
                break;
            case 1024:
                memcpy(buf, &mockTimestamp, count);
                break;
            case 48:
            case 52:
            case 1628:
            case 1640:
                memcpy(buf, &mockEnergyCounter, count);
                break;
            default:
                break;
            }
        }
        return count;
    });

    constexpr uint32_t mockEnergyCounter = 0xabcd;
    constexpr uint32_t mockXtalFrequency = 0xef;
    constexpr uint64_t mockTimestamp = 0xabef;
    constexpr double indexToXtalClockFrequecyMap[4] = {24, 19.2, 38.4, 25};

    auto handles = getPowerHandles();
    EXPECT_EQ(bmgPowerHandleComponentCount, handles.size());

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        auto result = zesPowerGetEnergyCounter(handle, &energyCounter);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        const uint32_t integerPart = static_cast<uint32_t>(mockEnergyCounter >> 14);
        const uint32_t decimalBits = static_cast<uint32_t>(mockEnergyCounter & 0x3FFF);
        const double decimalPart = static_cast<double>(decimalBits) / (1 << 14);
        const double finalValue = static_cast<double>(integerPart + decimalPart);
        const uint64_t expectedEnergyCounter = static_cast<uint64_t>((finalValue * convertJouleToMicroJoule));
        EXPECT_EQ(expectedEnergyCounter, energyCounter.energy);

        const double timestamp = mockTimestamp / indexToXtalClockFrequecyMap[mockXtalFrequency & 0x2];
        const uint64_t expectedTimestamp = static_cast<uint64_t>(timestamp);
        EXPECT_EQ(expectedTimestamp, energyCounter.timestamp);
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode, IsBMG) {
    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_DOMAIN_CARD};
    for (const auto &powerDomain : powerDomains) {
        zes_power_energy_counter_t energyCounter = {};
        std::unique_ptr<XePublicLinuxPowerImp> pLinuxPowerImp(new XePublicLinuxPowerImp(pOsSysman, false, 0, powerDomain));
        pLinuxPowerImp->isTelemetrySupportAvailable = false;
        const uint64_t timeStampInitial = SysmanDevice::getSysmanTimestamp();
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
        EXPECT_EQ(energyCounter.energy, xeMockEnergyCounter);
        EXPECT_GE(energyCounter.timestamp, timeStampInitial);
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainsAndSysfsReadResultsForSustainedPowerLimitWhenGetLimitsExtIsCalledThenProperResultsAreReturned, IsBMG) {
    pSysfsAccess->isCardBurstPowerLimitFilePresent = false;
    pSysfsAccess->isPackageBurstPowerLimitFilePresent = false;
    pSysfsAccess->isCardCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isPackageCriticalPowerLimitFilePresent = false;

    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        for (ze_result_t sustainedLimitResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
            for (ze_result_t sustainedLimitIntervalResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
                pSysfsAccess->sustainedReadResult = sustainedLimitResult;
                pSysfsAccess->sustainedIntervalReadResult = sustainedLimitIntervalResult;

                uint32_t count = 0;
                EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
                EXPECT_EQ(1u, count);

                std::vector<zes_power_limit_ext_desc_t> allLimits(count);
                ze_result_t expectedResult = sustainedLimitResult;

                EXPECT_EQ(expectedResult, pPowerImp->getLimitsExt(&count, allLimits.data()));

                if (sustainedLimitResult == ZE_RESULT_SUCCESS) {
                    EXPECT_EQ(allLimits[0].limit, static_cast<int32_t>(pSysfsAccess->sustainedPowerLimitVal / milliFactor));
                    EXPECT_EQ(allLimits[0].enabledStateLocked, true);
                    EXPECT_EQ(allLimits[0].intervalValueLocked, false);
                    EXPECT_EQ(allLimits[0].limitValueLocked, false);
                    EXPECT_EQ(allLimits[0].source, ZES_POWER_SOURCE_ANY);
                    EXPECT_EQ(allLimits[0].level, ZES_POWER_LEVEL_SUSTAINED);
                    EXPECT_EQ(allLimits[0].limitUnit, ZES_LIMIT_UNIT_POWER);
                    EXPECT_EQ(allLimits[0].interval, (sustainedLimitIntervalResult == ZE_RESULT_SUCCESS) ? pSysfsAccess->sustainedPowerLimitIntervalVal : -1);
                }
            }
        }
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainsAndSysfsWriteResultsForSustainedPowerLimitIntervalWhenSetLimitsExtIsCalledThenProperResultsAreReturned, IsBMG) {
    pSysfsAccess->isCardBurstPowerLimitFilePresent = false;
    pSysfsAccess->isPackageBurstPowerLimitFilePresent = false;
    pSysfsAccess->isCardCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isPackageCriticalPowerLimitFilePresent = false;

    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        for (ze_result_t sustainedLimitIntervalResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
            pSysfsAccess->sustainedIntervalWriteResult = sustainedLimitIntervalResult;

            uint32_t count = 0;

            EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
            EXPECT_EQ(1u, count);

            std::vector<zes_power_limit_ext_desc_t> allLimits(count);
            const int32_t testSustainedLimitInterval = 100;

            EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, allLimits.data()));
            EXPECT_EQ(allLimits[0].level, ZES_POWER_LEVEL_SUSTAINED);
            allLimits[0].interval = testSustainedLimitInterval;

            ze_result_t expectedResult = sustainedLimitIntervalResult;
            EXPECT_EQ(expectedResult, pPowerImp->setLimitsExt(&count, allLimits.data()));

            if (sustainedLimitIntervalResult == ZE_RESULT_SUCCESS) {
                EXPECT_EQ(allLimits[0].interval, testSustainedLimitInterval);
            }
        }
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainsAndSysfsReadResultsForBurstPowerLimitWhenGetLimitsExtIsCalledThenProperResultsAreReturned, IsBMG) {
    pSysfsAccess->isCardSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isPackageSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCardCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isPackageCriticalPowerLimitFilePresent = false;

    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        for (ze_result_t burstLimitResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
            for (ze_result_t burstLimitIntervalResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
                pSysfsAccess->burstReadResult = burstLimitResult;
                pSysfsAccess->burstIntervalReadResult = burstLimitIntervalResult;

                uint32_t count = 0;
                EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
                EXPECT_EQ(1u, count);

                std::vector<zes_power_limit_ext_desc_t> allLimits(count);
                ze_result_t expectedResult = burstLimitResult;

                EXPECT_EQ(expectedResult, pPowerImp->getLimitsExt(&count, allLimits.data()));

                if (burstLimitResult == ZE_RESULT_SUCCESS) {
                    EXPECT_EQ(allLimits[0].limit, static_cast<int32_t>(pSysfsAccess->burstPowerLimitVal / milliFactor));
                    EXPECT_EQ(allLimits[0].enabledStateLocked, true);
                    EXPECT_EQ(allLimits[0].intervalValueLocked, false);
                    EXPECT_EQ(allLimits[0].limitValueLocked, false);
                    EXPECT_EQ(allLimits[0].source, ZES_POWER_SOURCE_ANY);
                    EXPECT_EQ(allLimits[0].level, ZES_POWER_LEVEL_BURST);
                    EXPECT_EQ(allLimits[0].limitUnit, ZES_LIMIT_UNIT_POWER);
                    EXPECT_EQ(allLimits[0].interval, (burstLimitIntervalResult == ZE_RESULT_SUCCESS) ? pSysfsAccess->burstPowerLimitIntervalVal : -1);
                }
            }
        }
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainsAndSysfsWriteResultsForBurstPowerLimitIntervalWhenSetLimitsExtIsCalledThenProperResultsAreReturned, IsBMG) {
    pSysfsAccess->isCardSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isPackageSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCardCriticalPowerLimitFilePresent = false;
    pSysfsAccess->isPackageCriticalPowerLimitFilePresent = false;

    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        for (ze_result_t burstLimitIntervalResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
            pSysfsAccess->burstIntervalWriteResult = burstLimitIntervalResult;

            uint32_t count = 0;

            EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
            EXPECT_EQ(1u, count);

            std::vector<zes_power_limit_ext_desc_t> allLimits(count);
            const int32_t testBurstLimitInterval = 100;

            EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, allLimits.data()));
            EXPECT_EQ(allLimits[0].level, ZES_POWER_LEVEL_BURST);
            allLimits[0].interval = testBurstLimitInterval;

            ze_result_t expectedResult = burstLimitIntervalResult;
            EXPECT_EQ(expectedResult, pPowerImp->setLimitsExt(&count, allLimits.data()));

            if (burstLimitIntervalResult == ZE_RESULT_SUCCESS) {
                EXPECT_EQ(allLimits[0].interval, testBurstLimitInterval);
            }
        }
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainsAndSysfsReadResultsForPeakPowerLimitWhenGetLimitsExtIsCalledThenProperResultsAreReturned, IsBMG) {
    pSysfsAccess->isCardSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isPackageSustainedPowerLimitFilePresent = false;
    pSysfsAccess->isCardBurstPowerLimitFilePresent = false;
    pSysfsAccess->isPackageBurstPowerLimitFilePresent = false;

    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        for (ze_result_t criticalLimitResult : {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS}) {
            pSysfsAccess->criticalReadResult = criticalLimitResult;

            uint32_t count = 0;
            EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
            EXPECT_EQ(1u, count);

            std::vector<zes_power_limit_ext_desc_t> allLimits(count);
            ze_result_t expectedResult = criticalLimitResult;

            EXPECT_EQ(expectedResult, pPowerImp->getLimitsExt(&count, allLimits.data()));

            if (criticalLimitResult == ZE_RESULT_SUCCESS) {
                EXPECT_EQ(allLimits[0].limit, static_cast<int32_t>(pSysfsAccess->criticalPowerLimitVal / milliFactor));
                EXPECT_EQ(allLimits[0].enabledStateLocked, true);
                EXPECT_EQ(allLimits[0].intervalValueLocked, true);
                EXPECT_EQ(allLimits[0].limitValueLocked, false);
                EXPECT_EQ(allLimits[0].source, ZES_POWER_SOURCE_ANY);
                EXPECT_EQ(allLimits[0].level, ZES_POWER_LEVEL_PEAK);
                EXPECT_EQ(allLimits[0].limitUnit, ZES_LIMIT_UNIT_POWER);
                EXPECT_EQ(allLimits[0].interval, 0);
            }
        }
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainsAndSysfsReadSucceedsForAllLimitsWhenGetLimitsExtIsCalledThenProperLimitValuesAreReturned, IsBMG) {

    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
        EXPECT_EQ(bmgPowerLimitSupportedCount, count);

        std::vector<zes_power_limit_ext_desc_t> allLimits(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, allLimits.data()));

        uint8_t index = 0;
        EXPECT_EQ(allLimits[index].limit, static_cast<int32_t>(pSysfsAccess->sustainedPowerLimitVal / milliFactor));
        EXPECT_EQ(allLimits[index].enabledStateLocked, true);
        EXPECT_EQ(allLimits[index].intervalValueLocked, false);
        EXPECT_EQ(allLimits[index].limitValueLocked, false);
        EXPECT_EQ(allLimits[index].source, ZES_POWER_SOURCE_ANY);
        EXPECT_EQ(allLimits[index].level, ZES_POWER_LEVEL_SUSTAINED);
        EXPECT_EQ(allLimits[index].limitUnit, ZES_LIMIT_UNIT_POWER);
        EXPECT_EQ(allLimits[index].interval, pSysfsAccess->sustainedPowerLimitIntervalVal);

        index++;
        EXPECT_EQ(allLimits[index].limit, static_cast<int32_t>(pSysfsAccess->burstPowerLimitVal / milliFactor));
        EXPECT_EQ(allLimits[index].enabledStateLocked, true);
        EXPECT_EQ(allLimits[index].intervalValueLocked, false);
        EXPECT_EQ(allLimits[index].limitValueLocked, false);
        EXPECT_EQ(allLimits[index].source, ZES_POWER_SOURCE_ANY);
        EXPECT_EQ(allLimits[index].level, ZES_POWER_LEVEL_BURST);
        EXPECT_EQ(allLimits[index].limitUnit, ZES_LIMIT_UNIT_POWER);
        EXPECT_EQ(allLimits[index].interval, pSysfsAccess->burstPowerLimitIntervalVal);

        index++;
        EXPECT_EQ(allLimits[index].limit, static_cast<int32_t>(pSysfsAccess->criticalPowerLimitVal / milliFactor));
        EXPECT_EQ(allLimits[index].enabledStateLocked, true);
        EXPECT_EQ(allLimits[index].intervalValueLocked, true);
        EXPECT_EQ(allLimits[index].limitValueLocked, false);
        EXPECT_EQ(allLimits[index].source, ZES_POWER_SOURCE_ANY);
        EXPECT_EQ(allLimits[index].level, ZES_POWER_LEVEL_PEAK);
        EXPECT_EQ(allLimits[index].limitUnit, ZES_LIMIT_UNIT_POWER);
        EXPECT_EQ(allLimits[index].interval, 0);
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandleForPowerDomainAndSysfsWriteResultsForPowerLimitsWhenSetLimitsExtIsCalledThenProperResultsAreReturned, IsBMG) {
    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_CARD, ZES_POWER_DOMAIN_PACKAGE};

    for (auto powerDomain : powerDomains) {
        auto pPowerImp = std::make_unique<XePublicLinuxPowerImp>(pOsSysman, false, 0, powerDomain);

        uint32_t count = 0u;
        EXPECT_EQ(ZE_RESULT_SUCCESS, pPowerImp->getLimitsExt(&count, nullptr));
        EXPECT_EQ(bmgPowerLimitSupportedCount, count);

        for (ze_result_t sustainedLimitResult : {ZE_RESULT_ERROR_NOT_AVAILABLE, ZE_RESULT_SUCCESS}) {
            for (ze_result_t burstLimitResult : {ZE_RESULT_ERROR_NOT_AVAILABLE, ZE_RESULT_SUCCESS}) {
                for (ze_result_t peakLimitResult : {ZE_RESULT_ERROR_NOT_AVAILABLE, ZE_RESULT_SUCCESS}) {
                    pSysfsAccess->sustainedReadResult = sustainedLimitResult;
                    pSysfsAccess->burstReadResult = burstLimitResult;
                    pSysfsAccess->criticalReadResult = peakLimitResult;
                    pSysfsAccess->sustainedWriteResult = sustainedLimitResult;
                    pSysfsAccess->burstWriteResult = burstLimitResult;
                    pSysfsAccess->criticalWriteResult = peakLimitResult;

                    ze_result_t expectedResult = ((sustainedLimitResult == ZE_RESULT_SUCCESS) && (burstLimitResult == ZE_RESULT_SUCCESS) && (peakLimitResult == ZE_RESULT_SUCCESS))
                                                     ? ZE_RESULT_SUCCESS
                                                     : ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
                    std::vector<zes_power_limit_ext_desc_t> allLimits(count);
                    EXPECT_EQ(expectedResult, pPowerImp->getLimitsExt(&count, allLimits.data()));

                    uint8_t index = 0;
                    const uint64_t testLimit = 400000;
                    const int32_t testSustainedLimitInterval = 500;
                    const int32_t testBurstLimitInterval = 1000;

                    if (sustainedLimitResult == ZE_RESULT_SUCCESS) {
                        allLimits[index].limit = testLimit;
                        allLimits[index].interval = testSustainedLimitInterval;
                        index++;
                    }
                    if (burstLimitResult == ZE_RESULT_SUCCESS) {
                        allLimits[index].limit = testLimit;
                        allLimits[index].interval = testBurstLimitInterval;
                        index++;
                    }
                    if (peakLimitResult == ZE_RESULT_SUCCESS) {
                        allLimits[index].limit = testLimit;
                        index++;
                    }

                    EXPECT_EQ(expectedResult, pPowerImp->setLimitsExt(&count, allLimits.data()));
                    EXPECT_EQ(expectedResult, pPowerImp->getLimitsExt(&count, allLimits.data()));

                    index = 0;
                    if (sustainedLimitResult == ZE_RESULT_SUCCESS) {
                        EXPECT_EQ(allLimits[index].limit, static_cast<int32_t>(testLimit));
                        EXPECT_EQ(allLimits[index].interval, testSustainedLimitInterval);
                        index++;
                    }
                    if (burstLimitResult == ZE_RESULT_SUCCESS) {
                        EXPECT_EQ(allLimits[index].limit, static_cast<int32_t>(testLimit));
                        EXPECT_EQ(allLimits[index].interval, testBurstLimitInterval);
                        index++;
                    }
                    if (peakLimitResult == ZE_RESULT_SUCCESS) {
                        EXPECT_EQ(allLimits[index].limit, static_cast<int32_t>(testLimit));
                        index++;
                    }
                }
            }
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
