/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint64_t xeMockEnergyCounter = 123456785u;
constexpr uint32_t xeMockDefaultPowerLimitVal = 600000000;
constexpr uint64_t xeMockMinPowerLimitVal = 30000000;
constexpr uint64_t xeMockMaxPowerLimitVal = 600000000;

const std::string xeHwmonDir("device/hwmon/hwmon1");
const std::string xeCardEnergyCounterNode("energy1_input");
const std::string xeCardBurstLimitNode("power1_cap");
const std::string xeCardBurstLimitIntervalNode("power1_cap_interval");
const std::string xeCardSustainedLimitNode("power1_max");
const std::string xeCardSustainedLimitIntervalNode("power1_max_interval");
const std::string xeCardDefaultLimitNode("power1_rated_max");
const std::string xeCardCriticalLimitNode("power1_crit");
const std::string xePackageEnergyCounterNode("energy2_input");
const std::string xePackageBurstLimitNode("power2_cap");
const std::string xePackageBurstLimitIntervalNode("power2_cap_interval");
const std::string xePackageSustainedLimitNode("power2_max");
const std::string xePackageSustainedLimitIntervalNode("power2_max_interval");
const std::string xePackageDefaultLimitNode("power2_rated_max");
const std::string xePackageCriticalLimitNode("power2_crit");

class MockXePowerSysfsAccess : public L0::Sysman::SysFsAccessInterface {

  public:
    ze_result_t mockScanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteIntResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWritePeakLimitResult = ZE_RESULT_SUCCESS;
    std::vector<ze_result_t> mockReadValueUnsignedLongResult{};
    std::vector<ze_result_t> mockWriteValueUnsignedLongResult{};

    bool isCardEnergyCounterFilePresent = true;
    bool isCardSustainedPowerLimitFilePresent = true;
    bool isCardBurstPowerLimitFilePresent = true;
    bool isCardCriticalPowerLimitFilePresent = true;

    bool isPackageEnergyCounterFilePresent = true;
    bool isPackageSustainedPowerLimitFilePresent = true;
    bool isPackageBurstPowerLimitFilePresent = true;
    bool isPackageCriticalPowerLimitFilePresent = true;

    ze_result_t defaultReadResult = ZE_RESULT_SUCCESS;
    ze_result_t sustainedReadResult = ZE_RESULT_SUCCESS;
    ze_result_t sustainedIntervalReadResult = ZE_RESULT_SUCCESS;
    ze_result_t burstReadResult = ZE_RESULT_SUCCESS;
    ze_result_t burstIntervalReadResult = ZE_RESULT_SUCCESS;
    ze_result_t criticalReadResult = ZE_RESULT_SUCCESS;
    ze_result_t sustainedWriteResult = ZE_RESULT_SUCCESS;
    ze_result_t sustainedIntervalWriteResult = ZE_RESULT_SUCCESS;
    ze_result_t burstWriteResult = ZE_RESULT_SUCCESS;
    ze_result_t burstIntervalWriteResult = ZE_RESULT_SUCCESS;
    ze_result_t criticalWriteResult = ZE_RESULT_SUCCESS;

