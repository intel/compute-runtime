/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"
#include "level_zero/sysman/source/api/power/sysman_power_imp.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string hwmonDir("device/hwmon");
const std::string xeHwmonDir("device/hwmon/hwmon1");
const std::string baseTelemSysFS("/sys/class/intel_pmt");
const std::string sustainedPowerLimit("power1_max");
const std::string sustainedPowerLimitInterval("power1_max_interval");
const std::string criticalPowerLimit("power1_crit");
const std::string criticalPowerLimit2("curr1_crit");
const std::string energyCounterNode("energy1_input");
const std::string defaultPowerLimit("power1_rated_max");

const std::string packageSustainedPowerLimit("power2_max");
const std::string packageSustainedPowerLimitInterval("power2_max_interval");
const std::string packageCriticalPowerLimit1("power2_crit");
const std::string packageCriticalPowerLimit2("curr2_crit");
const std::string packageEnergyCounterNode("energy2_input");
const std::string packageDefaultPowerLimit("power2_rated_max");
constexpr uint32_t mockDefaultPowerLimitVal = 600000000;
constexpr uint64_t expectedEnergyCounter = 123456785u;
constexpr uint64_t mockMinPowerLimitVal = 300000000;
constexpr uint64_t mockMaxPowerLimitVal = 600000000;

struct MockXePowerSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    uint64_t sustainedPowerLimitVal = 0;
    uint64_t criticalPowerLimitVal = 0;
    int32_t sustainedPowerLimitIntervalVal = 0;
    std::vector<ze_result_t> mockReadValUnsignedLongResult{};
    ze_result_t mockScanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;

    bool isCardEnergyCounterFilePresent = true;
    bool isPackageEnergyCounterFilePresent = true;
    bool isSustainedPowerLimitFilePresent = true;
    bool isPackagedSustainedPowerLimitFilePresent = true;
    bool isCriticalPowerLimitFilePresent = true;
    bool isPackageCriticalPowerLimit1Present = true;
    bool isPackageCriticalPowerLimit2Present = true;
    bool isTelemDataAvailable = true;

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

    ze_result_t scanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) override {
        if (mockScanDirEntriesResult != ZE_RESULT_SUCCESS) {
            return mockScanDirEntriesResult;
        }
        if (file.compare(hwmonDir) == 0) {
            listOfEntries.push_back("hwmon1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {

        ze_result_t result = ZE_RESULT_SUCCESS;

        if (!mockReadValUnsignedLongResult.empty()) {
            result = mockReadValUnsignedLongResult.front();
            mockReadValUnsignedLongResult.erase(mockReadValUnsignedLongResult.begin());
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }

        if ((file.compare(xeHwmonDir + "/" + sustainedPowerLimit) == 0) || (file.compare(xeHwmonDir + "/" + packageSustainedPowerLimit) == 0)) {
            val = sustainedPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + criticalPowerLimit) == 0) || (file.compare(xeHwmonDir + "/" + packageCriticalPowerLimit1) == 0)) {
            val = criticalPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + criticalPowerLimit2) == 0) || (file.compare(xeHwmonDir + "/" + packageCriticalPowerLimit2) == 0)) {
            val = criticalPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + defaultPowerLimit) == 0) || (file.compare(xeHwmonDir + "/" + packageDefaultPowerLimit) == 0)) {
            val = mockDefaultPowerLimitVal;
        } else if ((file.compare(xeHwmonDir + "/" + energyCounterNode) == 0) || (file.compare(xeHwmonDir + "/" + packageEnergyCounterNode) == 0)) {
            val = expectedEnergyCounter;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t read(const std::string file, int32_t &val) override {

        if ((file.compare(xeHwmonDir + "/" + sustainedPowerLimitInterval) == 0) || (file.compare(xeHwmonDir + "/" + packageSustainedPowerLimitInterval) == 0)) {
            val = sustainedPowerLimitIntervalVal;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t write(const std::string file, const uint64_t val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if ((file.compare(xeHwmonDir + "/" + sustainedPowerLimit) == 0) || (file.compare(xeHwmonDir + "/" + packageSustainedPowerLimit) == 0)) {
            if (val < mockMinPowerLimitVal) {
                sustainedPowerLimitVal = mockMinPowerLimitVal;
            } else if (val > mockMaxPowerLimitVal) {
                sustainedPowerLimitVal = mockMaxPowerLimitVal;
            } else {
                sustainedPowerLimitVal = val;
            }
        } else if ((file.compare(xeHwmonDir + "/" + criticalPowerLimit) == 0) || (file.compare(xeHwmonDir + "/" + packageCriticalPowerLimit1) == 0)) {
            criticalPowerLimitVal = val;
        } else if ((file.compare(xeHwmonDir + "/" + criticalPowerLimit2) == 0) || (file.compare(xeHwmonDir + "/" + packageCriticalPowerLimit2) == 0)) {
            criticalPowerLimitVal = val;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t write(const std::string file, const int val) override {

        ze_result_t result = ZE_RESULT_SUCCESS;
        if ((file.compare(xeHwmonDir + "/" + sustainedPowerLimitInterval) == 0) || (file.compare(xeHwmonDir + "/" + packageSustainedPowerLimitInterval) == 0)) {
            sustainedPowerLimitIntervalVal = val;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    bool fileExists(const std::string file) override {
        if (file.find(energyCounterNode) != std::string::npos) {
            return isCardEnergyCounterFilePresent;
        } else if (file.find(packageEnergyCounterNode) != std::string::npos) {
            return isPackageEnergyCounterFilePresent;
        } else if (file.find(sustainedPowerLimit) != std::string::npos) {
            return isSustainedPowerLimitFilePresent;
        } else if (file.find(packageSustainedPowerLimit) != std::string::npos) {
            return isPackagedSustainedPowerLimitFilePresent;
        } else if (file.find(criticalPowerLimit) != std::string::npos) {
            return isCriticalPowerLimitFilePresent;
        } else if (file.find(criticalPowerLimit2) != std::string::npos) {
            return isCriticalPowerLimitFilePresent;
        } else if (file.find(packageCriticalPowerLimit1) != std::string::npos) {
            return isPackageCriticalPowerLimit1Present;
        } else if (file.find(packageCriticalPowerLimit2) != std::string::npos) {
            return isPackageCriticalPowerLimit2Present;
        }
        return false;
    }

    MockXePowerSysfsAccess() = default;
};

struct MockXePowerFsAccess : public L0::Sysman::FsAccessInterface {

    ze_result_t listDirectory(const std::string directory, std::vector<std::string> &listOfTelemNodes) override {
        if (directory.compare(baseTelemSysFS) == 0) {
            listOfTelemNodes.push_back("telem1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getRealPath(const std::string path, std::string &buf) override {
        if (path.compare("/sys/class/intel_pmt/telem1") == 0) {
            buf = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    MockXePowerFsAccess() = default;
};

class PublicLinuxPowerImp : public L0::Sysman::LinuxPowerImp {
  public:
    PublicLinuxPowerImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) : L0::Sysman::LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, powerDomain) {}
    using L0::Sysman::LinuxPowerImp::pSysfsAccess;
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
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

class SysmanMultiDevicePowerFixtureXe : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockXePowerFsAccess *pFsAccess = nullptr;
    MockXePowerSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pFsAccess = new MockXePowerFsAccess();
        pSysfsAccess = new MockXePowerSysfsAccess();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
        pSysmanDeviceImp->pPowerHandleContext->handleList.clear();
        getPowerHandles(0);
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0