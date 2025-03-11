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

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenComponentCountZeroWhenEnumeratingPowerDomainsWhenPmtSupportIsAvailableThenValidCountIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validOobmsmGuid = "0x5e2f8211";
        std::string validPunitGuid = "0x1e2f8200";

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
        uint64_t telemOffset = 0;
        std::string dummyGuid = "0xABCDEF";

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
        uint64_t telemOffset = 0;
        std::string validOobmsmGuid = "0x5e2f8211";
        std::string validPunitGuid = "0x1e2f8200";

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
            }
        } else if (fd == 8) {
            switch (offset) {
            case 4:
                count = (readFailCount == 2) ? -1 : sizeof(uint32_t);
                break;
            case 128:
                count = (readFailCount == 3) ? -1 : sizeof(uint64_t);
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

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandlesWhenGettingPowerEnergyCounterThenValidValuesAreReturned, IsBMG) {
    static uint32_t mockEnergyCounter = 0xabcd;
    static uint32_t mockXtalFrequency = 0xef;
    static uint64_t mockTimestamp = 0xabef;
    static double indexToXtalClockFrequecyMap[4] = {24, 19.2, 38.4, 25};

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        uint64_t telemOffset = 0;
        std::string validOobmsmGuid = "0x5e2f8211";
        std::string validPunitGuid = "0x1e2f8200";

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
            }
        } else if (fd == 8) {
            switch (offset) {
            case 4:
                memcpy(buf, &mockXtalFrequency, count);
                break;
            case 128:
                memcpy(buf, &mockTimestamp, count);
                break;
            case 1628:
            case 1640:
                memcpy(buf, &mockEnergyCounter, count);
                break;
            }
        }
        return count;
    });

    auto handles = getPowerHandles(bmgPowerHandleComponentCount);

    for (auto handle : handles) {
        zes_power_energy_counter_t energyCounter = {};
        auto result = zesPowerGetEnergyCounter(handle, &energyCounter);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        uint32_t integerPart = static_cast<uint32_t>(mockEnergyCounter >> 14);
        uint32_t decimalBits = static_cast<uint32_t>(mockEnergyCounter & 0x3FFF);
        double decimalPart = static_cast<double>(decimalBits) / (1 << 14);
        double finalValue = static_cast<double>(integerPart + decimalPart);
        uint64_t expectedEnergyCounter = static_cast<uint64_t>((finalValue * convertJouleToMicroJoule));
        EXPECT_EQ(expectedEnergyCounter, energyCounter.energy);

        double timestamp = mockTimestamp / indexToXtalClockFrequecyMap[mockXtalFrequency & 0x2];
        uint64_t expectedTimestamp = static_cast<uint64_t>(timestamp);
        EXPECT_EQ(expectedTimestamp, energyCounter.timestamp);
    }
}

HWTEST2_F(SysmanXeProductHelperPowerTest, GivenValidPowerHandlesWithTelemetrySupportNotAvailableButSysfsReadSucceedsWhenGettingPowerEnergyCounterThenValidPowerReadingsRetrievedFromSysfsNode, IsBMG) {
    std::vector<zes_power_domain_t> powerDomains = {ZES_POWER_DOMAIN_PACKAGE, ZES_POWER_DOMAIN_CARD};
    for (const auto &powerDomain : powerDomains) {
        zes_power_energy_counter_t energyCounter = {};
        std::unique_ptr<PublicLinuxPowerImp> pLinuxPowerImp(new PublicLinuxPowerImp(pOsSysman, false, 0, powerDomain));
        pLinuxPowerImp->isTelemetrySupportAvailable = false;
        uint64_t timeStampInitial = SysmanDevice::getSysmanTimestamp();
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxPowerImp->getEnergyCounter(&energyCounter));
        EXPECT_EQ(energyCounter.energy, expectedEnergyCounter);
        EXPECT_GE(energyCounter.timestamp, timeStampInitial);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
