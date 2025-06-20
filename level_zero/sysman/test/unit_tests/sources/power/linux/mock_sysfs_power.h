/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"
#include "level_zero/sysman/source/api/power/sysman_power_imp.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint64_t mockKeyOffset = 0x420;
constexpr uint32_t mockLimitCount = 2u;
const std::string hwmonDir("device/hwmon");
const std::string i915HwmonDir("device/hwmon/hwmon2");
const std::string nonI915HwmonDir("device/hwmon/hwmon1");
const std::string i915HwmonDirTile0("device/hwmon/hwmon3");
const std::string i915HwmonDirTile1("device/hwmon/hwmon4");
const std::vector<std::string> listOfMockedHwmonDirs = {"hwmon0", "hwmon1", "hwmon2", "hwmon3", "hwmon4"};
const std::string sustainedPowerLimit("power1_max");
const std::string sustainedPowerLimitInterval("power1_max_interval");
const std::string criticalPowerLimit1("curr1_crit");
const std::string criticalPowerLimit2("power1_crit");
const std::string packageEnergyCounterNode("energy1_input");
const std::string defaultPowerLimit("power1_rated_max");
constexpr uint64_t expectedEnergyCounter = 123456785u;
constexpr uint64_t expectedEnergyCounterTile0 = 123456785u;
constexpr uint64_t expectedEnergyCounterTile1 = 128955785u;
constexpr uint32_t mockDefaultPowerLimitVal = 600000000;
constexpr uint64_t mockMinPowerLimitVal = 300000000;
constexpr uint64_t mockMaxPowerLimitVal = 600000000;

const std::string realPathTelem1 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
const std::string realPathTelem2 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2";
const std::string realPathTelem3 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem3";
const std::string sysfsPathTelem1 = "/sys/class/intel_pmt/telem1";
const std::string sysfsPathTelem2 = "/sys/class/intel_pmt/telem2";
const std::string sysfsPathTelem3 = "/sys/class/intel_pmt/telem3";
const std::string telem1OffsetFileName("/sys/class/intel_pmt/telem1/offset");
const std::string telem1GuidFileName("/sys/class/intel_pmt/telem1/guid");
const std::string telem1TelemFileName("/sys/class/intel_pmt/telem1/telem");
const std::string telem2OffsetFileName("/sys/class/intel_pmt/telem2/offset");
const std::string telem2GuidFileName("/sys/class/intel_pmt/telem2/guid");
const std::string telem2TelemFileName("/sys/class/intel_pmt/telem2/telem");
const std::string telem3OffsetFileName("/sys/class/intel_pmt/telem3/offset");
const std::string telem3GuidFileName("/sys/class/intel_pmt/telem3/guid");
const std::string telem3TelemFileName("/sys/class/intel_pmt/telem3/telem");

struct MockPowerSysfsAccessInterface : public L0::Sysman::SysFsAccessInterface {

    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadPeakResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadIntResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWritePeakLimitResult = ZE_RESULT_SUCCESS;
    std::vector<ze_result_t> mockscanDirEntriesResult{};
    std::vector<ze_result_t> mockReadValUnsignedLongResult{};
    std::vector<ze_result_t> mockWriteUnsignedResult{};

    uint64_t sustainedPowerLimitVal = 0;
    uint64_t criticalPowerLimitVal = 0;
    int32_t sustainedPowerLimitIntervalVal = 0;
    bool isEnergyCounterFilePresent = true;
    bool isSustainedPowerLimitFilePresent = true;
    bool isCriticalPowerLimitFilePresent = true;

