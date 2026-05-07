/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/temperature/linux/sysman_os_temperature_imp.h"
#include "level_zero/sysman/source/api/temperature/sysman_temperature_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t offsetComputeTemperatures = 104;
constexpr uint32_t offsetCoreTemperatures = 108;
constexpr uint32_t offsetSocTemperatures1 = 96;
constexpr uint32_t offsetSocTemperatures2 = 56;

constexpr uint32_t offsetTileMaxTemperature = 44;
constexpr uint32_t offsetGtMaxTemperature = 52;
constexpr uint32_t offsetHbm0MaxDeviceTemperature = 28;
constexpr uint32_t offsetHbm1MaxDeviceTemperature = 36;
constexpr uint32_t offsetHbm2MaxDeviceTemperature = 300;
constexpr uint32_t offsetHbm3MaxDeviceTemperature = 308;

constexpr uint8_t memory0MaxTemperature = 0x12;
constexpr uint8_t memory1MaxTemperature = 0x45;
constexpr uint8_t memory2MaxTemperature = 0x32;
constexpr uint8_t memory3MaxTemperature = 0x36;
constexpr uint32_t gtMaxTemperature = 0x1d;
constexpr uint32_t tileMaxTemperature = 0x34;

constexpr uint8_t computeTempIndex = 8;
constexpr uint8_t coreTempIndex = 12;
constexpr uint8_t socTempIndex = 0;
constexpr uint8_t tempArrForNoSubDevices[19] = {0x09, 0x23, 0x43, 0xde, 0xa3, 0xce, 0x23, 0x11, 0x45, 0x32, 0x67, 0x47, 0xac, 0x21, 0x03, 0x90, 0, 0, 0};
constexpr uint8_t computeIndexForNoSubDevices = 9;
constexpr uint8_t gtTempIndexForNoSubDevices = 0;
const std::string baseTelemSysFS("/sys/class/intel_pmt");
const std::string realPathTelem1 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
const std::string realPathTelem2 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2";
const std::string realPathTelem3 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem3";
const std::string realPathTelem4 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem4";
const std::string realPathTelem5 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem5";
const std::string realPathTelem6 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem6";
const std::string sysfsPathTelem1 = "/sys/class/intel_pmt/telem1";
const std::string sysfsPathTelem2 = "/sys/class/intel_pmt/telem2";
const std::string sysfsPathTelem3 = "/sys/class/intel_pmt/telem3";
const std::string sysfsPathTelem4 = "/sys/class/intel_pmt/telem4";
const std::string sysfsPathTelem5 = "/sys/class/intel_pmt/telem5";
const std::string sysfsPathTelem6 = "/sys/class/intel_pmt/telem6";
const std::string telem1OffsetFileName("/sys/class/intel_pmt/telem1/offset");
const std::string telem1GuidFileName("/sys/class/intel_pmt/telem1/guid");
const std::string telem1TelemFileName("/sys/class/intel_pmt/telem1/telem");
const std::string telem2OffsetFileName("/sys/class/intel_pmt/telem2/offset");
const std::string telem2GuidFileName("/sys/class/intel_pmt/telem2/guid");
const std::string telem2TelemFileName("/sys/class/intel_pmt/telem2/telem");
const std::string telem3OffsetFileName("/sys/class/intel_pmt/telem3/offset");
const std::string telem3GuidFileName("/sys/class/intel_pmt/telem3/guid");
const std::string telem3TelemFileName("/sys/class/intel_pmt/telem3/telem");
const std::string telem4OffsetFileName("/sys/class/intel_pmt/telem4/offset");
const std::string telem4GuidFileName("/sys/class/intel_pmt/telem4/guid");
const std::string telem4TelemFileName("/sys/class/intel_pmt/telem4/telem");
const std::string telem5OffsetFileName("/sys/class/intel_pmt/telem5/offset");
const std::string telem5GuidFileName("/sys/class/intel_pmt/telem5/guid");
const std::string telem5TelemFileName("/sys/class/intel_pmt/telem5/telem");
const std::string telem6OffsetFileName("/sys/class/intel_pmt/telem6/offset");
const std::string telem6GuidFileName("/sys/class/intel_pmt/telem6/guid");
const std::string telem6TelemFileName("/sys/class/intel_pmt/telem6/telem");
const std::string mockTemperatureHwmonDir("device/hwmon");
const std::string mockTemperatureHwmonNameFile0("device/hwmon/hwmon0/name");
const std::string mockTemperatureHwmonNameFile1("device/hwmon/hwmon1/name");
const std::string mockTemperatureHwmonTempFile0("device/hwmon/hwmon0/temp2_max");

class MockTemperatureSysfsAccess : public L0::Sysman::SysFsAccessInterface {
  public:
    ze_result_t scanResult = ZE_RESULT_SUCCESS;
    std::vector<std::string> directoryEntries = {"hwmon0"};
    ze_result_t hwmonNameReadResult0 = ZE_RESULT_SUCCESS;
    ze_result_t hwmonNameReadResult1 = ZE_RESULT_SUCCESS;
    ze_result_t temp2MaxReadResult = ZE_RESULT_SUCCESS;
    std::string hwmonName0 = "xe";
    std::string hwmonName1 = "dummy";
    int32_t temp2MaxValue = 65000;
    bool temp2MaxExists = true;

    ze_result_t read(const std::string file, std::string &val) override {
        if (file == mockTemperatureHwmonNameFile0) {
            if (hwmonNameReadResult0 == ZE_RESULT_SUCCESS) {
                val = hwmonName0;
            }
            return hwmonNameReadResult0;
        }
        if (file == mockTemperatureHwmonNameFile1) {
            if (hwmonNameReadResult1 == ZE_RESULT_SUCCESS) {
                val = hwmonName1;
            }
            return hwmonNameReadResult1;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, int32_t &val) override {
        if (file == mockTemperatureHwmonTempFile0) {
            if (temp2MaxReadResult == ZE_RESULT_SUCCESS) {
                val = temp2MaxValue;
            }
            return temp2MaxReadResult;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t scanDirEntries(const std::string path, std::vector<std::string> &listOfEntries) override {
        if (scanResult != ZE_RESULT_SUCCESS) {
            return scanResult;
        }
        if (path != mockTemperatureHwmonDir) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        listOfEntries = directoryEntries;
        return ZE_RESULT_SUCCESS;
    }

    bool fileExists(const std::string file) override {
        if (file == mockTemperatureHwmonTempFile0) {
            return temp2MaxExists;
        }
        return false;
    }
};

struct MockTemperatureFsAccess : public L0::Sysman::FsAccessInterface {
    MockTemperatureFsAccess() = default;
};

struct MockTemperatureProcfsAccess : public L0::Sysman::ProcFsAccessInterface {
    MockTemperatureProcfsAccess() = default;
    ~MockTemperatureProcfsAccess() override = default;
};

class PublicLinuxTemperatureImp : public L0::Sysman::LinuxTemperatureImp {
  public:
    PublicLinuxTemperatureImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxTemperatureImp(pOsSysman, onSubdevice, subdeviceId) {}
};

} // namespace ult
} // namespace Sysman
} // namespace L0