    int32_t sustainedPowerLimitIntervalVal = 1000;
    int32_t burstPowerLimitIntervalVal = 500;
    uint64_t sustainedPowerLimitVal = 350000000;
    uint64_t burstPowerLimitVal = 450000000;
    uint64_t criticalPowerLimitVal = 550000000;

    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }
        ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
        if (file.compare(xeHwmonDir + "/" + "name") == 0) {
            val = "xe";
            result = ZE_RESULT_SUCCESS;
        } else {
            val = "";
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t read(const std::string file, int32_t &val) override {
        if ((file.compare(xeHwmonDir + "/" + xeCardSustainedLimitIntervalNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageSustainedLimitIntervalNode) == 0) && sustainedIntervalReadResult == ZE_RESULT_SUCCESS) {
            val = sustainedPowerLimitIntervalVal;
            return ZE_RESULT_SUCCESS;
        } else if ((file.compare(xeHwmonDir + "/" + xeCardBurstLimitIntervalNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageBurstLimitIntervalNode) == 0) && burstIntervalReadResult == ZE_RESULT_SUCCESS) {
            val = burstPowerLimitIntervalVal;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if ((file.compare(xeHwmonDir + "/" + xeCardEnergyCounterNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageEnergyCounterNode) == 0)) {
            val = xeMockEnergyCounter;
        } else if ((file.compare(xeHwmonDir + "/" + xeCardDefaultLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageDefaultLimitNode) == 0) && defaultReadResult == ZE_RESULT_SUCCESS) {
            val = xeMockDefaultPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + xeCardSustainedLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageSustainedLimitNode) == 0) && sustainedReadResult == ZE_RESULT_SUCCESS) {
            val = sustainedPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + xeCardBurstLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageBurstLimitNode) == 0) && burstReadResult == ZE_RESULT_SUCCESS) {
            val = burstPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + xeCardCriticalLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageCriticalLimitNode) == 0) && criticalReadResult == ZE_RESULT_SUCCESS) {
            val = criticalPowerLimitVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t write(const std::string file, const int32_t val) override {
        if (mockWriteIntResult != ZE_RESULT_SUCCESS) {
            return mockWriteIntResult;
        }

        ze_result_t result = ZE_RESULT_SUCCESS;
        if ((file.compare(xeHwmonDir + "/" + xeCardSustainedLimitIntervalNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageSustainedLimitIntervalNode) == 0) && sustainedIntervalWriteResult == ZE_RESULT_SUCCESS) {
            sustainedPowerLimitIntervalVal = val;
        } else if ((file.compare(xeHwmonDir + "/" + xeCardBurstLimitIntervalNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageBurstLimitIntervalNode) == 0) && burstIntervalWriteResult == ZE_RESULT_SUCCESS) {
            burstPowerLimitIntervalVal = val;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t write(const std::string file, const uint64_t val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if ((file.compare(xeHwmonDir + "/" + xeCardSustainedLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageSustainedLimitNode) == 0) && sustainedWriteResult == ZE_RESULT_SUCCESS) {
            if (val < xeMockMinPowerLimitVal) {
                sustainedPowerLimitVal = xeMockMinPowerLimitVal;
            } else if (val > xeMockMaxPowerLimitVal) {
                sustainedPowerLimitVal = xeMockMaxPowerLimitVal;
            } else {
                sustainedPowerLimitVal = val;
            }
        } else if ((file.compare(xeHwmonDir + "/" + xeCardBurstLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageBurstLimitNode) == 0) && burstWriteResult == ZE_RESULT_SUCCESS) {
            if (val < xeMockMinPowerLimitVal) {
                burstPowerLimitVal = xeMockMinPowerLimitVal;
            } else if (val > xeMockMaxPowerLimitVal) {
                burstPowerLimitVal = xeMockMaxPowerLimitVal;
            } else {
                burstPowerLimitVal = val;
            }
        } else if ((file.compare(xeHwmonDir + "/" + xeCardCriticalLimitNode) == 0 || file.compare(xeHwmonDir + "/" + xePackageCriticalLimitNode) == 0) && criticalWriteResult == ZE_RESULT_SUCCESS) {
            if (val < xeMockMinPowerLimitVal) {
                burstPowerLimitVal = xeMockMinPowerLimitVal;
            } else if (val > xeMockMaxPowerLimitVal) {
                burstPowerLimitVal = xeMockMaxPowerLimitVal;
            } else {
                criticalPowerLimitVal = val;
            }
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t scanDirEntries(const std::string path, std::vector<std::string> &listOfEntries) override {
        const std::string hwmonDir("device/hwmon");
        if (mockScanDirEntriesResult != ZE_RESULT_SUCCESS) {
            return mockScanDirEntriesResult;
        }
        if (path.compare(hwmonDir) == 0) {
            listOfEntries.push_back("hwmon1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    bool fileExists(const std::string file) override {
        if (file.find(xeCardEnergyCounterNode) != std::string::npos) {
            return isCardEnergyCounterFilePresent;
        } else if (file.find(xePackageEnergyCounterNode) != std::string::npos) {
            return isPackageEnergyCounterFilePresent;
        } else if (file.find(xeCardSustainedLimitNode) != std::string::npos) {
            return isCardSustainedPowerLimitFilePresent;
        } else if (file.find(xePackageSustainedLimitNode) != std::string::npos) {
            return isPackageSustainedPowerLimitFilePresent;
        } else if (file.find(xeCardBurstLimitNode) != std::string::npos) {
            return isCardBurstPowerLimitFilePresent;
        } else if (file.find(xePackageBurstLimitNode) != std::string::npos) {
            return isPackageBurstPowerLimitFilePresent;
        } else if (file.find(xeCardCriticalLimitNode) != std::string::npos) {
            return isCardCriticalPowerLimitFilePresent;
        } else if (file.find(xePackageCriticalLimitNode) != std::string::npos) {
            return isPackageCriticalPowerLimitFilePresent;
        }
        return false;
    }

    MockXePowerSysfsAccess() = default;
};

struct MockXePowerFsAccess : public L0::Sysman::FsAccessInterface {
    MockXePowerFsAccess() = default;
};

class XePublicLinuxPowerImp : public L0::Sysman::LinuxPowerImp {
  public:
    XePublicLinuxPowerImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) : L0::Sysman::LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, powerDomain) {}
    using L0::Sysman::LinuxPowerImp::isTelemetrySupportAvailable;
    using L0::Sysman::LinuxPowerImp::pSysfsAccess;

    bool mockGetPropertiesFail = false;
    bool mockGetPropertiesExtFail = false;

    ze_result_t getProperties(zes_power_properties_t *pProperties) override {
        if (mockGetPropertiesFail == true) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return LinuxPowerImp::getProperties(pProperties);
    }

    ze_result_t getPropertiesExt(zes_power_ext_properties_t *pExtProperties) override {
        if (mockGetPropertiesExtFail == true) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return LinuxPowerImp::getPropertiesExt(pExtProperties);
    }
};

class SysmanDevicePowerFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockXePowerFsAccess *pFsAccess = nullptr;
    MockXePowerSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pFsAccess = new MockXePowerFsAccess();
        pSysfsAccess = new MockXePowerSysfsAccess();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pFsAccess = pFsAccess;
        pSysfsAccess->mockScanDirEntriesResult = ZE_RESULT_SUCCESS;
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> getPowerHandles() {
        uint32_t count = 0;
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0