    ze_result_t getValString(const std::string file, std::string &val) {
        ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
        if (file.compare(i915HwmonDir + "/" + "name") == 0) {
            val = "i915";
            result = ZE_RESULT_SUCCESS;
        } else if (file.compare(nonI915HwmonDir + "/" + "name") == 0) {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else if (file.compare(i915HwmonDirTile1 + "/" + "name") == 0) {
            val = "i915_gt1";
            result = ZE_RESULT_SUCCESS;
        } else if (file.compare(i915HwmonDirTile0 + "/" + "name") == 0) {
            val = "i915_gt0";
            result = ZE_RESULT_SUCCESS;
        } else {
            val = "garbageI915";
            result = ZE_RESULT_SUCCESS;
        }
        return result;
    }

    ze_result_t getValUnsignedLongHelper(const std::string file, uint64_t &val);
    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            val = sustainedPowerLimitVal;
        } else if ((file.compare(i915HwmonDir + "/" + criticalPowerLimit1) == 0) || (file.compare(i915HwmonDir + "/" + criticalPowerLimit2) == 0)) {
            if (mockReadPeakResult != ZE_RESULT_SUCCESS) {
                return mockReadPeakResult;
            }
            val = criticalPowerLimitVal;
        } else if (file.compare(i915HwmonDirTile0 + "/" + packageEnergyCounterNode) == 0) {
            val = expectedEnergyCounterTile0;
        } else if (file.compare(i915HwmonDirTile1 + "/" + packageEnergyCounterNode) == 0) {
            val = expectedEnergyCounterTile1;
        } else if (file.compare(i915HwmonDir + "/" + packageEnergyCounterNode) == 0) {
            val = expectedEnergyCounter;
        } else if (file.compare(i915HwmonDir + "/" + defaultPowerLimit) == 0) {
            val = mockDefaultPowerLimitVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t getValUnsignedInt(const std::string file, uint32_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + defaultPowerLimit) == 0) {
            val = mockDefaultPowerLimitVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t setVal(const std::string file, const int val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if ((file.compare(i915HwmonDir + "/" + sustainedPowerLimitInterval) == 0)) {
            sustainedPowerLimitIntervalVal = val;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t getscanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) {
        if (file.compare(hwmonDir) == 0) {
            listOfEntries = listOfMockedHwmonDirs;
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

        return getValUnsignedLong(file, val);
    }

    ze_result_t read(const std::string file, int32_t &val) override {
        if (mockReadIntResult != ZE_RESULT_SUCCESS) {
            return mockReadIntResult;
        }

        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitInterval) == 0) {
            val = sustainedPowerLimitIntervalVal;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }

        return getValString(file, val);
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }
        return getValUnsignedInt(file, val);
    }

    ze_result_t write(const std::string file, const int32_t val) override {
        if (mockWriteResult != ZE_RESULT_SUCCESS) {
            return mockWriteResult;
        }

        return setVal(file, val);
    }

    ze_result_t write(const std::string file, const uint64_t val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (!mockWriteUnsignedResult.empty()) {
            result = mockWriteUnsignedResult.front();
            mockWriteUnsignedResult.erase(mockWriteUnsignedResult.begin());
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }

        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            if (val < mockMinPowerLimitVal) {
                sustainedPowerLimitVal = mockMinPowerLimitVal;
            } else if (val > mockMaxPowerLimitVal) {
                sustainedPowerLimitVal = mockMaxPowerLimitVal;
            } else {
                sustainedPowerLimitVal = val;
            }
        } else if ((file.compare(i915HwmonDir + "/" + criticalPowerLimit1) == 0) || (file.compare(i915HwmonDir + "/" + criticalPowerLimit2) == 0)) {
            if (mockWritePeakLimitResult != ZE_RESULT_SUCCESS) {
                return mockWritePeakLimitResult;
            }
            criticalPowerLimitVal = val;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t scanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) override {
        if (!mockscanDirEntriesResult.empty()) {
            ze_result_t result = mockscanDirEntriesResult.front();
            mockscanDirEntriesResult.erase(mockscanDirEntriesResult.begin());
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }
        return getscanDirEntries(file, listOfEntries);
    }

    bool fileExists(const std::string file) override {
        if (file.find(packageEnergyCounterNode) != std::string::npos) {
            return isEnergyCounterFilePresent;
        } else if (file.find(sustainedPowerLimit) != std::string::npos) {
            return isSustainedPowerLimitFilePresent;
        } else if (file.find(criticalPowerLimit1) != std::string::npos) {
            return isCriticalPowerLimitFilePresent;
        } else if (file.find(criticalPowerLimit2) != std::string::npos) {
            return isCriticalPowerLimitFilePresent;
        }
        return false;
    }

    MockPowerSysfsAccessInterface() = default;
};

struct MockPowerFsAccessInterface : public L0::Sysman::FsAccessInterface {
    MockPowerFsAccessInterface() = default;
};

class PublicLinuxPowerImp : public L0::Sysman::LinuxPowerImp {
  public:
    PublicLinuxPowerImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) : L0::Sysman::LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, powerDomain) {}
    using L0::Sysman::LinuxPowerImp::isTelemetrySupportAvailable;
    using L0::Sysman::LinuxPowerImp::pSysfsAccess;
};

class SysmanDevicePowerFixtureI915 : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = nullptr;
    MockPowerSysfsAccessInterface *pSysfsAccess = nullptr;
    MockPowerFsAccessInterface *pFsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pFsAccess = new MockPowerFsAccessInterface();
        pSysfsAccess = new MockPowerSysfsAccessInterface();
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pSysfsAccess->mockscanDirEntriesResult.push_back(ZE_RESULT_SUCCESS);
        pLinuxSysmanImp->pFsAccess = pFsAccess;
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

class SysmanDevicePowerMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    std::unique_ptr<PublicLinuxPowerImp> pPublicLinuxPowerImp;
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = nullptr;
    MockPowerSysfsAccessInterface *pSysfsAccess = nullptr;

    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccess = new MockPowerSysfsAccessInterface();
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess;
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
